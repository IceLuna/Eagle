#include "egpch.h"
#include "AssetImporter.h"
#include "AssetManager.h"

#include "Eagle/Core/DataBuffer.h"
#include "Eagle/Core/Serializer.h"
#include "Eagle/Core/SceneSerializer.h"
#include "Eagle/Classes/Font.h"
#include "Eagle/Classes/StaticMesh.h"
#include "Eagle/Classes/SkeletalMesh.h"
#include "Eagle/Animation/Animation.h"
#include "Eagle/Utils/Compressor.h"
#include "Eagle/Utils/PlatformUtils.h"
#include "Eagle/Utils/YamlUtils.h"
#include "Eagle/Utils/AssimpImporter.h"
#include "Eagle/Renderer/TextureCompressor.h"
#include "Eagle/Renderer/VidWrappers/Texture.h"
#include "Eagle/Core/Project.h"

#include <stb_image.h>

namespace Eagle
{
	// Returns an eagle asset file data
	static ScopedDataBuffer CreateTexture2DAssetFromMemory(DataBuffer buffer, const Path& outputFilename, const AssetImportTexture2DSettings& settings)
	{
		TextureCompressor::Result compressedData{};
		TextureCompressor::Quality compression = settings.Compression;

		if (compression != TextureCompressor::Quality::Disabled)
		{
			const uint32_t targetNumChannels = AssetTextureFormatToChannels(settings.ImportFormat, compression);
			compressedData = TextureCompressor::Compress(buffer, targetNumChannels, settings.MipsCount, compression, settings.bNormalMap);
			if (!compressedData)
				compression = TextureCompressor::Quality::Disabled; // Failed to compress
		}

		int width, height, channels;
		stbi_info_from_memory((uint8_t*)buffer.Data, (int)buffer.Size, &width, &height, &channels);

		const Path pathToRaw = {}; // Empty since it doesn't come from a file
		auto data = Serializer::SerializeAssetTexture2DFromData(buffer, compressedData.DataPerMip, compressedData.Format, GUID{}, pathToRaw,
			settings.FilterMode, settings.AddressMode, settings.Anisotropy, settings.MipsCount,
			width, height, settings.ImportFormat, compression, settings.bNormalMap);
		FileSystem::Write(outputFilename, data);

		return data;
	}

