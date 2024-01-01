#include "egpch.h"
#include "AssetImporter.h"
#include "AssetManager.h"
#include "Eagle/Core/DataBuffer.h"
#include "Eagle/Utils/Compressor.h"
#include "Eagle/Utils/PlatformUtils.h"
#include "Eagle/Utils/YamlUtils.h"

namespace Eagle
{
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
		{ ".wav",   AssetType::Sound },
		{ ".ogg",   AssetType::Sound },
		{ ".wma",   AssetType::Sound },
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
				bSuccess = ImportMesh(pathToRaw, outputFilename, settings);
				break;
			case AssetType::Sound:
				bSuccess = ImportSound(pathToRaw, outputFilename, settings);
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
		out << YAML::Key << "Type" << YAML::Value << Utils::GetEnumName(AssetType::Material);
		out << YAML::Key << "GUID" << YAML::Value << GUID{};
		out << YAML::EndMap;

		Path outputFilename = saveTo / (filename + Asset::GetExtension());
		if (std::filesystem::exists(outputFilename))
		{
			uint32_t i = 1u;
			while (true)
			{
				std::string uniqueFilename = filename + '_' + std::to_string(i);
				outputFilename = saveTo / (uniqueFilename + Asset::GetExtension());
				if (std::filesystem::exists(outputFilename) == false)
					break;
				++i;
			}
		}
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
	
	bool AssetImporter::ImportMesh(const Path& pathToRaw, const Path& outputFilename, const AssetImportSettings& settings)
	{
		return false;
	}
	
	bool AssetImporter::ImportSound(const Path& pathToRaw, const Path& outputFilename, const AssetImportSettings& settings)
	{
		return false;
	}
	
	bool AssetImporter::ImportFont(const Path& pathToRaw, const Path& outputFilename, const AssetImportSettings& settings)
	{
		return false;
	}
}
