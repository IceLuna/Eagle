#include "egpch.h"
#include "AssetManager.h"

#include "Eagle/Core/Project.h"
#include "Eagle/Core/Serializer.h"
#include "Eagle/Core/SceneSerializer.h"
#include "Eagle/Core/ThreadPool.h"
#include "Eagle/Core/AsyncTask.h"
#include "Eagle/Utils/Compressor.h"
#include "Eagle/Utils/Timer.h"
#include "Eagle/Utils/SerializerUtils.h"
#include "Eagle/Script/ScriptEngine.h"

namespace Eagle
{
	namespace Utils
	{
		static bool IsAssetExtension(const Path& filepath)
		{
			return Utils::HasExtension(filepath, Asset::GetExtension());
		}
	}

	AssetsMap AssetManager::s_Assets;
	AssetsMapByGUID AssetManager::s_AssetsByGUID;
	ankerl::unordered_dense::map<GUID, std::function<void(const Ref<Asset>&)>> AssetManager::s_Callbacks;
	AssetsMapByGUID AssetManager::s_RuntimeAssets;
	Ref<AssetTextureCube> AssetManager::s_Skybox;
	Ref<AssetStaticMesh> AssetManager::s_Sphere;
	Ref<AssetStaticMesh> AssetManager::s_Cube;

	// There's no point in loading all assets at the beggining.
	// We should load assets as needed and try to unload them when scenes change.
	// So here we are creating a map, where Path is a path to an asset and ScopedDataBuffer is a buffer that contains asset data.
	// Using this approach will allow us to quickly load assets since there's no need to parse the asset pack.
	// And when an asset is requested, we should check if it's loaded already (s_Assets)
	static ankerl::unordered_dense::map<Path, Ref<ScopedDataBuffer>> s_AssetPackAssets;
	static ankerl::unordered_dense::map<GUID, std::pair<Path, Ref<ScopedDataBuffer>>> s_AssetPackAssetsByGUID;
	static std::mutex s_Mutex;
	static bool s_bGame = false;

	// Guards `s_Callbacks`. Kept separate from `s_Mutex` so asset lookups don't contend with callback bookkeeping.
	// Never held while a callback is being called
	static std::mutex s_CallbacksMutex;

	// Game only. Assets that are currently being deserialized from the asset pack, keyed by path.
	// A thread that requests an asset that's already being loaded waits on its future instead of loading it again.
	struct PendingAssetLoad
	{
		std::shared_future<Ref<Asset>> Future;
		std::thread::id LoadingThread; // Used to detect an asset that (indirectly) depends on itself
	};
	static ankerl::unordered_dense::map<Path, PendingAssetLoad> s_PendingLoads;

	// Staging scope of the calling thread
	static thread_local AssetStagingScope* t_StagingScope = nullptr;

	AssetStagingScope::AssetStagingScope()
	{
		EG_CORE_ASSERT(t_StagingScope == nullptr, "Asset staging scopes can't be nested");
		t_StagingScope = this;
	}

	AssetStagingScope::~AssetStagingScope()
	{
		if (t_StagingScope == this)
			t_StagingScope = nullptr;

		if (!m_Pending.empty())
			EG_CORE_WARN("{} staged asset(s) were never registered", m_Pending.size());
	}