	bool AssetImporter::Import(const Path& pathToRaw, const Path& saveTo, AssetType type, const AssetImportSettings& settings)
	{
		if (!std::filesystem::exists(pathToRaw) || std::filesystem::is_directory(pathToRaw))
		{
			EG_CORE_ERROR("Import failed. File doesn't exist: {}", pathToRaw);
			spdlog::info("{}", pathToRaw);
			return false;
		}

		Path outputFilename = saveTo / Utils::AsPath(Utils::AsString(pathToRaw.stem()) + Asset::GetExtension());
		if (std::filesystem::exists(outputFilename))
			outputFilename = Utils::GetUniqueAssetFilepath(outputFilename.parent_path(), Utils::AsString(outputFilename.stem()));

		// Most importers produce exactly one asset. Static/Skeletal Mesh importers can produce several
		// when `settings.MeshSettings.bCombineMeshes` is `false` and the source file has multiple meshes.
		std::vector<Path> outputFilenames;
		switch (type)
		{
			case AssetType::Texture2D:
				if (ImportTexture2D(pathToRaw, outputFilename, settings))
					outputFilenames.push_back(outputFilename);
				break;
			case AssetType::TextureCube:
				if (ImportTextureCube(pathToRaw, outputFilename, settings))
					outputFilenames.push_back(outputFilename);
				break;
			case AssetType::StaticMesh:
				outputFilenames = ImportStaticMesh(pathToRaw, saveTo, outputFilename, settings);
				break;
			case AssetType::SkeletalMesh:
				outputFilenames = ImportSkeletalMesh(pathToRaw, saveTo, outputFilename, settings);
				break;
			case AssetType::Audio:
				if (ImportAudio(pathToRaw, outputFilename, settings))
					outputFilenames.push_back(outputFilename);
				break;
			case AssetType::Font:
				if (ImportFont(pathToRaw, outputFilename, settings))
					outputFilenames.push_back(outputFilename);
				break;
			case AssetType::Animation:
				return ImportAnimation(pathToRaw, saveTo, outputFilename, settings.AnimationSettings);
			default:
				EG_CORE_ERROR("Import failed. Unknown asset type: {} - {}", pathToRaw, Utils::GetEnumName(type));
				return false;
		}

		if (outputFilenames.empty())
			return false;

		// Animations apply to the whole armature, so even when multiple Skeletal Mesh assets were produced
		// (bCombineMeshes is false), only import the animation set once, against the first resulting skeletal asset.
		bool bAnimationsImported = false;

		for (const auto& filename : outputFilenames)
		{
			Ref<Asset> asset = Asset::Create(filename);
			AssetManager::Register(asset);
			const AssetType assetType = asset->GetAssetType();
			const bool bSkeletal = assetType == AssetType::SkeletalMesh;

			if (settings.MeshSettings.bImportAnimations && bSkeletal && !bAnimationsImported)
			{
				bAnimationsImported = true;

				Ref<AssetSkeletalMesh> skeletal = Cast<AssetSkeletalMesh>(asset);
				std::vector<SkeletalMeshAnimation> animations = Utils::ImportAnimations(pathToRaw, skeletal->GetMesh(), settings.AnimationSettings.RootMotionType);

				std::string filename = Utils::AsString(outputFilename.stem()) + "_Anim";
				uint32_t animIndex = 0;
				for (const auto& anim : animations)
				{
					Path output = Utils::GetUniqueAssetFilepath(saveTo, filename);
					auto data = Serializer::SerializeAssetAnimationFromData(GUID{}, pathToRaw, animIndex++, anim, skeletal);
					FileSystem::Write(output, data);

					AssetManager::Register(Asset::Create(output));
				}
			}
		}

		return true;
	}

	void AssetImporter::Import(const std::vector<Path>& pathsToRaw, const Path& saveTo)
	{
		if (pathsToRaw.empty())
			return;

		if (pathsToRaw.size() == 1)
			EG_CORE_INFO("Importing an asset...");
		else
			EG_CORE_INFO("Importing {} assets at once: ", pathsToRaw.size());

		AssetImportSettings settings;
		size_t counter = 0;
		for (const auto& path : pathsToRaw)
		{
			EG_CORE_TRACE("\tProgress: importing {}/{}: {}", ++counter, pathsToRaw.size(), path);
			Import(path, saveTo, GetAssetTypeByExtension(path), settings);
		}
		EG_CORE_INFO("Done");
	}

	Ref<AssetTexture2D> AssetImporter::ImportTexture2DFromMemory(DataBuffer buffer, const Path& saveTo, const std::string& filename, const AssetImportTexture2DSettings& settings)
	{
		const Path outputFilename = Utils::GetUniqueAssetFilepath(saveTo, filename);
		ScopedDataBuffer assetData = CreateTexture2DAssetFromMemory(buffer, outputFilename, settings);
		Ref<Asset> asset = Asset::Create(assetData, outputFilename);
		AssetManager::Register(asset);
		return Cast<AssetTexture2D>(asset);
	}

	Path AssetImporter::CreateMaterial(const Path& saveTo, const std::string& filename)
	{
		const Path outputFilename = Utils::GetUniqueAssetFilepath(saveTo, filename);
		ScopedDataBuffer data = Serializer::SerializeAssetMaterial(nullptr);
		FileSystem::Write(outputFilename, data);
		AssetManager::Register(Asset::Create(data, outputFilename));

		return outputFilename;
	}

	Path AssetImporter::CreatePhysicsMaterial(const Path& saveTo, const std::string& filename)
	{
		const Path outputFilename = Utils::GetUniqueAssetFilepath(saveTo, filename);
		ScopedDataBuffer data = Serializer::SerializeAssetPhysicsMaterial(nullptr);
		FileSystem::Write(outputFilename, data);
		AssetManager::Register(Asset::Create(data, outputFilename));

		return outputFilename;
	}

