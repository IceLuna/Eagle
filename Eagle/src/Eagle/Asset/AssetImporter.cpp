#include "egpch.h"
#include "AssetImporter.h"
#include "AssetManager.h"

#include "Eagle/Core/DataBuffer.h"
#include "Eagle/Core/Serializer.h"
#include "Eagle/Classes/StaticMesh.h"
#include "Eagle/Classes/SkeletalMesh.h"
#include "Eagle/Classes/Animation.h"
#include "Eagle/Utils/Compressor.h"
#include "Eagle/Utils/PlatformUtils.h"
#include "Eagle/Utils/YamlUtils.h"
#include "Eagle/Renderer/TextureCompressor.h"
#include "Eagle/Renderer/VidWrappers/Texture.h"
#include "Eagle/Core/Project.h"

#include <stb_image.h>
#include <stb_image_write.h>

namespace Eagle
{
	namespace Utils
	{
		static Path GetUniqueFilepath(const Path& saveTo, const std::string& filename, uint32_t& i)
		{
			Path outputFilename = saveTo / (filename + Asset::GetExtension());
			while (std::filesystem::exists(outputFilename))
			{
				std::string uniqueFilename = filename + '_' + std::to_string(i);
				outputFilename = saveTo / (uniqueFilename + Asset::GetExtension());
				++i;
			}

			return outputFilename;
		}

		template<typename MeshType> // Either `StaticMesh` or `SkeletalMesh`
		static void SerializeMesh(const Ref<MeshType>& mesh, const Path& pathToRaw, const Path& outputFilename)
		{
			constexpr bool bStatic = std::is_same<MeshType, StaticMesh>::value;
			constexpr bool bSkeletal = std::is_same<MeshType, SkeletalMesh>::value;
			if constexpr (!(bStatic || bSkeletal))
				static_assert(false, "Invalid type. Should be `StaticMesh` or `SkeletalMesh`");

			constexpr AssetType type = bStatic ? AssetType::StaticMesh : AssetType::SkeletalMesh;
			DataBuffer verticesBuffer = { (void*)mesh->GetVerticesData(), mesh->GetVerticesCount() * (bStatic ? sizeof(Vertex) : sizeof(SkeletalVertex)) };
			DataBuffer indicesBuffer{ (void*)mesh->GetIndicesData(), mesh->GetIndicesCount() * sizeof(Index) };
			const size_t origVerticesDataSize = verticesBuffer.Size; // Required for decompression
			const size_t origIndicesDataSize = indicesBuffer.Size; // Required for decompression

			ScopedDataBuffer compressedVertices(Compressor::Compress(verticesBuffer));
			ScopedDataBuffer compressedIndices(Compressor::Compress(indicesBuffer));

			YAML::Emitter out;
			out << YAML::BeginMap;
			out << YAML::Key << "Version" << YAML::Value << EG_VERSION;
			out << YAML::Key << "Type" << YAML::Value << Utils::GetEnumName(type);
			out << YAML::Key << "GUID" << YAML::Value << GUID{};
			out << YAML::Key << "RawPath" << YAML::Value << pathToRaw.string();

			if constexpr (bSkeletal)
			{
				const auto& skeletal = mesh->GetSkeletal();
				out << YAML::Key << "InverseTransform" << YAML::Value << skeletal.InverseTransform;

				out << YAML::Key << "Skeletal" << YAML::Value;
				Serializer::EmitBoneNode(out, skeletal.RootBone);

				out << YAML::Key << "BoneInfoMap";
				{
					out << YAML::Value << YAML::BeginSeq;

					const auto& bones = skeletal.BoneInfoMap;
					for (auto& [name, data] : bones)
					{
						out << YAML::BeginMap;
						out << YAML::Key << "Name" << YAML::Value << name;
						out << YAML::Key << "Matrix" << YAML::Value << data.Offset;
						out << YAML::Key << "ID" << YAML::Value << data.BoneID;
						out << YAML::EndMap;
					}

					out << YAML::EndSeq;
				}
			}

			out << YAML::Key << "Data" << YAML::Value << YAML::BeginMap;
			out << YAML::Key << "SizeVertices" << YAML::Value << origVerticesDataSize;
			out << YAML::Key << "SizeIndices" << YAML::Value << origIndicesDataSize;
			out << YAML::Key << "Vertices" << YAML::Value << YAML::Binary((uint8_t*)compressedVertices.Data(), compressedVertices.Size());
			out << YAML::Key << "Indices" << YAML::Value << YAML::Binary((uint8_t*)compressedIndices.Data(), compressedIndices.Size());

			out << YAML::EndMap;
			out << YAML::EndMap;

			std::ofstream fout(outputFilename);
			fout << out.c_str();
			fout.close();
		};
	
