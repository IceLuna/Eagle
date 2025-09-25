#include "egpch.h"
#include "AssetImporter.h"
#include "AssetManager.h"

#include "Eagle/Core/DataBuffer.h"
#include "Eagle/Core/Serializer.h"
#include "Eagle/Core/SceneSerializer.h"
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
#include <stb_image_write.h>

namespace Eagle
{
	bool AssetImporter::Import(const Path& pathToRaw, const Path& saveTo, AssetType type, const AssetImportSettings& settings)
	{
		if (!std::filesystem::exists(pathToRaw) || std::filesystem::is_directory(pathToRaw))
		{
			EG_CORE_ERROR("Import failed. File doesn't exist: {}", pathToRaw.u8string());
			return false;
		}

		Path outputFilename = saveTo / (pathToRaw.stem().u8string() + Asset::GetExtension());
		if (std::filesystem::exists(outputFilename))
			outputFilename = Utils::GetUniqueAssetFilepath(outputFilename.parent_path(), outputFilename.stem().u8string());

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
				bSuccess = ImportAnimation(pathToRaw, saveTo, outputFilename, settings.AnimationSettings);
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

			// Import animations if required
			if (settings.MeshSettings.bImportAnimations && bSkeletal)
			{
				Ref<AssetSkeletalMesh> skeletal = Cast<AssetSkeletalMesh>(asset);
				std::vector<SkeletalMeshAnimation> animations = Utils::ImportAnimations(pathToRaw, skeletal->GetMesh(), settings.AnimationSettings.bRootMotion);

				std::string filename = outputFilename.stem().u8string() + "_Anim";
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

		const void* compressedTextureHandle = nullptr;
		bool bCompressTexture = textureSettings.bCompress;

		DataBuffer ktxData;

		if (bCompressTexture)
		{
			constexpr int desiredChannels = 4;
			int width, height, channels;

			void* stbiImageData = stbi_load_from_memory((uint8_t*)buffer.Data(), (int)buffer.Size(), &width, &height, &channels, desiredChannels);
			if (!stbiImageData)
			{
				EG_CORE_ERROR("Import failed. stbi_load_from_memory failed: {}", pathToRaw.u8string());
				return false;
			}

			const size_t textureMemSize = desiredChannels * width * height;
			compressedTextureHandle = TextureCompressor::Compress(DataBuffer(stbiImageData, textureMemSize), glm::uvec2(width, height),
				textureSettings.MipsCount, textureSettings.bNormalMap, textureSettings.bNeedAlpha);

			if (compressedTextureHandle)
				ktxData = TextureCompressor::GetKTX2Data(compressedTextureHandle);
			else
				bCompressTexture = false; // Failed to compress

			stbi_image_free(stbiImageData);
		}

		int width, height, channels;
		stbi_info_from_memory((uint8_t*)buffer.Data(), (int)buffer.Size(), &width, &height, &channels);

		auto data = Serializer::SerializeAssetTexture2DFromData(buffer.GetDataBuffer(), ktxData, GUID{}, pathToRaw,
				textureSettings.FilterMode, textureSettings.AddressMode, textureSettings.Anisotropy, textureSettings.MipsCount,
				width, height, textureSettings.ImportFormat, bCompressTexture, textureSettings.bNormalMap, textureSettings.bNeedAlpha);
		FileSystem::Write(outputFilename, data);

		if (compressedTextureHandle)
			TextureCompressor::Destroy(compressedTextureHandle);

		return true;
	}
	
	bool AssetImporter::ImportTextureCube(const Path& pathToRaw, const Path& outputFilename, const AssetImportSettings& settings)
	{
		ScopedDataBuffer buffer(FileSystem::Read(pathToRaw));
		const auto& textureSettings = settings.TextureCubeSettings;

		auto data = Serializer::SerializeAssetTextureCubeFromData(buffer.GetDataBuffer(), GUID{}, pathToRaw,
			textureSettings.ImportFormat, textureSettings.LayerSize, textureSettings.PrefilterSize);
		FileSystem::Write(outputFilename, data);

		return true;
	}
	
	bool AssetImporter::ImportStaticMesh(const Path& pathToRaw, const Path& saveTo, const Path& outputFilename, const AssetImportSettings& settings)
	{
		Utils::StaticMeshImportData importedMeshData = Utils::ImportStaticMesh(pathToRaw);
		if (!importedMeshData.Mesh)
		{
			EG_CORE_ERROR("Failed to import a mesh. No meshes in file '{0}'", pathToRaw.u8string());
			return false;
		}

		if (settings.MeshSettings.bImportMaterials)
		{
			std::vector<Ref<AssetMaterial>> importedMaterials = Utils::ImportMaterials(pathToRaw, saveTo);
			for (size_t i = 0; i < importedMeshData.MaterialIndices.size(); ++i)
			{
				const uint32_t materialIndex = importedMeshData.MaterialIndices[i];
				if (materialIndex >= importedMaterials.size())
					continue;

				importedMeshData.Mesh->SetMaterialAsset(materialIndex, importedMaterials[materialIndex]);
			}
		}

		auto data = Serializer::SerializeAssetStaticMeshFromMesh(importedMeshData.Mesh, GUID{}, pathToRaw);
		FileSystem::Write(outputFilename, data);

		return true;
	}

	bool AssetImporter::ImportSkeletalMesh(const Path& pathToRaw, const Path& saveTo, const Path& outputFilename, const AssetImportSettings& settings)
	{
		Utils::SkeletalMeshImportData importedMeshData = Utils::ImportSkeletalMesh(pathToRaw);
		if (!importedMeshData.Mesh)
		{
			EG_CORE_ERROR("Failed to import a mesh. No meshes in file '{0}'", pathToRaw.u8string());
			return false;
		}

		if (settings.MeshSettings.bImportMaterials)
		{
			std::vector<Ref<AssetMaterial>> importedMaterials = Utils::ImportMaterials(pathToRaw, saveTo);
			for (size_t i = 0; i < importedMeshData.MaterialIndices.size(); ++i)
			{
				const uint32_t materialIndex = importedMeshData.MaterialIndices[i];
				if (materialIndex >= importedMaterials.size())
					continue;

				importedMeshData.Mesh->SetMaterialAsset(materialIndex, importedMaterials[materialIndex]);
			}
		}

		auto data = Serializer::SerializeAssetSkeletalMeshFromMesh(importedMeshData.Mesh, GUID{}, pathToRaw);
		FileSystem::Write(outputFilename, data);

		return true;
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

		auto data = Serializer::SerializeAssetFontFromData(buffer.GetDataBuffer(), GUID{}, pathToRaw);
		FileSystem::Write(outputFilename, data);

		return true;
	}

	bool AssetImporter::ImportAnimation(const Path& pathToRaw, const Path& saveTo, const Path& outputFilename, const AssetImportAnimationSettings& settings)
	{
		const auto& skeletal = settings.Skeletal;
		std::vector<SkeletalMeshAnimation> animations = Utils::ImportAnimations(pathToRaw, skeletal->GetMesh(), settings.bRootMotion);
		if (animations.empty())
		{
			EG_CORE_ERROR("Failed to import an animation. No animations in file '{0}'", pathToRaw.u8string());
			return false;
		}

		std::string filename = outputFilename.stem().u8string();
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
