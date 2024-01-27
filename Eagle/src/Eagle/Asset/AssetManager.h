#pragma once

#include "Asset.h"
#include "Eagle/Core/GUID.h"

namespace YAML
{
	class Emitter;
	class Node;
}

namespace Eagle
{
	using AssetsMap = std::unordered_map<Path, Ref<Asset>>;
	using AssetsMapByGUID = std::unordered_map<GUID, Ref<Asset>>;

	class AssetManager
	{
	public:
		static void Init();
		static void InitGame(const YAML::Node& assetNode);
		static void Reset();

		static void Register(const Ref<Asset>& asset);
		static bool Exist(const Path& path);
		static bool Exist(const GUID& guid);
		static bool Get(const Path& path, Ref<Asset>* outAsset);
		static bool Get(const GUID& guid, Ref<Asset>* outAsset);
		static bool GetRuntimeAssetNode(const Path& path, YAML::Node* outNode);

		// Can be used to rename or move an asset file.
		// Example: from "Content/texture.egasset" to "Content/texture2.egasset", or to "Content/Textures/texture.egasset"
		// Note: this DOES change the original asset-file
		// @filepath. Where to put the new asset. In the example above, it would be "Content/texture2.egasset", or "Content/Textures/texture.egasset"
		static bool Rename(const Ref<Asset>& asset, const Path& filepath);

		// Can be used to duplicate/copy the asset file.
		// Example: "Content/texture.egasset" to "Content/texture2.egasset", or to "Content/Textures/texture.egasset"
		// Note: this DOES NOT change the original asset-file
		// @filepath. Where to put the new asset. Example: "Content/texture_duplicated.egasset"
		static bool Duplicate(const Ref<Asset>& asset, const Path& filepath);

		static const AssetsMap& GetAssets() { return s_Assets; }

		static bool BuildAssetPack(YAML::Emitter& out);

		static const char* GetAssetPackExtension() { return ".egpack"; }

	private:
		static AssetsMap s_Assets;
		static AssetsMapByGUID s_AssetsByGUID;
	};
}
