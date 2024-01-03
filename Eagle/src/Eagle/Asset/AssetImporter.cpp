#include "egpch.h"
#include "AssetImporter.h"
#include "AssetManager.h"

#include "Eagle/Core/DataBuffer.h"
#include "Eagle/Classes/StaticMesh.h"
#include "Eagle/Utils/Compressor.h"
#include "Eagle/Utils/PlatformUtils.h"
#include "Eagle/Utils/YamlUtils.h"

namespace Eagle
{
	namespace Utils
	{
		static Path GetUniqueFilepath(const Path& saveTo, const std::string& filename, uint32_t& i)
		{
			Path outputFilename;
			do
			{
				std::string uniqueFilename = filename + '_' + std::to_string(i);
				outputFilename = saveTo / (uniqueFilename + Asset::GetExtension());
				++i;
			} while (std::filesystem::exists(outputFilename));
			return outputFilename;
		}
	}

	static const std::unordered_map<std::string, AssetType> s_SupportedFileFormats =
	{
		{ ".png",   AssetType::Texture2D },
		{ ".jpg",   AssetType::Texture2D },
		{ ".tga",   AssetType::Texture2D },
		{ ".hdr",   AssetType::TextureCube },
		{ ".eagle", AssetType::Scene },
		{ ".fbx",   AssetType::Mesh },
		{ ".blend", AssetType::Mesh },
		{ ".3ds",   AssetType::Mesh },
		{ ".obj",   AssetType::Mesh },
		{ ".smd",   AssetType::Mesh },
		{ ".vta",   AssetType::Mesh },
		{ ".stl",   AssetType::Mesh },
		{ ".wav",   AssetType::Audio },
		{ ".ogg",   AssetType::Audio },
		{ ".wma",   AssetType::Audio },
		{ ".ttf",   AssetType::Font },
		{ ".otf",   AssetType::Font },
	};

	bool AssetImporter::Import(const Path& pathToRaw, const Path& saveTo, const AssetImportSettings& settings)
	{
		if (!std::filesystem::exists(pathToRaw) || std::filesystem::is_directory(pathToRaw))
		{
			EG_CORE_ERROR("Import failed. File doesn't exist: {}", pathToRaw);
			return false;
		}

		Path outputFilename = saveTo / (pathToRaw.stem().u8string() + Asset::GetExtension());
		if (std::filesystem::exists(outputFilename))
		{
			EG_CORE_ERROR("Import failed. Asset already exists: {}", outputFilename);
			return false;
		}

		bool bSuccess = false;
		AssetType type = GetAssetTypeByExtension(pathToRaw);
		switch (type)
		{
			case AssetType::Texture2D:
				bSuccess = ImportTexture2D(pathToRaw, outputFilename, settings);
				break;
			case AssetType::TextureCube:
				bSuccess = ImportTextureCube(pathToRaw, outputFilename, settings);
				break;
			case AssetType::Mesh:
				bSuccess = ImportMesh(pathToRaw, saveTo, outputFilename, settings);
				break;
			case AssetType::Audio:
				bSuccess = ImportAudio(pathToRaw, outputFilename, settings);
				break;
			case AssetType::Font:
				bSuccess = ImportFont(pathToRaw, outputFilename, settings);
				break;
			default:
				EG_CORE_ERROR("Import failed. Unknown asset type: {} - {}", pathToRaw, Utils::GetEnumName(type));
				return false;
		}

		if (bSuccess)
			AssetManager::Register(Asset::Create(outputFilename));

		return bSuccess;
	}

	Path AssetImporter::CreateMaterial(const Path& saveTo, const std::string& filename)
	{
		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Version" << YAML::Value << EG_VERSION;
		out << YAML::Key << "Type" << YAML::Value << Utils::GetEnumName(AssetType::Material);
		out << YAML::Key << "GUID" << YAML::Value << GUID{};
		out << YAML::EndMap;

		uint32_t i = 0;
		const Path outputFilename = Utils::GetUniqueFilepath(saveTo, filename, i);
		std::ofstream fout(outputFilename);
		fout << out.c_str();
		fout.close();

		AssetManager::Register(Asset::Create(outputFilename));

		return outputFilename;
	}

	AssetType AssetImporter::GetAssetTypeByExtension(const Path& filepath)
	{
		if (!filepath.has_extension())
			return AssetType::None;

		static const std::locale& loc = std::locale("RU_ru");
		std::string extension = filepath.extension().u8string();

		for (char& c : extension)
			c = std::tolower(c, loc);

		auto it = s_SupportedFileFormats.find(extension);
		if (it != s_SupportedFileFormats.end())
			return it->second;

		return AssetType::None;
	}
	
