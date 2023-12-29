#include "egpch.h"
#include "AssetManager.h"
#include "Eagle/Core/Project.h"

namespace Eagle
{
	namespace Utils
	{
		static bool IsAssetExtension(const Path& filepath)
		{
			if (!filepath.has_extension())
				return false;

			static const std::locale& loc = std::locale("RU_ru");
			std::string extension = filepath.extension().u8string();

			for (char& c : extension)
				c = std::tolower(c, loc);

			return extension == Asset::GetExtension();
		}
	}

	AssetsMap AssetManager::s_Assets;
	
	void AssetManager::Init()
	{
		const Path contentPath = Project::GetContentPath();
		for (auto& dirEntry : std::filesystem::recursive_directory_iterator(contentPath))
		{
			if (dirEntry.is_directory())
				continue;

			Path assetPath = dirEntry.path();
			if (!Utils::IsAssetExtension(assetPath))
				continue;

			Register(Asset::Create(assetPath));
		}
	}

	void AssetManager::Reset()
	{
		s_Assets.clear();
	}

	void AssetManager::Register(const Ref<Asset>& asset)
	{
		if (asset)
			s_Assets.emplace(asset->GetPath(), asset);
	}
	
	bool AssetManager::Exist(const Path& path)
	{
		return s_Assets.find(path) != s_Assets.end();
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

		for (const auto& [unused, asset] : s_Assets)
		{
			if (guid == asset->GetGUID())
			{
				*outAsset = asset;
				return true;
			}
		}

		return false;
	}
}