		static void SerializeAnimation(const SkeletalMeshAnimation& animation, const Ref<AssetSkeletalMesh>& skeletal, const Path& pathToRaw, const Path& outputFilename, uint32_t animIndex)
		{
			YAML::Emitter out;
			out << YAML::BeginMap;
			out << YAML::Key << "Version" << YAML::Value << EG_VERSION;
			out << YAML::Key << "Type" << YAML::Value << Utils::GetEnumName(AssetType::Animation);
			out << YAML::Key << "GUID" << YAML::Value << GUID{};
			out << YAML::Key << "RawPath" << YAML::Value << pathToRaw.string();
			out << YAML::Key << "Index" << YAML::Value << animIndex;
			out << YAML::Key << "Skeletal" << YAML::Value << skeletal->GetGUID();
			Serializer::SerializeAnimation(out, animation);
			out << YAML::EndMap;

			std::ofstream fout(outputFilename);
			fout << out.c_str();
			fout.close();
		}
	}

	bool AssetImporter::Import(const Path& pathToRaw, const Path& saveTo, AssetType type, const AssetImportSettings& settings)
	{
		if (!std::filesystem::exists(pathToRaw) || std::filesystem::is_directory(pathToRaw))
		{
			EG_CORE_ERROR("Import failed. File doesn't exist: {}", pathToRaw.u8string());
			return false;
		}

		Path outputFilename = saveTo / (pathToRaw.stem().u8string() + Asset::GetExtension());
		if (std::filesystem::exists(outputFilename))
		{
			EG_CORE_ERROR("Import failed. Asset already exists: {}", outputFilename.u8string());
			return false;
		}

		bool bSuccess = false;
		switch (type)
		{
			case AssetType::Texture2D:
				bSuccess = ImportTexture2D(pathToRaw, outputFilename, settings);
				break;
			case AssetType::TextureCube:
				bSuccess = ImportTextureCube(pathToRaw, outputFilename, settings);
				break;
			case AssetType::StaticMesh:
				bSuccess = ImportStaticMesh(pathToRaw, saveTo, outputFilename, settings);
				break;
			case AssetType::SkeletalMesh:
				bSuccess = ImportSkeletalMesh(pathToRaw, saveTo, outputFilename, settings);
				break;
			case AssetType::Audio:
				bSuccess = ImportAudio(pathToRaw, outputFilename, settings);
				break;
			case AssetType::Font:
				bSuccess = ImportFont(pathToRaw, outputFilename, settings);
				break;
			case AssetType::Animation:
				bSuccess = ImportAnimation(pathToRaw, saveTo, outputFilename, settings.AnimationSettings.Skeletal);
				break;
			default:
				EG_CORE_ERROR("Import failed. Unknown asset type: {} - {}", pathToRaw.u8string(), Utils::GetEnumName(type));
				return false;
		}

		if (bSuccess)
		{
			Ref<Asset> asset = Asset::Create(outputFilename);
			AssetManager::Register(asset);
			const AssetType assetType = asset->GetAssetType();
			const bool bSkeletal = assetType == AssetType::SkeletalMesh;
			const bool bStatic = assetType == AssetType::StaticMesh;

			// Import animations if required
			if (settings.MeshSettings.bImportAnimations && bSkeletal)
			{
				Ref<AssetSkeletalMesh> skeletal = Cast<AssetSkeletalMesh>(asset);
				std::vector<SkeletalMeshAnimation> animations = Utils::ImportAnimations(pathToRaw, skeletal->GetMesh());

				std::string filename = outputFilename.stem().u8string() + "_Anim";
				uint32_t filenameIndex = 0;
				uint32_t animIndex = 0;
				for (const auto& anim : animations)
				{
					Path output = Utils::GetUniqueFilepath(saveTo, filename, filenameIndex);
					Utils::SerializeAnimation(anim, skeletal, pathToRaw, output, animIndex++);
					AssetManager::Register(Asset::Create(output));
				}
			}
			
			if (settings.MeshSettings.bImportMaterials && (bSkeletal || bStatic))
				Utils::ImportMaterials(pathToRaw, saveTo);
		}

		return bSuccess;
	}

