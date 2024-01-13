#include "egpch.h"
#include "AssetManager.h"

#include "Eagle/Core/Project.h"
#include "Eagle/Core/Serializer.h"

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

	void AssetManager::Init()
	{
		AssetEntity::s_EntityAssetsScene = MakeRef<Scene>();

		std::vector<Path> delayedAssets;
		delayedAssets.reserve(25);

		std::vector<Path> delayedAssetsLastly;
		delayedAssetsLastly.reserve(25);

		const Path contentPath = Project::GetContentPath();
		for (auto& dirEntry : std::filesystem::recursive_directory_iterator(contentPath))
		{
			if (dirEntry.is_directory())
				continue;

			Path assetPath = dirEntry.path();
			if (!Utils::IsAssetExtension(assetPath))
				continue;

			const AssetType type = Serializer::GetAssetType(assetPath);

			// We deffer the loading of some assets:
			// Materials: we can't load materials unless all textures are loaded since materials refer to them
			// Audio: we can't load audios unless all sound groups are loaded since audios refer to them
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

	void AssetManager::Reset()
	{
		AssetEntity::s_EntityAssetsScene.reset();
		s_Assets.clear();
		s_AssetsByGUID.clear();
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

		return false;
	}
}