	Path AssetImporter::CreateSoundGroup(const Path& saveTo, const std::string& filename)
	{
		const Path outputFilename = Utils::GetUniqueAssetFilepath(saveTo, filename);
		ScopedDataBuffer data = Serializer::SerializeAssetSoundGroup(nullptr);
		FileSystem::Write(outputFilename, data);
		AssetManager::Register(Asset::Create(data, outputFilename));

		return outputFilename;
	}

	Path AssetImporter::CreateEntity(const Path& saveTo, const std::string& filename)
	{
		const Path outputFilename = Utils::GetUniqueAssetFilepath(saveTo, filename);
		ScopedDataBuffer data = Serializer::SerializeAssetEntity(nullptr);
		FileSystem::Write(outputFilename, data);
		AssetManager::Register(Asset::Create(data, outputFilename));

		return outputFilename;
	}

	Path AssetImporter::CreateScene(const Path& saveTo, const std::string& filename)
	{
		const Path outputFilename = Utils::GetUniqueAssetFilepath(saveTo, filename);
		SceneSerializer::Serialize(nullptr, outputFilename);
		AssetManager::Register(Asset::Create(outputFilename));

		return outputFilename;
	}

	Path AssetImporter::CreateAnimationGraph(const Path& saveTo, const Ref<AssetSkeletalMesh>& skeletal, const std::string& filename)
	{
		const Path outputFilename = Utils::GetUniqueAssetFilepath(saveTo, filename);
		ScopedDataBuffer data = Serializer::SerializeAssetAnimationGraph(nullptr, skeletal);
		FileSystem::Write(outputFilename, data);
		AssetManager::Register(Asset::Create(data, outputFilename));

		return outputFilename;
	}

	Path AssetImporter::CreateParticleSystem(const Path& saveTo, const std::string& filename)
	{
		const Path outputFilename = Utils::GetUniqueAssetFilepath(saveTo, filename);
		ScopedDataBuffer data = Serializer::SerializeAssetParticleSystem(nullptr);
		FileSystem::Write(outputFilename, data);

		AssetManager::Register(Asset::Create(data, outputFilename));

		return outputFilename;
	}

	Path AssetImporter::CreateAnimationBlendSpace(const Path& saveTo, const Ref<AssetSkeletalMesh>& skeletal, const std::string& filename)
	{
		const Path outputFilename = Utils::GetUniqueAssetFilepath(saveTo, filename);
		ScopedDataBuffer data = Serializer::SerializeAssetAnimationBlendSpace(nullptr, skeletal);
		FileSystem::Write(outputFilename, data);
		AssetManager::Register(Asset::Create(data, outputFilename));

		return outputFilename;
	}

	Path AssetImporter::CreateBehaviorGraph(const Path& saveTo, const std::string& filename)
	{
		const Path outputFilename = Utils::GetUniqueAssetFilepath(saveTo, filename);
		ScopedDataBuffer data = Serializer::SerializeAssetBehaviorGraph(nullptr);
		FileSystem::Write(outputFilename, data);

		AssetManager::Register(Asset::Create(data, outputFilename));

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
			{ ".fbx",   AssetType::StaticMesh },
			{ ".gltf",  AssetType::StaticMesh },
			{ ".blend", AssetType::StaticMesh },
			{ ".3ds",   AssetType::StaticMesh },
			{ ".obj",   AssetType::StaticMesh },
			{ ".smd",   AssetType::StaticMesh },
			{ ".vta",   AssetType::StaticMesh },
			{ ".stl",   AssetType::StaticMesh },
			{ ".mp3",   AssetType::Audio },
			{ ".wav",   AssetType::Audio },
			{ ".ogg",   AssetType::Audio },
			{ ".wma",   AssetType::Audio },
			{ ".ttf",   AssetType::Font },
			{ ".otf",   AssetType::Font },
		};

		static const std::locale& loc = std::locale("RU_ru");
		std::string extension = Utils::AsString(filepath.extension());

		for (char& c : extension)
			c = std::tolower(c, loc);

		auto it = s_SupportedFileFormats.find(extension);
		if (it != s_SupportedFileFormats.end())
			return it->second;