	void AssetManager::Init()
	{
		s_bGame = Application::Get().IsGame();
		if (s_bGame)
			return;
		
		AssetEntity::s_EntityAssetsScene = MakeRef<Scene>();

		struct AssetsQueue
		{
			std::vector<Path> Paths;
			bool bAsync = true;
		};

		// Defines the order for assets loading
		// All `assetsToLoadQueue[0]` will be loaded first, then [1] and so on.
		std::array<AssetsQueue, 7> assetsToLoadQueue;
		for (auto& assets : assetsToLoadQueue)
			assets.Paths.reserve(25);

		const Path contentPath = Project::GetContentPath();
		const Path& projectPath = Project::GetProjectPath();

		const auto& project = Project::GetProjectInfo();
		if (!ScriptEngine::LoadAppAssembly(Project::GetBinariesPath() / (project.Name + ".dll")))
		{
			const std::string error = std::string("Open VS solution (") +
				Utils::AsString(project.BasePath / (project.Name + ".sln")) + " or \"File > Open VS Solution\") and compile the project.\nIf the solution is not there, try to generate it \"File > Generate VS Solution\"";
			EG_CORE_WARN(error);
		}

		Timer globalTimer;
		for (auto& dirEntry : std::filesystem::recursive_directory_iterator(contentPath))
		{
			if (dirEntry.is_directory())
				continue;

			Path assetPath = std::filesystem::relative(dirEntry.path(), projectPath);
			if (!Utils::IsAssetExtension(assetPath))
				continue;

			const AssetType type = Serializer::GetAssetType(assetPath);

			// We deffer the loading of some assets:
			// Materials: we can't load materials unless all textures are loaded since materials refer to them
			// Audio: we can't load audios unless all sound groups are loaded since audios refer to them
			if (type == AssetType::Material || type == AssetType::Audio)
			{
				assetsToLoadQueue[1].Paths.emplace_back(std::move(assetPath));
				continue;
			}
			// Static & Skeletal meshes: we can't load meshes unless all materials are loaded since meshes refer to them
			else if (type == AssetType::StaticMesh || type == AssetType::SkeletalMesh)
			{
				assetsToLoadQueue[2].Paths.emplace_back(std::move(assetPath));
				continue;
			}
			// Animation: we can't load animations unless all skeletal meshes are loaded since animations refer to them
			// Particle System: we can't load particles unless all skeletal meshes are loaded since particle systems might refer to them
			else if (type == AssetType::Animation || type == AssetType::ParticleSystem)
			{
				assetsToLoadQueue[3].Paths.emplace_back(std::move(assetPath));
				continue;
			}
			// Animation BlendSpace: we can't load them unless all skeletal meshes & animations are loaded since blend spaces refer to them
			else if (type == AssetType::AnimationBlendSpace)
			{
				assetsToLoadQueue[4].Paths.emplace_back(std::move(assetPath));
				continue;
			}
			// Animation Graph: we can't load graphs unless all skeletal meshes & animations are loaded since graphs refer to them
			else if (type == AssetType::AnimationGraph)
			{
				assetsToLoadQueue[5].Paths.emplace_back(std::move(assetPath));
				// Currently, we can't load graphs in parallel because during its compilation we use imgui node editor and it causes issues
				// TODO: fix it by decoupling it from node editor
				assetsToLoadQueue[5].bAsync = false;
				continue;
			}
			// Entity: we can't load entities unless all assets are loaded since entities might refer to anything
			else if (type == AssetType::Entity)
			{
				// Entity assets need to be created in a single thread (C# mono related issues)
				assetsToLoadQueue[6].Paths.emplace_back(std::move(assetPath));
				assetsToLoadQueue[6].bAsync = false;
				continue;
			}

			// Note: BehaviorGraph might refer to any asset via its C# public fields.
			// But it's ok since PublicField doesn't require Asset to be loaded.
			// It just stores an asset GUID
			assetsToLoadQueue[0].Paths.emplace_back(std::move(assetPath));
		}
		EG_CORE_INFO("Took {}s to traverse directories and find assets", globalTimer.GetSeconds());

		std::mutex mutex;
		constexpr bool bEnableAsyncLoading = true;
		const uint32_t threadCount = bEnableAsyncLoading ? std::thread::hardware_concurrency() : 1u;
		ThreadPool threadPool("AssetManager", threadCount, false);

		auto loadAssetFunc = [&mutex](const Path& assetPath, bool bUseMutex)
		{
			Timer timer;
			Ref<Asset> asset = Asset::Create(assetPath);
			EG_CORE_INFO("Loaded asset in {}s: {}", timer.GetSeconds(), assetPath);
			if (bUseMutex)
			{
				std::scoped_lock lock(mutex);
				Register(asset);
			}
			else
			{
				Register(asset);
			}
		};

		globalTimer.Restart();
		for (const auto& assets : assetsToLoadQueue)
		{
			if (assets.bAsync)
			{
				for (const auto& assetPath : assets.Paths)
				{
					threadPool->detach_task([&assetPath, &loadAssetFunc]()
					{
						const bool bUseMutex = true;
						loadAssetFunc(assetPath, bUseMutex);
					});
				}
				threadPool->wait();
			}
			else
			{
				for (const auto& assetPath : assets.Paths)
				{
					loadAssetFunc(assetPath, false);
				}
			}
		}

		EG_CORE_INFO("Took {}s to load all {} project assets using {} threads", globalTimer.GetSeconds(), s_Assets.size(), threadCount);

		s_Skybox = Cast<AssetTextureCube>(Asset::Create(Application::GetCorePath() / "assets/textures/IBL.egasset"));
		s_Sphere = Cast<AssetStaticMesh>(Asset::Create(Application::GetCorePath() / "assets/meshes/Sphere.egasset"));
		s_Cube = Cast<AssetStaticMesh>(Asset::Create(Application::GetCorePath() / "assets/meshes/Cube.egasset"));
	}