	bool AssetImporter::ImportTexture2D(const Path& pathToRaw, const Path& outputFilename, const AssetImportSettings& settings)
	{
		const auto& textureSettings = settings.Texture2DSettings;
		ScopedDataBuffer buffer(FileSystem::Read(pathToRaw));
		const size_t origDataSize = buffer.Size(); // Required for decompression
		ScopedDataBuffer compressed(Compressor::Compress(DataBuffer{ buffer.Data(), buffer.Size() }));

		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Version" << YAML::Value << EG_VERSION;
		out << YAML::Key << "Type" << YAML::Value << Utils::GetEnumName(AssetType::Texture2D);
		out << YAML::Key << "GUID" << YAML::Value << GUID{};
		out << YAML::Key << "RawPath" << YAML::Value << pathToRaw.string();
		out << YAML::Key << "FilterMode" << YAML::Value << Utils::GetEnumName(textureSettings.FilterMode);
		out << YAML::Key << "AddressMode" << YAML::Value << Utils::GetEnumName(textureSettings.AddressMode);
		out << YAML::Key << "Anisotropy" << YAML::Value << textureSettings.Anisotropy;
		out << YAML::Key << "MipsCount" << YAML::Value << textureSettings.MipsCount;
		out << YAML::Key << "Format" << YAML::Value << Utils::GetEnumName(textureSettings.ImportFormat);

		out << YAML::Key << "Data" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "Compressed" << YAML::Value << true;
		out << YAML::Key << "Size" << YAML::Value << origDataSize;
		out << YAML::Key << "Data" << YAML::Value << YAML::Binary((uint8_t*)compressed.Data(), compressed.Size());
		out << YAML::EndMap;

		out << YAML::EndMap;

		std::ofstream fout(outputFilename);
		fout << out.c_str();
		fout.close();

		return true;
	}
	
	bool AssetImporter::ImportTextureCube(const Path& pathToRaw, const Path& outputFilename, const AssetImportSettings& settings)
	{
		ScopedDataBuffer buffer(FileSystem::Read(pathToRaw));
		const size_t origDataSize = buffer.Size(); // Required for decompression
		ScopedDataBuffer compressed( Compressor::Compress(DataBuffer{ buffer.Data(), buffer.Size() }) );

		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Version" << YAML::Value << EG_VERSION;
		out << YAML::Key << "Type" << YAML::Value << Utils::GetEnumName(AssetType::TextureCube);
		out << YAML::Key << "GUID" << YAML::Value << GUID{};
		out << YAML::Key << "RawPath" << YAML::Value << pathToRaw.string();
		out << YAML::Key << "Format" << YAML::Value << Utils::GetEnumName(settings.TextureCubeSettings.ImportFormat);
		out << YAML::Key << "LayerSize" << YAML::Value << settings.TextureCubeSettings.LayerSize;

		out << YAML::Key << "Data" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "Compressed" << YAML::Value << true;
		out << YAML::Key << "Size" << YAML::Value << origDataSize;
		out << YAML::Key << "Data" << YAML::Value << YAML::Binary((uint8_t*)compressed.Data(), compressed.Size());
		out << YAML::EndMap;
		
		out << YAML::EndMap;

		std::ofstream fout(outputFilename);
		fout << out.c_str();

		return true;
	}
	
