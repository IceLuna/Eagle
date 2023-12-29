#pragma once
#include "Asset.h"

namespace Eagle
{
	using AssetsMap = std::unordered_map<Path, Ref<Asset>>;

	class AssetManager
	{
	public:
		static void Init();
		static void Reset();

		static void Register(const Ref<Asset>& asset);
		static bool Exist(const Path& path);
		static bool Get(const Path& path, Ref<Asset>* outAsset);
		static bool Get(const GUID& guid, Ref<Asset>* outAsset); // TODO: Think about it

		static const AssetsMap& GetAssets() { return s_Assets; }

	private:
		static AssetsMap s_Assets;
	};
}