	void AssetManager::InitGame(const DataBuffer& assetPack)
	{
		s_bGame = Application::Get().IsGame();
		if (!s_bGame || assetPack.Size == 0)
			return;

		AssetEntity::s_EntityAssetsScene = MakeRef<Scene>();

		YAML::Node baseNode;
		Utils::ReadYAML(assetPack, &baseNode);

		auto assetsNodes = baseNode["Assets"];

		for (const auto& assetNode : assetsNodes)
		{
			const Path path = assetNode["Path"].as<std::string>();
			const GUID assetGUID = assetNode["GUID"].as<GUID>();
			const size_t dataSize = assetNode["DataSize"].as<size_t>();
			const size_t dataOffset = assetNode["DataOffset"].as<size_t>();

			Ref<ScopedDataBuffer> assetData = MakeRef<ScopedDataBuffer>();
			Utils::ReadBinary(assetPack, dataSize, dataOffset, assetData.get());

			s_AssetPackAssets.emplace(path, assetData);
			s_AssetPackAssetsByGUID.emplace(assetGUID, std::pair{ std::move(path), std::move(assetData) });
		}
	}

	void AssetManager::Reset()
	{
		{
			std::scoped_lock lock(s_CallbacksMutex);
			s_Callbacks.clear();
		}

		std::scoped_lock lock(s_Mutex);

		AssetEntity::s_EntityAssetsScene.reset();
		s_PendingLoads.clear();
		s_Assets.clear();
		s_AssetsByGUID.clear();
		s_RuntimeAssets.clear();
		s_AssetPackAssets.clear();
		s_AssetPackAssetsByGUID.clear();
		s_Skybox.reset();
		s_Sphere.reset();
		s_Cube.reset();
	}

	void AssetManager::ResetGameAssets()
	{
		std::scoped_lock lock(s_Mutex);

		s_Assets.clear();
		s_AssetsByGUID.clear();
	}

	void AssetManager::ResetRuntimeAsset()
	{
		std::scoped_lock lock(s_Mutex);
		s_RuntimeAssets.clear();
	}

	Ref<Asset> AssetManager::Register(const Ref<Asset>& asset)
	{
		if (!asset)
			return {};

		if (AssetStagingScope* staging = t_StagingScope)
		{
			// If it's already registered, use it instead
			{
				std::scoped_lock lock(s_Mutex);
				if (auto it = s_Assets.find(asset->GetPath()); it != s_Assets.end())
				{
					return it->second;
				}
			}

			auto [pathIt, bPathInserted] = staging->m_ByPath.emplace(asset->GetPath(), asset);
			if (!bPathInserted)
			{
				return pathIt->second;
			}

			auto [guidIt, bGUIDInserted] = staging->m_ByGUID.emplace(asset->GetGUID(), asset);
			if (!bGUIDInserted && guidIt->second != asset)
				EG_CORE_ERROR("Two staged assets share the same GUID: '{}' and '{}'", asset->GetPath(), guidIt->second->GetPath());

			staging->m_Pending.push_back(asset);
			return asset;
		}

		std::scoped_lock lock(s_Mutex);
		return Register_Internal(asset);
	}

	Ref<Asset> AssetManager::Register_Internal(const Ref<Asset>& asset)
	{
		auto [pathIt, bPathInserted] = s_Assets.emplace(asset->GetPath(), asset);
		if (!bPathInserted)
		{
			// Return the existing instance so the caller doesn't end up holding a second copy of the asset that the rest of the engine can't see
			if (pathIt->second != asset)
				EG_CORE_WARN("An asset is already registered at this path. Using the existing instance: {}", asset->GetPath());
			return pathIt->second;
		}

		auto [guidIt, bGUIDInserted] = s_AssetsByGUID.emplace(asset->GetGUID(), asset);
		if (!bGUIDInserted && guidIt->second != asset)
		{
			// Typically happens when an asset file was copied outside of the editor, so both files carry the same GUID.
			// Both assets stay reachable by path, but references by GUID will always resolve to the first registered one
			EG_CORE_ERROR("Two assets share the same GUID: '{}' and '{}'. References to this GUID will resolve to '{}'. "
				"Was one of them copied outside of the editor? Use `Duplicate` in the Content Browser to make copies of assets",
				asset->GetPath(), guidIt->second->GetPath(), guidIt->second->GetPath());
		}

		return asset;
	}

