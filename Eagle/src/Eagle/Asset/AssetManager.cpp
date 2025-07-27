#include "egpch.h"
#include "AssetManager.h"

#include "Eagle/Core/Project.h"
#include "Eagle/Core/Serializer.h"
#include "Eagle/Core/SceneSerializer.h"
#include "Eagle/Core/ThreadPool.h"
#include "Eagle/Utils/Compressor.h"
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

	// Just to store path along side with the node when we use GUID as a key.
	struct NodeData
	{
		YAML::Node Node;
		Path AssetPath;
	};

	// There's no point in loading all assets at the beggining.
	// We should load assets as needed and try to unload them when scenes change.
	// So here we are creating a map, where Path is a path to an asset and YAML::Node is a node that contains asset data.
	// Using this approach will allow us to quickly load assets since there's no need to parse the asset pack.
	// And when an asset is requested, we should check if it's loaded already (s_Assets)
	static std::unordered_map<Path, YAML::Node> s_AssetPackAssets;
	static std::unordered_map<GUID, NodeData> s_AssetPackAssetsByGUID;
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
		std::array<AssetsQueue, 6> assetsToLoadQueue;
		std::vector<Path> entityAssetsToLoad; // Entity assets need to be created in a single thread (mono related issues)
		entityAssetsToLoad.reserve(25);
		for (auto& assets : assetsToLoadQueue)
			assets.Paths.reserve(25);

		const Path contentPath = Project::GetContentPath();
		const Path& projectPath = Project::GetProjectPath();

		const auto& project = Project::GetProjectInfo();
		if (!ScriptEngine::LoadAppAssembly(Project::GetBinariesPath() / (project.Name + ".dll")))
		{
			const std::string error = std::string("Open VS solution (") +
				(project.BasePath / (project.Name + ".sln")).u8string() + " or \"File > Open VS Solution\") and compile the project.\nIf the solution is not there, try to generate it \"File > Generate VS Solution\"";
			EG_CORE_WARN(error);
		}

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
				entityAssetsToLoad.emplace_back(std::move(assetPath));
				continue;
			}

			assetsToLoadQueue[0].Paths.emplace_back(std::move(assetPath));
		}

		std::mutex mutex;
		constexpr bool bEnableAsyncLoading = true;
		const uint32_t threadCount = bEnableAsyncLoading ? std::thread::hardware_concurrency() : 1u;
		ThreadPool threadPool("AssetManager", threadCount, false);

		auto loadAssetFunc = [&mutex](const Path& assetPath)
		{
			EG_CORE_INFO("Loading asset: {}", assetPath.u8string());
			Ref<Asset> asset = Asset::Create(assetPath);
			std::scoped_lock lock(mutex);
			Register(asset);
		};

		for (const auto& assets : assetsToLoadQueue)
		{
			if (assets.bAsync)
			{
				for (const auto& assetPath : assets.Paths)
				{
					threadPool->push_task(loadAssetFunc, assetPath);
				}
				threadPool->wait_for_tasks();
			}
			else
			{
				for (const auto& assetPath : assets.Paths)
				{
					loadAssetFunc(assetPath);
				}
			}
		}

		for (const auto& assetPath : entityAssetsToLoad)
		{
			EG_CORE_INFO("Loading asset: {}", assetPath.u8string());
			Register(Asset::Create(assetPath));
		}

		s_Skybox = AssetTextureCube::Create(Application::GetCorePath() / "assets/textures/IBL.egasset");
		s_Sphere = AssetStaticMesh::Create(Application::GetCorePath() / "assets/meshes/Sphere.egasset");
		s_Cube = AssetStaticMesh::Create(Application::GetCorePath() / "assets/meshes/Cube.egasset");
	}

	void AssetManager::InitGame(const YAML::Node& baseNode)
	{
		s_bGame = Application::Get().IsGame();
		if (!s_bGame || !baseNode)
			return;

		for (auto& baseAssetNode : baseNode)
		{
			const Path path = baseAssetNode["Path"].as<std::string>();
			auto assetNode = baseAssetNode["Asset"];
			const GUID assetGUID = assetNode["GUID"].as<GUID>();

			s_AssetPackAssets.emplace(path, assetNode);
			s_AssetPackAssetsByGUID.emplace(assetGUID, NodeData{ assetNode, path });
		}
	}

	void AssetManager::Reset()
	{
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
		s_Assets.clear();
		s_AssetsByGUID.clear();
	}

	void AssetManager::ResetRuntimeAsset()
	{
		s_RuntimeAssets.clear();
	}

	void AssetManager::Register(const Ref<Asset>& asset)
	{
		if (asset)
		{
			s_Assets.emplace(asset->GetPath(), asset);
			s_AssetsByGUID.emplace(asset->GetGUID(), asset);
		}
	}

	void AssetManager::AddRuntimeAsset(const Ref<Asset>& asset)
	{
		if (asset)
		{
			s_RuntimeAssets.emplace(asset->GetGUID(), asset);
		}
	}
	
	bool AssetManager::Get(const Path& path, Ref<Asset>* outAsset)
	{
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
				const auto& assetNode = it->second;

				AssetType assetType = AssetType::None;
				auto typeNode = assetNode["Type"];
				if (!typeNode)
				{
					EG_CORE_ERROR("Failed to load an asset. It's not an Eagle asset: {}", path.u8string());
					return false;
				}
				EG_CORE_INFO("Loading asset: {}", path.u8string());
				
				assetType = Utils::GetEnumFromName<AssetType>(typeNode.as<std::string>());
				if (assetType == AssetType::Scene)
					*outAsset = AssetScene::Create(path, assetNode);
				else
					*outAsset = Serializer::DeserializeAsset(assetNode, path, false);

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
				const NodeData& assetNodeData = it->second;

				AssetType assetType = AssetType::None;
				auto typeNode = assetNodeData.Node["Type"];
				if (!typeNode)
				{
					EG_CORE_ERROR("Failed to load an asset. It's not an eagle asset");
					return false;
				}
				EG_CORE_INFO("Loading asset: {}", assetNodeData.AssetPath.u8string());

				assetType = Utils::GetEnumFromName<AssetType>(typeNode.as<std::string>());
				if (assetType == AssetType::Scene)
					*outAsset = AssetScene::Create(assetNodeData.AssetPath, assetNodeData.Node);
				else
					*outAsset = Serializer::DeserializeAsset(assetNodeData.Node, assetNodeData.AssetPath, false);

				Register(*outAsset);
				return true;
			}
		}

		return false;
	}

	bool AssetManager::GetRuntimeAssetNode(const Path& path, YAML::Node* outNode)
	{
		if (!s_bGame)
			return false;

		auto it = s_AssetPackAssets.find(path);
		if (it != s_AssetPackAssets.end())
		{
			*outNode = it->second;
			return true;
		}

		return false;
	}

	std::vector<Ref<Asset>> AssetManager::GetDirtyAssets()
	{
		std::vector<Ref<Asset>> dirty;

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
			std::string sceneDesc = FileSystem::ReadText(filepath);

			// Update GUID
			constexpr char searchKey[] = "GUID:";
			size_t pos = sceneDesc.find(searchKey);
			size_t endLinePos = sceneDesc.find_first_of('\n', pos);
			if (pos != std::string::npos)
			{
				constexpr size_t keySize = sizeof(searchKey) - 1; // -1 to remove '\0'
				sceneDesc.erase(pos + keySize, endLinePos - pos - keySize);
				std::string guidStr = " [" + std::to_string(newGUID.GetHigh()) + ", " + std::to_string(newGUID.GetLow()) + ']';
				sceneDesc.insert(pos + keySize, guidStr);
			}

			std::ofstream fout(filepath);
			fout << sceneDesc;
			fout.close();
			Register(Asset::Create(filepath));
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
			EG_CORE_ERROR("Failed to delete an asset: {}. Didn't find it in the asset manager", assetPath.u8string());
			return;
		}

		std::error_code error;
		std::filesystem::remove(assetPath, error);
		if (error)
			EG_CORE_ERROR("Failed to delete {}. Error: {}", assetPath.u8string(), error.message());
		else
		{
			s_Assets.erase(it);
			s_AssetsByGUID.erase(asset->GetGUID());
			EG_CORE_TRACE("Deleted asset at: {}", assetPath.u8string());
		}
	}
	
	bool AssetManager::BuildAssetPack(YAML::Emitter& out)
	{
		out << YAML::Key << "Assets" << YAML::Value << YAML::BeginSeq;

		for (const auto& [path, asset] : s_Assets)
		{
			out << YAML::BeginMap;
			out << YAML::Key << "Path" << YAML::Value << path.string();
			out << YAML::Key << "Asset" << YAML::Value;
			if (asset->GetAssetType() == AssetType::Scene)
			{
				Ref<Scene> scene = MakeRef<Scene>();
				SceneSerializer serializer(scene);
				// First, we need to deserialize a scene to fill it up
				serializer.Deserialize(path);
				// Then the scene is serialized
				serializer.Serialize(out);
			}
			else
			{
				Serializer::SerializeAsset(out, asset);
			}
			out << YAML::EndMap;
		}
		out << YAML::EndSeq;

		return true;
	}
}