	bool AssetImporter::CreateFromTexture2D(const Ref<Texture2D>& texture, const Path& outputFilename)
	{
		if (std::filesystem::exists(outputFilename))
		{
			EG_CORE_ERROR("Import failed. Asset already exists: {}", outputFilename.u8string());
			return false;
		}

		// TODO: Test
		const auto& data = texture->GetData();
		const int width = (int)texture->GetWidth();
		const int height = (int)texture->GetHeight();
		const int comp = GetImageFormatChannels(texture->GetFormat());

		const std::string& name = texture->GetImage()->GetDebugName();
		const Path cache = Project::GetCachePath() / name;
		const bool bSuccess = stbi_write_png(cache.string().c_str(), width, height, comp, data.Data(), width * comp);
		if (bSuccess)
			return AssetImporter::ImportTexture2D(cache, outputFilename, {});
		return false;
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

	Path AssetImporter::CreatePhysicsMaterial(const Path& saveTo, const std::string& filename)
	{
		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Version" << YAML::Value << EG_VERSION;
		out << YAML::Key << "Type" << YAML::Value << Utils::GetEnumName(AssetType::PhysicsMaterial);
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

	Path AssetImporter::CreateSoundGroup(const Path& saveTo, const std::string& filename)
	{
		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Version" << YAML::Value << EG_VERSION;
		out << YAML::Key << "Type" << YAML::Value << Utils::GetEnumName(AssetType::SoundGroup);
		out << YAML::Key << "GUID" << YAML::Value << GUID{};
		out << YAML::Key << "Volume" << YAML::Value << 1.f;
		out << YAML::Key << "Pitch" << YAML::Value << 1.f;
		out << YAML::Key << "IsPaused" << YAML::Value << false;
		out << YAML::Key << "IsMuted" << YAML::Value << false;
		out << YAML::EndMap;

		uint32_t i = 0;
		const Path outputFilename = Utils::GetUniqueFilepath(saveTo, filename, i);
		std::ofstream fout(outputFilename);
		fout << out.c_str();
		fout.close();

		AssetManager::Register(Asset::Create(outputFilename));

		return outputFilename;
	}

	Path AssetImporter::CreateEntity(const Path& saveTo, const std::string& filename)
	{
		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Version" << YAML::Value << EG_VERSION;
		out << YAML::Key << "Type" << YAML::Value << Utils::GetEnumName(AssetType::Entity);
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

	Path AssetImporter::CreateScene(const Path& saveTo, const std::string& filename)
	{
		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Version" << YAML::Value << EG_VERSION;
		out << YAML::Key << "Type" << YAML::Value << Utils::GetEnumName(AssetType::Scene);
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

		static const std::unordered_map<std::string, AssetType> s_SupportedFileFormats =
		{
			{ ".png",   AssetType::Texture2D },
			{ ".jpg",   AssetType::Texture2D },
			{ ".tga",   AssetType::Texture2D },
			{ ".hdr",   AssetType::TextureCube },
			{ ".eagle", AssetType::Scene },
			{ ".fbx",   AssetType::StaticMesh },
			{ ".blend", AssetType::StaticMesh },
			{ ".3ds",   AssetType::StaticMesh },
			{ ".obj",   AssetType::StaticMesh },
			{ ".smd",   AssetType::StaticMesh },
			{ ".vta",   AssetType::StaticMesh },
			{ ".stl",   AssetType::StaticMesh },
			{ ".wav",   AssetType::Audio },
			{ ".ogg",   AssetType::Audio },
			{ ".wma",   AssetType::Audio },
			{ ".ttf",   AssetType::Font },
			{ ".otf",   AssetType::Font },
		};

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

		const void* compressedTextureHandle = nullptr;
		bool bCompressTexture = textureSettings.bCompress;

		const void* ktxData = nullptr;
		size_t ktxDataSize = 0ull;

		if (bCompressTexture)
		{
			constexpr int desiredChannels = 4;
			int width, height, channels;

			void* stbiImageData = stbi_load_from_memory((uint8_t*)buffer.Data(), (int)buffer.Size(), &width, &height, &channels, desiredChannels);
			if (!stbiImageData)
			{
				EG_CORE_ERROR("Import failed. stbi_load failed: {}", pathToRaw);
				return false;
			}

			const size_t textureMemSize = desiredChannels * width * height;
			compressedTextureHandle = TextureCompressor::Compress(DataBuffer(stbiImageData, textureMemSize), glm::uvec2(width, height),
				textureSettings.MipsCount, textureSettings.bNormalMap, textureSettings.bNeedAlpha);

			if (compressedTextureHandle)
			{
				DataBuffer compressedTextureData = TextureCompressor::GetKTX2Data(compressedTextureHandle);
				ktxData = compressedTextureData.Data;
				ktxDataSize = compressedTextureData.Size;
			}
			else
				bCompressTexture = false; // Failed to compress

			stbi_image_free(stbiImageData);
		}

		int width, height, channels;
		stbi_info_from_memory((uint8_t*)buffer.Data(), (int)buffer.Size(), &width, &height, &channels);

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
		out << YAML::Key << "Width" << YAML::Value << width;
		out << YAML::Key << "Height" << YAML::Value << height;
		out << YAML::Key << "Format" << YAML::Value << Utils::GetEnumName(textureSettings.ImportFormat);
		out << YAML::Key << "IsCompressed" << YAML::Value << bCompressTexture;
		out << YAML::Key << "IsNormalMap" << YAML::Value << textureSettings.bNormalMap;
		out << YAML::Key << "NeedAlpha" << YAML::Value << textureSettings.bNeedAlpha;

		out << YAML::Key << "Data" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "Size" << YAML::Value << origDataSize;
		out << YAML::Key << "Data" << YAML::Value << YAML::Binary((uint8_t*)compressed.Data(), compressed.Size());
		if (bCompressTexture)
			out << YAML::Key << "KTXData" << YAML::Value << YAML::Binary((uint8_t*)ktxData, ktxDataSize);
		out << YAML::EndMap;

		out << YAML::EndMap;

		std::ofstream fout(outputFilename);
		fout << out.c_str();
		fout.close();

		if (compressedTextureHandle)
			TextureCompressor::Destroy(compressedTextureHandle);

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
		out << YAML::Key << "Size" << YAML::Value << origDataSize;
		out << YAML::Key << "Data" << YAML::Value << YAML::Binary((uint8_t*)compressed.Data(), compressed.Size());
		out << YAML::EndMap;
		
		out << YAML::EndMap;

		std::ofstream fout(outputFilename);
		fout << out.c_str();

		return true;
	}
	
	bool AssetImporter::ImportStaticMesh(const Path& pathToRaw, const Path& saveTo, const Path& outputFilename, const AssetImportSettings& settings)
	{
		Ref<StaticMesh> importedMesh = Utils::ImportStaticMesh(pathToRaw);
		if (!importedMesh)
		{
			EG_CORE_ERROR("Failed to import a mesh. No meshes in file '{0}'", pathToRaw.u8string());
			return false;
		}

		Utils::SerializeMesh(StaticMesh::Create(importedMesh), pathToRaw, outputFilename);

		return true;
	}

	bool AssetImporter::ImportSkeletalMesh(const Path& pathToRaw, const Path& saveTo, const Path& outputFilename, const AssetImportSettings& settings)
	{
		Ref<SkeletalMesh> importedMesh = Utils::ImportSkeletalMesh(pathToRaw);
		if (!importedMesh)
		{
			EG_CORE_ERROR("Failed to import a mesh. No meshes in file '{0}'", pathToRaw.u8string());
			return false;
		}

		Utils::SerializeMesh(importedMesh, pathToRaw, outputFilename);

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
		out << YAML::Key << "Volume" << YAML::Value << 1.f;
		out << YAML::Key << "RawPath" << YAML::Value << pathToRaw.string();

		out << YAML::Key << "Data" << YAML::Value << YAML::BeginMap;
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
		ScopedDataBuffer buffer(FileSystem::Read(pathToRaw));
		const size_t origDataSize = buffer.Size(); // Required for decompression
		ScopedDataBuffer compressed(Compressor::Compress(DataBuffer{ buffer.Data(), buffer.Size() }));

		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Version" << YAML::Value << EG_VERSION;
		out << YAML::Key << "Type" << YAML::Value << Utils::GetEnumName(AssetType::Font);
		out << YAML::Key << "GUID" << YAML::Value << GUID{};
		out << YAML::Key << "RawPath" << YAML::Value << pathToRaw.string();

		out << YAML::Key << "Data" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "Size" << YAML::Value << origDataSize;
		out << YAML::Key << "Data" << YAML::Value << YAML::Binary((uint8_t*)compressed.Data(), compressed.Size());
		out << YAML::EndMap;

		out << YAML::EndMap;

		std::ofstream fout(outputFilename);
		fout << out.c_str();
		fout.close();

		return true;
	}

	bool AssetImporter::ImportAnimation(const Path& pathToRaw, const Path& saveTo, const Path& outputFilename, const Ref<AssetSkeletalMesh>& skeletal)
	{
		std::vector<SkeletalMeshAnimation> animations = Utils::ImportAnimations(pathToRaw, skeletal->GetMesh());
		if (animations.empty())
		{
			EG_CORE_ERROR("Failed to import an animation. No animations in file '{0}'", pathToRaw.u8string());
			return false;
		}

		std::string filename = outputFilename.stem().u8string();
		uint32_t filenameIndex = 0;
		uint32_t animIndex = 0;
		for (const auto& anim : animations)
		{
			Path output = Utils::GetUniqueFilepath(saveTo, filename, filenameIndex);
			Utils::SerializeAnimation(anim, skeletal, pathToRaw, output, animIndex++);
		}

		return true;
	}
}