	Ref<Asset> AssetManager::LoadFromAssetPack(const Path& path, const Ref<ScopedDataBuffer>& assetData)
	{
		std::promise<Ref<Asset>> promise;
		{
			std::unique_lock lock(s_Mutex);

			// Another thread might have finished loading it between the caller's lookup and now
			if (auto it = s_Assets.find(path); it != s_Assets.end())
				return it->second;

			if (auto it = s_PendingLoads.find(path); it != s_PendingLoads.end())
			{
				// This thread is already loading this asset further up the stack, meaning the asset depends on itself.
				// Waiting here would deadlock, so just return.
				if (it->second.LoadingThread == std::this_thread::get_id())
				{
					EG_CORE_ERROR("Failed to load the asset. It (indirectly) references itself: {}", path);
					return {};
				}

				// Another thread is loading it. Wait for its result instead of deserializing it a second time
				std::shared_future<Ref<Asset>> future = it->second.Future;
				lock.unlock();
				return future.get();
			}

			s_PendingLoads.emplace(path, PendingAssetLoad{ promise.get_future().share(), std::this_thread::get_id() });
		}

		Ref<Asset> result;
		Timer timer;

		// Catching everything here guarantees that the pending entry gets removed and the promise gets fulfilled.
		// Otherwise, every thread waiting for this asset would wait forever
		try
		{
			result = Serializer::DeserializeAsset(assetData->GetDataBuffer(), path, false);
		}
		catch (const std::exception& e)
		{
			EG_CORE_ERROR("Exception while loading the asset {}: {}", path, e.what());
			result.reset();
		}
		catch (...)
		{
			EG_CORE_ERROR("Unknown exception while loading the asset: {}", path);
			result.reset();
		}

		if (result)
			EG_CORE_INFO("Loaded asset in {}s: {}", timer.GetSeconds(), path);
		else
			EG_CORE_ERROR("Failed to load the asset: {}", path);

		{
			// Registering and removing the pending entry happen under the same lock,
			// so other threads always see the asset either as pending or as registered, never as neither
			std::scoped_lock lock(s_Mutex);
			if (result)
				result = Register_Internal(result);
			s_PendingLoads.erase(path);
		}

		promise.set_value(result);
		return result;
	}

	void AssetManager::AddRuntimeAsset(const Ref<Asset>& asset)
	{
		if (asset)
		{
			std::scoped_lock lock(s_Mutex);
			s_RuntimeAssets.emplace(asset->GetGUID(), asset);
		}
	}
	
	bool AssetManager::Get(const Path& path, Ref<Asset>* outAsset)
	{
		if (const AssetStagingScope* staging = t_StagingScope)
		{
			if (auto it = staging->m_ByPath.find(path); it != staging->m_ByPath.end())
			{
				*outAsset = it->second;
				return true;
			}
		}

		{
			std::scoped_lock lock(s_Mutex);

			auto it = s_Assets.find(path);
			if (it != s_Assets.end())
			{
				*outAsset = it->second;
				return true;
			}
		}

		// Try to load it.
		// Note: `s_AssetPackAssets` is only written by `InitGame` / `Reset`, so it can be read without the lock here
		if (s_bGame)
		{
			auto it = s_AssetPackAssets.find(path);
			if (it != s_AssetPackAssets.end())
			{
				Ref<Asset> result = LoadFromAssetPack(path, it->second);
				if (!result)
					return false;

				*outAsset = std::move(result);
				return true;
			}
		}

		return false;
	}
	
	bool AssetManager::Get(const GUID& guid, Ref<Asset>* outAsset)
	{
		if (guid.IsNull())
			return false;

		if (const AssetStagingScope* staging = t_StagingScope)
		{
			if (auto it = staging->m_ByGUID.find(guid); it != staging->m_ByGUID.end())
			{
				*outAsset = it->second;
				return true;
			}
		}

		{
			std::scoped_lock lock(s_Mutex);

			auto it = s_AssetsByGUID.find(guid);
			if (it != s_AssetsByGUID.end())
			{
				*outAsset = it->second;
				return true;
			}

			it = s_RuntimeAssets.find(guid);
			if (it != s_RuntimeAssets.end())
			{
				*outAsset = it->second;
				return true;
			}
		}

		// Try to load it
		if (s_bGame)
		{
			auto it = s_AssetPackAssetsByGUID.find(guid);
			if (it != s_AssetPackAssetsByGUID.end())
			{
				const std::pair<Path, Ref<ScopedDataBuffer>>& assetInfo = it->second;
				const auto& assetPath = assetInfo.first;
				const auto& assetData = assetInfo.second;

				// Keyed by path, so a GUID request and a path request for the same asset share one load
				Ref<Asset> result = LoadFromAssetPack(assetPath, assetData);
				if (!result)
					return false;

				*outAsset = std::move(result);
				return true;
			}
		}

		return false;
	}