		return AssetType::None;
	}
	
	bool AssetImporter::ImportTexture2D(const Path& pathToRaw, const Path& outputFilename, const AssetImportSettings& settings)
	{
		ScopedDataBuffer buffer(FileSystem::Read(pathToRaw));
		CreateTexture2DAssetFromMemory(buffer.GetDataBuffer(), outputFilename, settings.Texture2DSettings);
		return true;
	}
	
	bool AssetImporter::ImportTextureCube(const Path& pathToRaw, const Path& outputFilename, const AssetImportSettings& settings)
	{
		ScopedDataBuffer buffer(FileSystem::Read(pathToRaw));
		const auto& textureSettings = settings.TextureCubeSettings;

		auto data = Serializer::SerializeAssetTextureCubeFromData(buffer.GetDataBuffer(), GUID{}, pathToRaw,
			textureSettings.ImportFormat, textureSettings.LayerSize, textureSettings.PrefilterSize, textureSettings.bCompress);
		FileSystem::Write(outputFilename, data);

		return true;
	}
	
	// Assigns each imported material to the sub-mesh slot it actually belongs to.
	// @materialIndices - `MeshImportData::MaterialIndices` for this mesh: materialIndices[slot] == index into `importedMaterials`.
	// Note: the slot is the position within this array (0, 1, 2...), NOT the raw assimp/global material index
	template<typename MeshRef>
	static void AssignImportedMaterials(const MeshRef& mesh, const std::vector<uint32_t>& materialIndices,
		const std::vector<Ref<AssetMaterial>>& importedMaterials)
	{
		for (size_t slot = 0; slot < materialIndices.size(); ++slot)
		{
			const uint32_t materialIndex = materialIndices[slot];
			if (materialIndex >= importedMaterials.size())
				continue;

			const auto& material = importedMaterials[materialIndex];
			mesh->SetMaterialAsset(uint32_t(slot), material);

			// The asset is used, register & save it if required
			if (!AssetManager::Exists(material->GetPath()))
			{
				AssetManager::Register(material);
				Asset::Save(material);
			}
		}
	}

	std::vector<Path> AssetImporter::ImportStaticMesh(const Path& pathToRaw, const Path& saveTo, const Path& outputFilename, const AssetImportSettings& settings)
	{
		std::vector<Utils::StaticMeshImportData> importedMeshes = Utils::ImportStaticMesh(pathToRaw, settings.MeshSettings.bCombineMeshes, settings.MeshSettings.bResetLocation);
		if (importedMeshes.empty())
		{
			EG_CORE_ERROR("Failed to import a mesh. No meshes in file '{0}'", pathToRaw);
			return {};
		}

		std::vector<Ref<AssetMaterial>> importedMaterials;
		if (settings.MeshSettings.bImportMaterials)
			importedMaterials = Utils::ImportMaterials(pathToRaw, saveTo);

		const bool bMultipleAssets = importedMeshes.size() > 1;
		std::vector<Path> outputFilenames;
		outputFilenames.reserve(importedMeshes.size());

		for (size_t i = 0; i < importedMeshes.size(); ++i)
		{
			auto& importedMeshData = importedMeshes[i];
			AssignImportedMaterials(importedMeshData.Mesh, importedMeshData.MaterialIndices, importedMaterials);

			// The originally-computed `outputFilename` is only reused as-is when there's a single resulting asset.
			// Otherwise every mesh gets its own uniquely-named file, preferring the mesh's own name when it has one.
			Path meshOutputFilename = outputFilename;
			if (bMultipleAssets)
			{
				const std::string baseName = !importedMeshData.Name.empty()
					? importedMeshData.Name
					: Utils::AsString(outputFilename.stem()) + "_" + std::to_string(i);
				meshOutputFilename = Utils::GetUniqueAssetFilepath(saveTo, baseName);
			}

			auto data = Serializer::SerializeAssetStaticMeshFromMesh(importedMeshData.Mesh, GUID{}, pathToRaw,
				settings.MeshSettings.bCombineMeshes, importedMeshData.Name, uint32_t(i), settings.MeshSettings.bResetLocation);
			FileSystem::Write(meshOutputFilename, data);
			outputFilenames.push_back(std::move(meshOutputFilename));
		}

		return outputFilenames;
	}

	std::vector<Path> AssetImporter::ImportSkeletalMesh(const Path& pathToRaw, const Path& saveTo, const Path& outputFilename, const AssetImportSettings& settings)
	{
		std::vector<Utils::SkeletalMeshImportData> importedMeshes = Utils::ImportSkeletalMesh(pathToRaw, settings.MeshSettings.bCombineMeshes, settings.MeshSettings.bResetLocation);
		if (importedMeshes.empty())
		{
			EG_CORE_ERROR("Failed to import a mesh. No meshes in file '{0}'", pathToRaw);
			return {};
		}

		std::vector<Ref<AssetMaterial>> importedMaterials;
		if (settings.MeshSettings.bImportMaterials)
			importedMaterials = Utils::ImportMaterials(pathToRaw, saveTo);

		const bool bMultipleAssets = importedMeshes.size() > 1;
		std::vector<Path> outputFilenames;
		outputFilenames.reserve(importedMeshes.size());

		for (size_t i = 0; i < importedMeshes.size(); ++i)
		{
			auto& importedMeshData = importedMeshes[i];
			AssignImportedMaterials(importedMeshData.Mesh, importedMeshData.MaterialIndices, importedMaterials);

			Path meshOutputFilename = outputFilename;
			if (bMultipleAssets)
			{
				const std::string baseName = !importedMeshData.Name.empty()
					? importedMeshData.Name
					: Utils::AsString(outputFilename.stem()) + "_" + std::to_string(i);
				meshOutputFilename = Utils::GetUniqueAssetFilepath(saveTo, baseName);
			}

			auto data = Serializer::SerializeAssetSkeletalMeshFromMesh(importedMeshData.Mesh, GUID{}, pathToRaw,
				settings.MeshSettings.bCombineMeshes, importedMeshData.Name, uint32_t(i), settings.MeshSettings.bResetLocation);
			FileSystem::Write(meshOutputFilename, data);
			outputFilenames.push_back(std::move(meshOutputFilename));
		}

		return outputFilenames;
	}
	
	bool AssetImporter::ImportAudio(const Path& pathToRaw, const Path& outputFilename, const AssetImportSettings& settings)
	{
		ScopedDataBuffer buffer(FileSystem::Read(pathToRaw));
		auto data = Serializer::SerializeAssetAudioFromData(buffer.GetDataBuffer(), GUID{}, pathToRaw, 1.f, 1.f, 0.f, nullptr);
		FileSystem::Write(outputFilename, data);

		return true;
	}
	
	bool AssetImporter::ImportFont(const Path& pathToRaw, const Path& outputFilename, const AssetImportSettings& settings)
	{
		ScopedDataBuffer buffer(FileSystem::Read(pathToRaw));

		Ref<Font> font = Font::Create(buffer.GetDataBuffer());
		auto data = Serializer::SerializeAssetFontFromData(buffer.GetDataBuffer(), font->GetAtlasData().GetDataBuffer(), font->GetAtlas()->GetSize(), GUID{}, pathToRaw);
		FileSystem::Write(outputFilename, data);

		return true;
	}

	bool AssetImporter::ImportAnimation(const Path& pathToRaw, const Path& saveTo, const Path& outputFilename, const AssetImportAnimationSettings& settings)
	{
		const auto& skeletal = settings.Skeletal;
		std::vector<SkeletalMeshAnimation> animations = Utils::ImportAnimations(pathToRaw, skeletal->GetMesh(), settings.RootMotionType);
		if (animations.empty())
		{
			EG_CORE_ERROR("Failed to import an animation. No animations in file '{0}'", pathToRaw);
			return false;
		}

		std::string filename = Utils::AsString(outputFilename.stem());
		uint32_t animIndex = 0;
		for (const auto& anim : animations)
		{
			Path output = Utils::GetUniqueAssetFilepath(saveTo, filename);

			auto data = Serializer::SerializeAssetAnimationFromData(GUID{}, pathToRaw, animIndex++, anim, skeletal);
			FileSystem::Write(output, data);
		}

		return true;
	}
}
