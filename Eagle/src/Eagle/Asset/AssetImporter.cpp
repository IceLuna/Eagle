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
#include "Eagle/Components/Components.h"

#include <stb_image.h>

namespace Eagle
{
	// Writes an asset file. If writing fails, removes whatever might have been partially written,
	// so a broken asset file doesn't get picked up the next time the project is opened
	static bool WriteAssetFile(const Path& outputFilename, const ScopedDataBuffer& data)
	{
		if (data && FileSystem::Write(outputFilename, data))
			return true;

		EG_CORE_ERROR("Failed to write the asset file: {}", outputFilename);
		std::error_code error;
		std::filesystem::remove(outputFilename, error);
		return false;
	}

	// @pathToRaw. Can be empty if it doesn't come from a file
	// Returns an eagle asset file data. Empty if the image is invalid or the asset file couldn't be written
	static ScopedDataBuffer CreateTexture2DAssetFromMemory(DataBuffer buffer, const Path& outputFilename, const AssetImportTexture2DSettings& settings, const Path pathToRaw = {})
	{
		const Path& source = pathToRaw.empty() ? outputFilename : pathToRaw;
		if (!buffer.Data || buffer.Size == 0)
		{
			EG_CORE_ERROR("Failed to import a texture. The image data is empty: {}", source);
			return {};
		}

		// stb takes the size as an `int`
		if (buffer.Size > size_t(std::numeric_limits<int>::max()))
		{
			EG_CORE_ERROR("Failed to import a texture. The image file is too large ({} bytes): {}", buffer.Size, source);
			return {};
		}

		int width = 0, height = 0, channels = 0;
		if (!stbi_info_from_memory((const stbi_uc*)buffer.Data, (int)buffer.Size, &width, &height, &channels) || width <= 0 || height <= 0)
		{
			const char* reason = stbi_failure_reason();
			EG_CORE_ERROR("Failed to import a texture. Unsupported or corrupted image ({}): {}", reason ? reason : "unknown reason", source);
			return {};
		}

		TextureCompressor::Result compressedData{};
		TextureCompressor::Quality compression = settings.Compression;

		if (compression != TextureCompressor::Quality::Disabled)
		{
			const uint32_t targetNumChannels = AssetTextureFormatToChannels(settings.ImportFormat, compression);
			compressedData = TextureCompressor::Compress(buffer, targetNumChannels, settings.MipsCount, compression, settings.bNormalMap);
			if (!compressedData)
				compression = TextureCompressor::Quality::Disabled; // Failed to compress
		}

		auto data = Serializer::SerializeAssetTexture2DFromData(buffer, compressedData.DataPerMip, compressedData.Format, GUID{}, pathToRaw,
			settings.FilterMode, settings.AddressMode, settings.Anisotropy, settings.MipsCount,
			width, height, settings.ImportFormat, compression, settings.bNormalMap);
		if (!WriteAssetFile(outputFilename, data))
			return {};

		return data;
	}