	bool AssetManager::Exists(const Path& path)
	{
		if (const AssetStagingScope* staging = t_StagingScope)
		{
			if (staging->m_ByPath.find(path) != staging->m_ByPath.end())
				return true;
		}

		std::scoped_lock lock(s_Mutex);

		auto it = s_Assets.find(path);
		if (it != s_Assets.end())
		{
			return true;
		}

		// Check asset pack
		if (s_bGame)
		{
			auto it = s_AssetPackAssets.find(path);
			if (it != s_AssetPackAssets.end())
			{
				return true;
			}
		}

		return false;
	}

	bool AssetManager::GetRuntimeAssetData(const Path& path, Ref<ScopedDataBuffer>* outData)
	{
		if (!s_bGame)
			return false;

		std::scoped_lock lock(s_Mutex);

		auto it = s_AssetPackAssets.find(path);
		if (it != s_AssetPackAssets.end())
		{
			*outData = it->second;
			return true;
		}

		return false;
	}

	std::vector<Ref<Asset>> AssetManager::GetDirtyAssets()
	{
		std::vector<Ref<Asset>> dirty;

		std::scoped_lock lock(s_Mutex);
		for (const auto& [unused, asset] : s_Assets)
			if (asset->IsDirty())
				dirty.push_back(asset);

		return dirty;
	}

	void AssetManager::OnModified(const Ref<Asset>& asset)
	{
		// Callbacks are allowed to add or remove listeners (including themselves) and to destroy other listeners.
		// So, save the IDs, then look each one up again right before calling it,
		std::vector<GUID> ids;
		{
			std::scoped_lock lock(s_CallbacksMutex);
			ids.reserve(s_Callbacks.size());
			for (const auto& [id, _] : s_Callbacks)
				ids.push_back(id);
		}

		for (const GUID& id : ids)
		{
			std::function<void(const Ref<Asset>&)> func;
			{
				std::scoped_lock lock(s_CallbacksMutex);
				auto it = s_Callbacks.find(id);
				if (it == s_Callbacks.end())
					continue;
				func = it->second;
			}
			func(asset);
		}
	}

	void AssetManager::AddOnAssetModifiedCallback(const GUID& id, const std::function<void(const Ref<Asset>&)>& func)
	{
		std::scoped_lock lock(s_CallbacksMutex);
		s_Callbacks[id] = func;
	}

	void AssetManager::RemoveOnAssetModifiedCallback(const GUID& id)
	{
		std::scoped_lock lock(s_CallbacksMutex);
		s_Callbacks.erase(id);
	}
	
	bool AssetManager::Rename(const Ref<Asset>& asset, const Path& filepath)
	{
		if (!asset)
			return false;

		std::error_code error;
		const Path& assetPath = asset->GetPath();
		std::filesystem::rename(assetPath, filepath, error);
		if (error)
		{
			EG_CORE_ERROR("Failed to rename an asset: {}", error.message());
			return false;
		}

		{
			std::scoped_lock lock(s_Mutex);
			s_Assets.erase(assetPath);
			s_Assets.emplace(filepath, asset);
		}
		
		asset->m_Path = filepath;

		return true;
	}
	
