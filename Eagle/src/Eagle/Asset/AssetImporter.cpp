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
#include "Eagle/Core/AsyncTask.h"

#include <stb_image.h>

namespace Eagle
{
	namespace
	{
		// State of a background import
		struct ImportSession
		{
			// Assets aren't registered right away, they're registered on the main thread once a file is fully imported
			AssetStagingScope Staging;

			// Files written since the last import. They're deleted if the import of the current file gets cancelled
			std::vector<Path> WrittenFiles;

			std::vector<std::function<void()>> MainThreadWork;

			void Commit(AsyncTaskContext& task, const std::function<void()>& onFileImported)
			{
				WrittenFiles.clear();

				std::vector<Ref<Asset>> assets = Staging.Flush();
				task.RunOnMainThread([assets = std::move(assets), work = std::move(MainThreadWork), onFileImported]()
				{
					for (const auto& asset : assets)
						AssetManager::Register(asset);
					for (const auto& func : work)
						func();
					if (onFileImported)
						onFileImported();
				});
			}

			void Rollback()
			{
				Staging.Clear();
				MainThreadWork.clear();

				std::error_code error;
				for (const Path& path : WrittenFiles)
					std::filesystem::remove(path, error);

				if (!WrittenFiles.empty())
					EG_CORE_WARN("Import was cancelled. Removed {} partially imported file(s)", WrittenFiles.size());
				WrittenFiles.clear();
			}
		};

		thread_local ImportSession* t_ImportSession = nullptr;

		// `Import` can be called recursively (for example, a mesh importing textures for its materials).
		// Only the outermost call moves the progress bar, so that the bar doesn't jump back and forth
		thread_local uint32_t t_ImportDepth = 0;
		struct ImportDepthGuard
		{
			ImportDepthGuard() { ++t_ImportDepth; }
			~ImportDepthGuard() { --t_ImportDepth; }
		};

		// Same as `AsyncTaskProgressScope`, but only for the outermost `Import` call
		class ImportProgressScope
		{
		public:
			ImportProgressScope(float begin, float end)
			{
				if (t_ImportDepth <= 1)
					m_Scope.emplace(begin, end);
			}

		private:
			std::optional<AsyncTaskProgressScope> m_Scope;
		};

		void SetImportProgress(size_t done, size_t total)
		{
			if (t_ImportDepth <= 1)
				if (auto* task = AsyncTaskContext::GetCurrent())
					task->SetProgress(done, total);
		}

		void SetImportProgress(float progress)
		{
			if (t_ImportDepth <= 1)
				if (auto* task = AsyncTaskContext::GetCurrent())
					task->SetProgress(progress);
		}

		void SetImportDetail(std::string detail)
		{
			if (auto* task = AsyncTaskContext::GetCurrent())
				task->SetDetail(std::move(detail));
		}

		void SetImportExtraDetail(std::string detail)
		{
			if (auto* task = AsyncTaskContext::GetCurrent())
				task->SetExtraDetail(std::move(detail));
		}

		void TrackCreatedFile(const Path& path, bool bExistedBefore)
		{
			if (t_ImportSession && !bExistedBefore)
				t_ImportSession->WrittenFiles.push_back(path);
		}

		bool WriteImportedFile(const Path& path, const DataBuffer& data)
		{
			std::error_code error;
			const bool bExisted = std::filesystem::exists(path, error);
			const bool bResult = FileSystem::Write(path, data);
			TrackCreatedFile(path, bExisted);
			return bResult;
		}

		bool WriteImportedFile(const Path& path, const ScopedDataBuffer& data)
		{
			return WriteImportedFile(path, data.GetDataBuffer());
		}

