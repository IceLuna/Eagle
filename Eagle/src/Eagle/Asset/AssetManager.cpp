#include "egpch.h"
#include "AssetManager.h"

#include "Eagle/Core/Project.h"
#include "Eagle/Core/Serializer.h"
#include "Eagle/Core/SceneSerializer.h"
#include "Eagle/Utils/Compressor.h"

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

		std::vector<Path> delayedAssets;
		delayedAssets.reserve(25);

		std::vector<Path> delayedAssetsLastly;
		delayedAssetsLastly.reserve(25);

		const Path contentPath = Project::GetContentPath();
		const Path& projectPath = Project::GetProjectPath();
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
			// Entity: we can't load entities unless all assets are loaded since entities might refer to anything
			if (type == AssetType::Material || type == AssetType::Audio)
			{
				delayedAssets.emplace_back(std::move(assetPath));
				continue;
			}
			else if (type == AssetType::Entity)
			{
				delayedAssetsLastly.emplace_back(std::move(assetPath));
				continue;
			}

			Register(Asset::Create(assetPath));
		}

		for (const auto& assetPath : delayedAssets)
			Register(Asset::Create(assetPath));

		for (const auto& assetPath : delayedAssetsLastly)
			Register(Asset::Create(assetPath));
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
		s_Assets.clear();
		s_AssetsByGUID.clear();
		s_AssetPackAssets.clear();
		s_AssetPackAssetsByGUID.clear();
	}

	void AssetManager::Register(const Ref<Asset>& asset)
	{
		if (asset)
		{
			s_Assets.emplace(asset->GetPath(), asset);
			s_AssetsByGUID.emplace(asset->GetGUID(), asset);
		}
	}
	
	bool AssetManager::Exist(const Path& path)
	{
		return s_Assets.find(path) != s_Assets.end();
	}

	bool AssetManager::Exist(const GUID& guid)
	{
		return s_AssetsByGUID.find(guid) != s_AssetsByGUID.end();
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
					EG_CORE_ERROR("Failed to load an asset. It's not an eagle asset");
					return false;
				}
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