	bool AssetManager::Duplicate(const Ref<Asset>& asset, const Path& filepath)
	{
		if (!asset)
			return false;

		std::error_code error;
		const Path& assetPath = asset->GetPath();
		std::filesystem::copy(assetPath, filepath, error);
		if (error)
		{
			EG_CORE_ERROR("Failed to copy an asset: {}", error.message());
			return false;
		}

		if (asset->GetAssetType() == AssetType::Scene)
		{
			GUID newGUID{};
			ScopedDataBuffer data = FileSystem::Read(filepath);
			std::string sceneDesc = Utils::ReadYAMLToString(data);

			// Update GUID
			constexpr char searchKey[] = "GUID:";
			size_t pos = sceneDesc.find(searchKey);
			if (pos != std::string::npos)
			{
				size_t endLinePos = sceneDesc.find_first_of('\n', pos);
				constexpr size_t keySize = sizeof(searchKey) - 1; // -1 to remove '\0'
				sceneDesc.erase(pos + keySize, endLinePos - pos - keySize);
				std::string guidStr = " [" + std::to_string(newGUID.GetHigh()) + ", " + std::to_string(newGUID.GetLow()) + ']';
				sceneDesc.insert(pos + keySize, guidStr);

				if (!SceneSerializer::SerializeWithYaml(filepath, sceneDesc))
				{
					EG_CORE_ERROR("Failed to write to: {}", filepath);
					return false;
				}
				Register(Asset::Create(filepath));
			}
			else
			{
				EG_CORE_ERROR("Failed to duplicate a scene. Couldn't find its GUID. {}", assetPath);
				return false;
			}
		}
		else
		{
			Ref<Asset> newAsset = Asset::Create(filepath);
			newAsset->m_GUID = GUID{}; // Generate a new GUID
			Asset::Save(newAsset); // Save changes
			Register(newAsset);
		}

		return true;
	}

	void AssetManager::Delete(const Ref<Asset>& asset)
	{
		if (!asset)
			return;

		const Path& assetPath = asset->GetPath();
		bool bFound = false;
		{
			std::scoped_lock lock(s_Mutex);
			bFound = s_Assets.find(assetPath) != s_Assets.end();
		}
		if (!bFound)
		{
			EG_CORE_ERROR("Failed to delete an asset: {}. Didn't find it in the asset manager", assetPath);
			return;
		}

		std::error_code error;
		std::filesystem::remove(assetPath, error);
		if (error)
			EG_CORE_ERROR("Failed to delete {}. Error: {}", assetPath, error.message());
		else
		{
			if (asset->GetAssetType() == AssetType::Entity)
			{
				Ref<AssetEntity> entityAsset = Cast<AssetEntity>(asset);
				Entity entity = *(entityAsset->GetEntity());
				AssetEntity::GetScene()->DestroyEntityImmediately(entity, true);
			}

			{
				std::scoped_lock lock(s_Mutex);
				s_Assets.erase(assetPath);
				if (auto guidIt = s_AssetsByGUID.find(asset->GetGUID()); guidIt != s_AssetsByGUID.end() && guidIt->second == asset)
					s_AssetsByGUID.erase(guidIt);
			}
			EG_CORE_TRACE("Deleted asset at: {}", assetPath);
		}
	}
	
	ScopedDataBuffer AssetManager::BuildAssetPack()
	{
		AsyncTaskContext* task = AsyncTaskContext::GetCurrent();

		// Copy the list, so that the lock isn't held while reading files.
		// Because main thread can modify the assets list
		std::vector<std::pair<Path, GUID>> assets;
		{
			std::scoped_lock lock(s_Mutex);
			assets.reserve(s_Assets.size());
			for (const auto& [path, asset] : s_Assets)
				assets.emplace_back(path, asset->GetGUID());
		}

		const size_t assetsCount = assets.size();
		std::vector<ScopedDataBuffer> serializedDatas;
		serializedDatas.reserve(assetsCount);

		size_t totalSize = sizeof(AssetHeader);

		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Assets" << YAML::Value << YAML::BeginSeq;
		for (size_t i = 0; i < assetsCount; ++i)
		{
			const auto& [path, guid] = assets[i];
			if (task)
			{
				if (task->IsCancelRequested())
					return {};

				task->SetDetail(Utils::AsString(path));
				task->SetProgress(i, assetsCount);
			}

			if (std::filesystem::exists(path) == false)
				continue;

			const auto& data = serializedDatas.emplace_back(FileSystem::Read(path));
			const size_t offset = Utils::AddSize(data, &totalSize);

			out << YAML::BeginMap;

			out << YAML::Key << "Path" << YAML::Value << Utils::AsString(path);
			out << YAML::Key << "GUID" << YAML::Value << guid;
			out << YAML::Key << "DataSize" << YAML::Value << data.Size();
			out << YAML::Key << "DataOffset" << YAML::Value << offset;

			out << YAML::EndMap;
		}
		out << YAML::EndSeq;
		out << YAML::EndMap;

		AssetHeader header = Utils::CreateHeader(out, &totalSize);
		ScopedDataBuffer totalData(totalSize);
		
		size_t offset = 0;
		Utils::WriteToBuffer(totalData, &header, sizeof(header), &offset);
		for (const auto& data : serializedDatas)
		{
			Utils::WriteToBuffer(totalData, data, &offset);
		}
		Utils::WriteYaml(totalData, out, &offset);

		return totalData;
	}
}
