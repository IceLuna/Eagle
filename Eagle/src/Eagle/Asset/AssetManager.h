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
		static void ResetGameAssets();

		static void Register(const Ref<Asset>& asset);
		static bool Exist(const Path& path);
		static bool Exist(const GUID& guid);
		static bool Get(const Path& path, Ref<Asset>* outAsset);
		static bool Get(const GUID& guid, Ref<Asset>* outAsset);
		static bool GetRuntimeAssetNode(const Path& path, YAML::Node* outNode);
		static std::vector<Ref<Asset>> GetDirtyAssets();

		static void OnModified(const Ref<Asset>& asset);
		static void AddOnAssetModifiedCallback(const GUID& id, const std::function<void(const Ref<Asset>&)>& func);
		static void RemoveOnAssetModifiedCallback(const GUID& id);

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

		static void Delete(const Ref<Asset>& asset);

		static const AssetsMap& GetAssets() { return s_Assets; }

		static bool BuildAssetPack(YAML::Emitter& out);

		static const char* GetAssetPackExtension() { return ".egpack"; }

		static const Ref<AssetTextureCube>& GetPreviewSkybox() { return s_Skybox; }
		static const Ref<AssetStaticMesh>& GetPreviewSphere() { return s_Sphere; }
		static const Ref<AssetStaticMesh>& GetPreviewCube() { return s_Cube; }

	private:
		static AssetsMap s_Assets;
		static AssetsMapByGUID s_AssetsByGUID;
		static std::unordered_map<GUID, std::function<void(const Ref<Asset>&)>> s_Callbacks;

		// Engine-only assets that's used for asset previews
		static Ref<AssetTextureCube> s_Skybox;
		static Ref<AssetStaticMesh> s_Sphere;
		static Ref<AssetStaticMesh> s_Cube;
	};
}