	bool AssetImporter::Import(const Path& pathToRaw, const Path& saveTo, AssetType type, const AssetImportSettings& settings)
	{
		if (!std::filesystem::exists(pathToRaw) || std::filesystem::is_directory(pathToRaw))
		{
			EG_CORE_ERROR("Import failed. File doesn't exist: {}", pathToRaw);
			return false;
		}

		Path outputFilename = Utils::GetUniqueAssetFilepath(saveTo, Utils::AsString(pathToRaw.stem()));

		// Most importers produce exactly one asset. Static/Skeletal Mesh importers can produce several
		// when `settings.MeshSettings.bCombineMeshes` is `false` and the source file has multiple meshes.
		std::vector<Path> outputFilenames;
		std::vector<MeshImportResult> meshResults;
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
				meshResults = ImportStaticMesh(pathToRaw, saveTo, outputFilename, settings);
				for (const auto& result : meshResults)
					outputFilenames.push_back(result.OutputFilename);
				break;
			case AssetType::SkeletalMesh:
				meshResults = ImportSkeletalMesh(pathToRaw, saveTo, outputFilename, settings);
				for (const auto& result : meshResults)
					outputFilenames.push_back(result.OutputFilename);
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
			{
				auto outputs = ImportAnimations(pathToRaw, saveTo, outputFilename, settings.AnimationSettings);
				outputFilenames.insert(outputFilenames.end(), outputs.begin(), outputs.end());
				break;
			}
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
			if (!asset)
			{
				EG_CORE_ERROR("Failed to load the asset: {}", filename);
				continue;
			}

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

		const bool bStatic = type == AssetType::StaticMesh;
		const bool bSkeletal = type == AssetType::SkeletalMesh;
		if ((bStatic || bSkeletal) && settings.MeshSettings.bCreateScene)
		{
			Ref<Scene> scene = MakeRef<Scene>();

			for (const auto& result : meshResults)
			{
				Ref<Asset> asset;
				AssetManager::Get(result.OutputFilename, &asset);

				if (!asset)
					continue;

				for (const auto& instance : result.Instances)
				{
					Entity entity = scene->CreateEntity(instance.Name);
					entity.SetWorldTransform(Math::DecomposeTransformMatrix(instance.Transform));

					if (bStatic)
					{
						auto mesh = Cast<AssetStaticMesh>(asset);
						auto& component = entity.AddComponent<StaticMeshComponent>();
						component.SetMeshAsset(mesh);
					}
					else if (bSkeletal)
					{
						auto mesh = Cast<AssetSkeletalMesh>(asset);
						auto& component = entity.AddComponent<SkeletalMeshComponent>();
						component.SetMeshAsset(mesh);
					}
					else
					{
						EG_CORE_ASSERT(false);
					}
				}
			}

			std::string sceneFilename = Utils::AsString(outputFilename.stem()) + "_Scene";
			Path sceneAssetPath = AssetImporter::CreateScene(saveTo, sceneFilename);
			Ref<Asset> asset;
			if (AssetManager::Get(sceneAssetPath, &asset))
			{
				if (Ref<AssetScene> sceneAsset = Cast<AssetScene>(asset))
					SceneSerializer::Serialize(scene, sceneAssetPath);
				else
					EG_CORE_ERROR("Failed to create a scene during mesh import. It's not a scene asset {0}", sceneAssetPath);
			}
			else
			{
				EG_CORE_ERROR("Failed to create a scene during mesh import. It's not a scene asset {0}", sceneAssetPath);
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
		if (!assetData)
			return {};

		Ref<Asset> asset = Asset::Create(assetData, outputFilename);
		if (!asset)
		{
			EG_CORE_ERROR("Failed to load the imported texture: {}", outputFilename);
			return {};
		}

		return Cast<AssetTexture2D>(AssetManager::Register(asset));
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

	Path AssetImporter::CreateSceneSequence(const Path& saveTo, const std::string& filename)
	{
		const Path outputFilename = Utils::GetUniqueAssetFilepath(saveTo, filename);
		ScopedDataBuffer data = Serializer::SerializeAssetSceneSequence(nullptr);
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
			{ ".glb",   AssetType::StaticMesh },
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

		std::string extension = Utils::AsString(filepath.extension());
		for (char& c : extension)
			c = std::tolower((unsigned char)c);

		auto it = s_SupportedFileFormats.find(extension);
		if (it != s_SupportedFileFormats.end())
			return it->second;

		return AssetType::None;
	}
	
	bool AssetImporter::ImportTexture2D(const Path& pathToRaw, const Path& outputFilename, const AssetImportSettings& settings)
	{
		ScopedDataBuffer buffer(FileSystem::Read(pathToRaw));
		if (!buffer)
		{
			EG_CORE_ERROR("Failed to import a texture. Couldn't read the file: {}", pathToRaw);
			return false;
		}

		const ScopedDataBuffer assetData = CreateTexture2DAssetFromMemory(buffer.GetDataBuffer(), outputFilename, settings.Texture2DSettings, pathToRaw);
		return assetData.IsValid();
	}
	
	bool AssetImporter::ImportTextureCube(const Path& pathToRaw, const Path& outputFilename, const AssetImportSettings& settings)
	{
		ScopedDataBuffer buffer(FileSystem::Read(pathToRaw));
		const auto& textureSettings = settings.TextureCubeSettings;

		auto data = Serializer::SerializeAssetTextureCubeFromData(buffer.GetDataBuffer(), GUID{}, pathToRaw,
			textureSettings.ImportFormat, textureSettings.LayerSize, textureSettings.PrefilterSize, textureSettings.bCompress);
		return FileSystem::Write(outputFilename, data);
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

	std::vector<MeshImportResult> AssetImporter::ImportStaticMesh(const Path& pathToRaw, const Path& saveTo, const Path& outputFilename, const AssetImportSettings& settings)
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
		std::vector<MeshImportResult> results;
		results.reserve(importedMeshes.size());

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
			results.push_back({ meshOutputFilename, std::move(importedMeshData.Instances) });
		}

		return results;
	}

	std::vector<MeshImportResult> AssetImporter::ImportSkeletalMesh(const Path& pathToRaw, const Path& saveTo, const Path& outputFilename, const AssetImportSettings& settings)
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
		std::vector<MeshImportResult> results;
		results.reserve(importedMeshes.size());

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
			results.push_back({ meshOutputFilename, std::move(importedMeshData.Instances) });
		}

		return results;
	}
	