		// Must be called on the main thread because `Scene` isn't thread-safe
		void CreateSceneFromMeshes(const std::vector<MeshImportResult>& meshResults, bool bStatic, const Path& saveTo, const std::string& sceneFilename)
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
					else
					{
						auto mesh = Cast<AssetSkeletalMesh>(asset);
						auto& component = entity.AddComponent<SkeletalMeshComponent>();
						component.SetMeshAsset(mesh);
					}
				}
			}

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
	}

	// Writes an asset file. If writing fails, removes whatever might have been partially written,
	// so a broken asset file doesn't get picked up the next time the project is opened
	static bool WriteAssetFile(const Path& outputFilename, const ScopedDataBuffer& data)
	{
		if (data && WriteImportedFile(outputFilename, data))
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
			SetImportDetail("Compressing " + Utils::AsString(source.filename()) + " (" + std::to_string(width) + "x" + std::to_string(height) + ")");

			uint32_t lastPercent = uint32_t(-1);
			auto compressionCallback = [&](float progress) -> bool
			{
				const uint32_t percent = glm::clamp(uint32_t(progress * 100.f), 0u, 100u);
				if (lastPercent != percent)
				{
					std::string str = " (" + std::to_string(percent) + "%)";
					SetImportExtraDetail(std::move(str));
					lastPercent = percent;
				}

				SetImportProgress(progress);
				return !AsyncTaskContext::IsCurrentCancelRequested();
			};

			const uint32_t targetNumChannels = AssetTextureFormatToChannels(settings.ImportFormat, compression);
			{
				ImportProgressScope compressionProgress(0.f, 0.95f);
				compressedData = TextureCompressor::Compress(buffer, targetNumChannels, settings.MipsCount, compression, settings.bNormalMap, false, compressionCallback);
			}
			if (!compressedData)
				compression = TextureCompressor::Quality::Disabled; // Failed to compress
		}

		if (AsyncTaskContext::IsCurrentCancelRequested())
			return {};

		SetImportDetail("Writing " + Utils::AsString(outputFilename.filename()));
		auto data = Serializer::SerializeAssetTexture2DFromData(buffer, compressedData.DataPerMip, compressedData.Format, GUID{}, pathToRaw,
			settings.FilterMode, settings.AddressMode, settings.Anisotropy, settings.MipsCount,
			width, height, settings.ImportFormat, compression, settings.bNormalMap);
		if (!WriteAssetFile(outputFilename, data))
			return {};

		return data;
	}

	bool AssetImporter::Import(const Path& pathToRaw, const Path& saveTo, AssetType type, const AssetImportSettings& settings)
	{
		ImportDepthGuard depthGuard;

		if (AsyncTaskContext::IsCurrentCancelRequested())
			return false;

		if (!std::filesystem::exists(pathToRaw) || std::filesystem::is_directory(pathToRaw))
		{
			EG_CORE_ERROR("Import failed. File doesn't exist: {}", pathToRaw);
			return false;
		}

		Path outputFilename = Utils::GetUniqueAssetFilepath(saveTo, Utils::AsString(pathToRaw.stem()));
		SetImportDetail("Reading " + Utils::AsString(pathToRaw.filename()));

		// Most importers produce exactly one asset. Static/Skeletal Mesh importers can produce several
		// when `settings.MeshSettings.bCombineMeshes` is `false` and the source file has multiple meshes.
		std::vector<Path> outputFilenames;
		std::vector<MeshImportResult> meshResults;

		std::optional<ImportProgressScope> convertProgress;
		convertProgress.emplace(0.f, 0.85f);
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

		convertProgress.reset();

		if (outputFilenames.empty() || AsyncTaskContext::IsCurrentCancelRequested())
			return false;

		ImportProgressScope loadProgress(0.85f, 1.f);

		// Animations apply to the whole armature, so even when multiple Skeletal Mesh assets were produced
		// (bCombineMeshes is false), only import the animation set once, against the first resulting skeletal asset.
		bool bAnimationsImported = false;

		for (size_t outputIndex = 0; outputIndex < outputFilenames.size(); ++outputIndex)
		{
			if (AsyncTaskContext::IsCurrentCancelRequested())
				return false;

			const Path& filename = outputFilenames[outputIndex];
			SetImportDetail("Loading " + Utils::AsString(filename.filename()));
			SetImportProgress(outputIndex, outputFilenames.size());

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
				SetImportDetail("Reading animations from " + Utils::AsString(pathToRaw.filename()));
				std::vector<SkeletalMeshAnimation> animations = Utils::ImportAnimations(pathToRaw, skeletal->GetMesh(), settings.AnimationSettings.RootMotionType);

				std::string animFilename = Utils::AsString(outputFilename.stem()) + "_Anim";
				uint32_t animIndex = 0;
				for (const auto& anim : animations)
				{
					if (AsyncTaskContext::IsCurrentCancelRequested())
						return false;

					Path output = Utils::GetUniqueAssetFilepath(saveTo, animFilename);
					SetImportDetail("Writing " + Utils::AsString(output.filename()));
					auto data = Serializer::SerializeAssetAnimationFromData(GUID{}, pathToRaw, animIndex++, anim, skeletal);
					WriteImportedFile(output, data);

					AssetManager::Register(Asset::Create(output));
				}
			}
		}

		const bool bStatic = type == AssetType::StaticMesh;
		const bool bSkeletal = type == AssetType::SkeletalMesh;
		if ((bStatic || bSkeletal) && settings.MeshSettings.bCreateScene)
		{
			if (AsyncTaskContext::IsCurrentCancelRequested())
				return false;

			std::string sceneFilename = Utils::AsString(outputFilename.stem()) + "_Scene";
			if (t_ImportSession)
			{
				// Scenes can only be created on the main thread. It's done after this file's assets are registered
				t_ImportSession->MainThreadWork.push_back([meshResults = std::move(meshResults), bStatic, saveTo, sceneFilename = std::move(sceneFilename)]()
				{
					CreateSceneFromMeshes(meshResults, bStatic, saveTo, sceneFilename);
				});
			}
			else
			{
				CreateSceneFromMeshes(meshResults, bStatic, saveTo, sceneFilename);
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

	void AssetImporter::ImportAsync(std::vector<AssetImportRequest> requests, const Path& saveTo,
		std::function<void(const AssetImportAsyncResult&)> onFinished, std::function<void()> onFileImported)
	{
		if (requests.empty())
		{
			if (onFinished)
				onFinished(AssetImportAsyncResult{});
			return;
		}

		const uint32_t count = uint32_t(requests.size());

		AsyncTaskDesc desc;
		desc.Name = count == 1 ? "Importing " + Utils::AsString(requests[0].PathToRaw.filename()) : "Importing " + std::to_string(count) + " files";
		desc.bCancelable = true;
		desc.bModal = false; // Don't block the user

		auto result = MakeRef<AssetImportAsyncResult>();
		AsyncTaskManager::Submit(desc,
			[requests = std::move(requests), saveTo, result, onFileImported](AsyncTaskContext& task) -> AsyncTaskResult
			{
				const size_t count = requests.size();
				EG_CORE_INFO("Importing {} file(s) in background into: {}", count, saveTo);

				ImportSession session;
				t_ImportSession = &session;
				struct SessionGuard { ~SessionGuard() { t_ImportSession = nullptr; } } sessionGuard;

				try
				{
					for (size_t i = 0; i < count; ++i)
					{
						if (task.IsCancelRequested())
						{
							result->Skipped += uint32_t(count - i);
							result->bCancelled = true;
							break;
						}

						const AssetImportRequest& request = requests[i];
						task.SetStatus("Importing " + std::to_string(i + 1) + "/" + std::to_string(count) + ": " + Utils::AsString(request.PathToRaw.filename()));
						task.SetDetail({});
						EG_CORE_TRACE("\tProgress: importing {}/{}: {}", i + 1, count, request.PathToRaw);

						bool bImported = false;
						{
							AsyncTaskProgressScope progress(float(i) / float(count), float(i + 1) / float(count));
							const AssetType type = (request.Type == AssetType::None) ? GetAssetTypeByExtension(request.PathToRaw) : request.Type;
							bImported = Import(request.PathToRaw, saveTo, type, request.Settings);
						}

						if (!bImported && task.IsCancelRequested())
						{
							session.Rollback();
							result->Skipped += uint32_t(count - i);
							result->bCancelled = true;
							break;
						}

						if (bImported)
							++result->Succeeded;
						else
							++result->Failed;

						session.Commit(task, onFileImported);
					}
				}
				catch (...)
				{
					session.Rollback();
					throw;
				}

				if (result->bCancelled)
					return AsyncTaskResult::Cancelled;
				return result->Failed > 0 ? AsyncTaskResult::Failed : AsyncTaskResult::Succeeded;
			},
			[result, count, onFinished = std::move(onFinished)](AsyncTaskResult taskResult)
			{
				const uint32_t processed = result->Succeeded + result->Failed + result->Skipped;
				if (processed < count)
				{
					// Cancelled before it started, or failed with an exception
					if (taskResult == AsyncTaskResult::Cancelled)
					{
						result->bCancelled = true;
						result->Skipped += count - processed;
					}
					else
					{
						result->Failed += count - processed;
					}
				}

				if (result->bCancelled)
					EG_CORE_WARN("Import was cancelled. Imported: {}. Failed: {}. Skipped: {}", result->Succeeded, result->Failed, result->Skipped);
				else
					EG_CORE_INFO("Import finished. Imported: {}. Failed: {}", result->Succeeded, result->Failed);

				if (onFinished)
					onFinished(*result);
			});
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

		SetImportDetail("Generating cube map from " + Utils::AsString(pathToRaw.filename()));
		auto data = Serializer::SerializeAssetTextureCubeFromData(buffer.GetDataBuffer(), GUID{}, pathToRaw,
			textureSettings.ImportFormat, textureSettings.LayerSize, textureSettings.PrefilterSize, textureSettings.bCompress);
		if (AsyncTaskContext::IsCurrentCancelRequested())
			return false;

		SetImportDetail("Writing " + Utils::AsString(outputFilename.filename()));
		return WriteImportedFile(outputFilename, data);
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
				std::error_code error;
				const bool bExisted = std::filesystem::exists(material->GetPath(), error);

				AssetManager::Register(material);
				Asset::Save(material);
				TrackCreatedFile(material->GetPath(), bExisted);
			}
		}
	}

	std::vector<MeshImportResult> AssetImporter::ImportStaticMesh(const Path& pathToRaw, const Path& saveTo, const Path& outputFilename, const AssetImportSettings& settings)
	{
		std::vector<Utils::StaticMeshImportData> importedMeshes;
		{
			ImportProgressScope progress(0.f, 0.4f);
			SetImportDetail("Reading meshes from " + Utils::AsString(pathToRaw.filename()));
			importedMeshes = Utils::ImportStaticMesh(pathToRaw, settings.MeshSettings.bCombineMeshes, settings.MeshSettings.bResetLocation);
		}
		if (AsyncTaskContext::IsCurrentCancelRequested())
			return {};
		if (importedMeshes.empty())
		{
			EG_CORE_ERROR("Failed to import a mesh. No meshes in file '{0}'", pathToRaw);
			return {};
		}

		std::vector<Ref<AssetMaterial>> importedMaterials;
		if (settings.MeshSettings.bImportMaterials)
		{
			ImportProgressScope progress(0.4f, 0.8f);

			// Textures of the materials are imported as nested imports, so they only update the text, not the bar.
			// The number of textures isn't known up front, so they'd make the bar jump back and forth.
			// Needed for embedded textures too, cause they don't go through `Import`, so they don't increase the depth themselves
			ImportDepthGuard nestedImportGuard;

			SetImportDetail("Importing materials & textures from " + Utils::AsString(pathToRaw.filename()));
			importedMaterials = Utils::ImportMaterials(pathToRaw, saveTo);
		}
		if (AsyncTaskContext::IsCurrentCancelRequested())
			return {};

		ImportProgressScope writeProgress(0.8f, 1.f);

		const bool bMultipleAssets = importedMeshes.size() > 1;
		std::vector<MeshImportResult> results;
		results.reserve(importedMeshes.size());

		for (size_t i = 0; i < importedMeshes.size(); ++i)
		{
			if (AsyncTaskContext::IsCurrentCancelRequested())
				return {};
			SetImportProgress(i, importedMeshes.size());

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

			SetImportDetail("Writing " + Utils::AsString(meshOutputFilename.filename()));
			auto data = Serializer::SerializeAssetStaticMeshFromMesh(importedMeshData.Mesh, GUID{}, pathToRaw,
				settings.MeshSettings.bCombineMeshes, importedMeshData.Name, uint32_t(i), settings.MeshSettings.bResetLocation);
			WriteImportedFile(meshOutputFilename, data);
			results.push_back({ meshOutputFilename, std::move(importedMeshData.Instances) });
		}

		return results;
	}

	std::vector<MeshImportResult> AssetImporter::ImportSkeletalMesh(const Path& pathToRaw, const Path& saveTo, const Path& outputFilename, const AssetImportSettings& settings)
	{
		std::vector<Utils::SkeletalMeshImportData> importedMeshes;
		{
			ImportProgressScope progress(0.f, 0.4f);
			SetImportDetail("Reading meshes from " + Utils::AsString(pathToRaw.filename()));
			importedMeshes = Utils::ImportSkeletalMesh(pathToRaw, settings.MeshSettings.bCombineMeshes, settings.MeshSettings.bResetLocation);
		}
		if (AsyncTaskContext::IsCurrentCancelRequested())
			return {};
		if (importedMeshes.empty())
		{
			EG_CORE_ERROR("Failed to import a mesh. No meshes in file '{0}'", pathToRaw);
			return {};
		}

		std::vector<Ref<AssetMaterial>> importedMaterials;
		if (settings.MeshSettings.bImportMaterials)
		{
			ImportProgressScope progress(0.4f, 0.8f);

			// Textures of the materials are imported as nested imports, so they only update the text, not the bar.
			// The number of textures isn't known up front, so they'd make the bar jump back and forth.
			// Needed for embedded textures too, cause they don't go through `Import`, so they don't increase the depth themselves
			ImportDepthGuard nestedImportGuard;

			SetImportDetail("Importing materials & textures from " + Utils::AsString(pathToRaw.filename()));
			importedMaterials = Utils::ImportMaterials(pathToRaw, saveTo);
		}
		if (AsyncTaskContext::IsCurrentCancelRequested())
			return {};

		ImportProgressScope writeProgress(0.8f, 1.f);

		const bool bMultipleAssets = importedMeshes.size() > 1;
		std::vector<MeshImportResult> results;
		results.reserve(importedMeshes.size());

		for (size_t i = 0; i < importedMeshes.size(); ++i)
		{
			if (AsyncTaskContext::IsCurrentCancelRequested())
				return {};
			SetImportProgress(i, importedMeshes.size());

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

			SetImportDetail("Writing " + Utils::AsString(meshOutputFilename.filename()));
			auto data = Serializer::SerializeAssetSkeletalMeshFromMesh(importedMeshData.Mesh, GUID{}, pathToRaw,
				settings.MeshSettings.bCombineMeshes, importedMeshData.Name, uint32_t(i), settings.MeshSettings.bResetLocation);
			WriteImportedFile(meshOutputFilename, data);
			results.push_back({ meshOutputFilename, std::move(importedMeshData.Instances) });
		}

		return results;
	}
	
	bool AssetImporter::ImportAudio(const Path& pathToRaw, const Path& outputFilename, const AssetImportSettings& settings)
	{
		ScopedDataBuffer buffer(FileSystem::Read(pathToRaw));
		auto data = Serializer::SerializeAssetAudioFromData(buffer.GetDataBuffer(), GUID{}, pathToRaw, 1.f, 1.f, 0.f, nullptr);
		return WriteImportedFile(outputFilename, data);
	}
	
	bool AssetImporter::ImportFont(const Path& pathToRaw, const Path& outputFilename, const AssetImportSettings& settings)
	{
		ScopedDataBuffer buffer(FileSystem::Read(pathToRaw));
		if (!buffer)
		{
			EG_CORE_ERROR("Failed to import a font. Couldn't read the file: {}", pathToRaw);
			return false;
		}

		SetImportDetail("Generating font atlas for " + Utils::AsString(pathToRaw.filename()));
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
		std::vector<SkeletalMeshAnimation> animations;
		{
			ImportProgressScope progress(0.f, 0.6f);
			SetImportDetail("Reading animations from " + Utils::AsString(pathToRaw.filename()));
			animations = Utils::ImportAnimations(pathToRaw, skeletal->GetMesh(), settings.RootMotionType);
		}
		if (AsyncTaskContext::IsCurrentCancelRequested())
			return {};
		if (animations.empty())
		{
			EG_CORE_ERROR("Failed to import an animation. No animations in file '{0}'", pathToRaw);
			return {};
		}

		std::vector<Path> outputs;
		outputs.reserve(animations.size());

		ImportProgressScope writeProgress(0.6f, 1.f);
		std::string filename = Utils::AsString(outputFilename.stem());
		uint32_t animIndex = 0;
		for (const auto& anim : animations)
		{
			if (AsyncTaskContext::IsCurrentCancelRequested())
				return {};
			SetImportProgress(animIndex, animations.size());

			Path output = Utils::GetUniqueAssetFilepath(saveTo, filename);
			SetImportDetail("Writing " + Utils::AsString(output.filename()));

			auto data = Serializer::SerializeAssetAnimationFromData(GUID{}, pathToRaw, animIndex++, anim, skeletal);
			WriteImportedFile(output, data);

			outputs.push_back(std::move(output));
		}

		return outputs;
	}
}