	bool AssetImporter::ImportMesh(const Path& pathToRaw, const Path& saveTo, const Path& outputFilename, const AssetImportSettings& settings)
	{
		std::vector<Ref<StaticMesh>> importedMeshes = Utils::ImportMeshes(pathToRaw);
		size_t meshesCount = importedMeshes.size();

		if (meshesCount == 0)
		{
			EG_CORE_ERROR("Failed to load a mesh. No meshes in file '{0}'", pathToRaw);
			return false;
		}

		const bool bImportAsSingleMesh = settings.MeshSettings.bImportAsSingleMesh;
		const std::string fileStem = pathToRaw.stem().u8string();
		std::vector<Ref<StaticMesh>> meshes;
		std::vector<Path> outputFilenames;
		meshes.reserve(bImportAsSingleMesh ? 1u : meshesCount);
		outputFilenames.reserve(bImportAsSingleMesh ? 1u : meshesCount);

		if (meshesCount > 1)
		{
			if (bImportAsSingleMesh)
			{
				std::vector<Vertex> vertices;
				std::vector<Index> indeces;
				size_t verticesTotalSize = 0;
				size_t indecesTotalSize = 0;

				for (const auto& mesh : importedMeshes)
					verticesTotalSize += mesh->GetVerticesCount();
				for (const auto& mesh : importedMeshes)
					indecesTotalSize += mesh->GetIndicesCount();

				vertices.reserve(verticesTotalSize);
				indeces.reserve(indecesTotalSize);

				for (const auto& mesh : importedMeshes)
				{
					const auto& meshVertices = mesh->GetVertices();
					const auto& meshIndeces = mesh->GetIndices();

					const size_t vSizeBeforeCopy = vertices.size();
					vertices.insert(vertices.end(), meshVertices.begin(), meshVertices.end());

					const size_t iSizeBeforeCopy = indeces.size();
					indeces.insert(indeces.end(), meshIndeces.begin(), meshIndeces.end());
					const size_t iSizeAfterCopy = indeces.size();

					for (size_t i = iSizeBeforeCopy; i < iSizeAfterCopy; ++i)
						indeces[i] += uint32_t(vSizeBeforeCopy);
				}

				meshes.emplace_back(StaticMesh::Create(vertices, indeces));
				outputFilenames.push_back(outputFilename);
			}
			else
			{
				uint32_t meshIndex = 0u;
				Ref<StaticMesh> firstSM = StaticMesh::Create(importedMeshes[0]->GetVertices(), importedMeshes[0]->GetIndices());
				meshes.emplace_back(std::move(firstSM));
				outputFilenames.emplace_back(Utils::GetUniqueFilepath(saveTo, fileStem, meshIndex));

				for (uint32_t i = 1; i < uint32_t(meshesCount); ++i)
				{
					Ref<StaticMesh> sm = StaticMesh::Create(importedMeshes[i]->GetVertices(), importedMeshes[i]->GetIndices());
					meshes.emplace_back(std::move(sm));

					Path outputFilename = Utils::GetUniqueFilepath(saveTo, fileStem, meshIndex);
					outputFilenames.emplace_back(std::move(outputFilename));
				}
			}
		}
		else
		{
			meshes.emplace_back(StaticMesh::Create(importedMeshes[0]));
			outputFilenames.emplace_back(outputFilename);
		}

		for (size_t i = 0; i < meshes.size(); ++i)
		{
			const auto& mesh = meshes[i];
			DataBuffer verticesBuffer{ (void*)mesh->GetVerticesData(), mesh->GetVerticesCount() * sizeof(Vertex) };
			DataBuffer indicesBuffer{ (void*)mesh->GetIndicesData(), mesh->GetIndicesCount() * sizeof(Vertex) };
			const size_t origVerticesDataSize = verticesBuffer.Size; // Required for decompression
			const size_t origIndicesDataSize = indicesBuffer.Size; // Required for decompression

			ScopedDataBuffer compressedVertices(Compressor::Compress(verticesBuffer));
			ScopedDataBuffer compressedIndices(Compressor::Compress(indicesBuffer));

			YAML::Emitter out;
			out << YAML::BeginMap;
			out << YAML::Key << "Version" << YAML::Value << EG_VERSION;
			out << YAML::Key << "Type" << YAML::Value << Utils::GetEnumName(AssetType::Mesh);
			out << YAML::Key << "GUID" << YAML::Value << GUID{};
			out << YAML::Key << "RawPath" << YAML::Value << pathToRaw.string();
			out << YAML::Key << "Index" << YAML::Value << i; // Index of a mesh if a mesh-file (fbx) contains multiple meshes

			out << YAML::Key << "Data" << YAML::Value << YAML::BeginMap;
			out << YAML::Key << "Compressed" << YAML::Value << true;
			out << YAML::Key << "SizeVertices" << YAML::Value << origVerticesDataSize;
			out << YAML::Key << "SizeIndices" << YAML::Value << origIndicesDataSize;
			out << YAML::Key << "Vertices" << YAML::Value << YAML::Binary((uint8_t*)compressedVertices.Data(), compressedVertices.Size());
			out << YAML::Key << "Indices" << YAML::Value << YAML::Binary((uint8_t*)compressedIndices.Data(), compressedIndices.Size());
			out << YAML::EndMap;

			out << YAML::EndMap;

			std::ofstream fout(outputFilenames[i]);
			fout << out.c_str();
			fout.close();
		}

		return true;
	}
	
	bool AssetImporter::ImportAudio(const Path& pathToRaw, const Path& outputFilename, const AssetImportSettings& settings)
	{
		ScopedDataBuffer buffer(FileSystem::Read(pathToRaw));
		const size_t origDataSize = buffer.Size(); // Required for decompression
		ScopedDataBuffer compressed(Compressor::Compress(DataBuffer{ buffer.Data(), buffer.Size() }));

		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Version" << YAML::Value << EG_VERSION;
		out << YAML::Key << "Type" << YAML::Value << Utils::GetEnumName(AssetType::Audio);
		out << YAML::Key << "GUID" << YAML::Value << GUID{};
		out << YAML::Key << "RawPath" << YAML::Value << pathToRaw.string();

		out << YAML::Key << "Data" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "Compressed" << YAML::Value << true;
		out << YAML::Key << "Size" << YAML::Value << origDataSize;
		out << YAML::Key << "Data" << YAML::Value << YAML::Binary((uint8_t*)compressed.Data(), compressed.Size());
		out << YAML::EndMap;

		out << YAML::EndMap;

		std::ofstream fout(outputFilename);
		fout << out.c_str();
		fout.close();

		return true;
	}
	
	bool AssetImporter::ImportFont(const Path& pathToRaw, const Path& outputFilename, const AssetImportSettings& settings)
	{
		return false;
	}
}