	bool AssetImporter::ImportAudio(const Path& pathToRaw, const Path& outputFilename, const AssetImportSettings& settings)
	{
		ScopedDataBuffer buffer(FileSystem::Read(pathToRaw));
		auto data = Serializer::SerializeAssetAudioFromData(buffer.GetDataBuffer(), GUID{}, pathToRaw, 1.f, 1.f, 0.f, nullptr);
		return FileSystem::Write(outputFilename, data);
	}
	
	bool AssetImporter::ImportFont(const Path& pathToRaw, const Path& outputFilename, const AssetImportSettings& settings)
	{
		ScopedDataBuffer buffer(FileSystem::Read(pathToRaw));
		if (!buffer)
		{
			EG_CORE_ERROR("Failed to import a font. Couldn't read the file: {}", pathToRaw);
			return false;
		}

		Ref<Font> font = Font::Create(buffer.GetDataBuffer());
		if (!font || !font->GetAtlas() || !font->GetAtlasData())
		{
			EG_CORE_ERROR("Failed to import a font. Unsupported or corrupted font file: {}", pathToRaw);
			return false;
		}

		auto data = Serializer::SerializeAssetFontFromData(buffer.GetDataBuffer(), font->GetAtlasData().GetDataBuffer(), font->GetAtlas()->GetSize(), GUID{}, pathToRaw);
		return WriteAssetFile(outputFilename, data);
	}

	std::vector<Path> AssetImporter::ImportAnimations(const Path& pathToRaw, const Path& saveTo, const Path& outputFilename, const AssetImportAnimationSettings& settings)
	{
		const auto& skeletal = settings.Skeletal;
		std::vector<SkeletalMeshAnimation> animations = Utils::ImportAnimations(pathToRaw, skeletal->GetMesh(), settings.RootMotionType);
		if (animations.empty())
		{
			EG_CORE_ERROR("Failed to import an animation. No animations in file '{0}'", pathToRaw);
			return {};
		}

		std::vector<Path> outputs;
		outputs.reserve(animations.size());

		std::string filename = Utils::AsString(outputFilename.stem());
		uint32_t animIndex = 0;
		for (const auto& anim : animations)
		{
			Path output = Utils::GetUniqueAssetFilepath(saveTo, filename);

			auto data = Serializer::SerializeAssetAnimationFromData(GUID{}, pathToRaw, animIndex++, anim, skeletal);
			FileSystem::Write(output, data);

			outputs.push_back(std::move(output));
		}

		return outputs;
	}
}
