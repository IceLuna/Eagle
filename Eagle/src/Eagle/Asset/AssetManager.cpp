#include "egpch.h"
#include "AssetManager.h"

#include "Eagle/Core/Project.h"
#include "Eagle/Core/Serializer.h"
#include "Eagle/Core/SceneSerializer.h"
#include "Eagle/Core/ThreadPool.h"
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
	std::unordered_map<GUID, std::function<void(const Ref<Asset>&)>> AssetManager::s_Callbacks;
	AssetsMapByGUID AssetManager::s_RuntimeAssets;
	Ref<AssetTextureCube> AssetManager::s_Skybox;
	Ref<AssetStaticMesh> AssetManager::s_Sphere;
	Ref<AssetStaticMesh> AssetManager::s_Cube;

	// There's no point in loading all assets at the beggining.
	// We should load assets as needed and try to unload them when scenes change.
	// So here we are creating a map, where Path is a path to an asset and ScopedDataBuffer is a buffer that contains asset data.
	// Using this approach will allow us to quickly load assets since there's no need to parse the asset pack.
	// And when an asset is requested, we should check if it's loaded already (s_Assets)
	static std::unordered_map<Path, Ref<ScopedDataBuffer>> s_AssetPackAssets;
	static std::unordered_map<GUID, std::pair<Path, Ref<ScopedDataBuffer>>> s_AssetPackAssetsByGUID;
	static std::mutex s_Mutex;
	static bool s_bGame = false;

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
				(project.BasePath / (project.Name + ".sln")).string() + " or \"File > Open VS Solution\") and compile the project.\nIf the solution is not there, try to generate it \"File > Generate VS Solution\"";
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
					threadPool->push_task(loadAssetFunc, assetPath, true);
				}
				threadPool->wait_for_tasks();
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
		std::scoped_lock lock(s_Mutex);

		AssetEntity::s_EntityAssetsScene.reset();
		s_Callbacks.clear();
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

	void AssetManager::Register(const Ref<Asset>& asset)
	{
		if (asset)
		{
			std::scoped_lock lock(s_Mutex);
			s_Assets.emplace(asset->GetPath(), asset);
			s_AssetsByGUID.emplace(asset->GetGUID(), asset);
		}
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
		std::scoped_lock lock(s_Mutex);

		auto it = s_Assets.find(path);
		if (it != s_Assets.end())
		{
			*outAsset = it->second;
			return true;
		}

		// Try to load it
		if (s_bGame)
		{
			auto it = s_AssetPackAssets.find(path);
			if (it != s_AssetPackAssets.end())
			{
				Timer timer;
				const auto& assetData = it->second;
				*outAsset = Serializer::DeserializeAsset(assetData->GetDataBuffer(), path, false);
				EG_CORE_INFO("Loaded asset in {}s: {}", timer.GetSeconds(), path);

				Register(*outAsset);
				return true;
			}
		}

		return false;
	}
	
	bool AssetManager::Get(const GUID& guid, Ref<Asset>* outAsset)
	{
		if (guid.IsNull())
			return false;

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

		// Try to load it
		if (s_bGame)
		{
			auto it = s_AssetPackAssetsByGUID.find(guid);
			if (it != s_AssetPackAssetsByGUID.end())
			{
				const std::pair<Path, Ref<ScopedDataBuffer>>& assetInfo = it->second;
				const auto& assetPath = assetInfo.first;
				const auto& assetData = assetInfo.second;

				Timer timer;
				*outAsset = Serializer::DeserializeAsset(assetData->GetDataBuffer(), assetPath, false);
				EG_CORE_INFO("Loaded asset in {}s: {}", timer.GetSeconds(), assetPath);

				Register(*outAsset);
				return true;
			}
		}

		return false;
	}

	bool AssetManager::Exists(const Path& path)
	{
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
		for (auto& [_, func] : s_Callbacks)
			func(asset);
	}

	void AssetManager::AddOnAssetModifiedCallback(const GUID& id, const std::function<void(const Ref<Asset>&)>& func)
	{
		s_Callbacks[id] = func;
	}

	void AssetManager::RemoveOnAssetModifiedCallback(const GUID& id)
	{
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

		s_Assets.erase(assetPath);
		s_Assets.emplace(filepath, asset);
		
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
		auto it = s_Assets.find(assetPath);
		if (it == s_Assets.end())
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
			s_Assets.erase(it);
			s_AssetsByGUID.erase(asset->GetGUID());
			EG_CORE_TRACE("Deleted asset at: {}", assetPath);
		}
	}
	
	ScopedDataBuffer AssetManager::BuildAssetPack()
	{
		std::vector<ScopedDataBuffer> serializedDatas;
		serializedDatas.reserve(s_Assets.size());

		size_t totalSize = sizeof(AssetHeader);

		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Assets" << YAML::Value << YAML::BeginSeq;
		for (const auto& [path, asset] : s_Assets)
		{
			if (std::filesystem::exists(path) == false)
				continue;

			const auto& data = serializedDatas.emplace_back(FileSystem::Read(path));
			const size_t offset = Utils::AddSize(data, &totalSize);

			out << YAML::BeginMap;

			out << YAML::Key << "Path" << YAML::Value << path.string();
			out << YAML::Key << "GUID" << YAML::Value << asset->GetGUID();
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
