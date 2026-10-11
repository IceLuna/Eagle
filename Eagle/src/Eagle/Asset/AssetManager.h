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
	using AssetsMap = std::map<Path, Ref<Asset>>;
	using AssetsMapByGUID = ankerl::unordered_dense::map<GUID, Ref<Asset>>;

	// While a staging scope is alive, `AssetManager::Register` calls made on the same thread don't touch the global asset maps.
	// The assets are staged instead, and they're registered later on the main thread.
	// Why: the main thread iterates `AssetManager::GetAssets()` without locking (content browser, asset pickers, etc.),
	// so a background thread must never insert into it.
	// Scopes can't be nested.
	class AssetStagingScope
	{
	public:
		AssetStagingScope();
		~AssetStagingScope();

		AssetStagingScope(const AssetStagingScope&) = delete;
		AssetStagingScope& operator=(const AssetStagingScope&) = delete;

		std::vector<Ref<Asset>> Flush() { return std::move(m_Pending); }

		void Clear()
		{
			m_Pending.clear();
			m_ByGUID.clear();
			m_ByPath.clear();
		}

	private:
		AssetsMap m_ByPath;
		AssetsMapByGUID m_ByGUID;
		std::vector<Ref<Asset>> m_Pending;

		friend class AssetManager;
	};

	class AssetManager
	{
	public:
		static void Init();
		static void InitGame(const DataBuffer& assetPack);
		static void Reset();
		static void ResetGameAssets();
		static void ResetRuntimeAsset();

		// Registers an asset so it can be found by its path and GUID.
		// Returns the instance that ends up in the manager: `asset` itself, or the instance that was already registered at the same path.
		// Callers that go on to use the asset should use the returned instance, so everyone shares a single object.
		// Returns null if `asset` is null.
		static Ref<Asset> Register(const Ref<Asset>& asset);
		static void AddRuntimeAsset(const Ref<Asset>& asset); // Created by C#
		static bool Get(const Path& path, Ref<Asset>* outAsset);
		static bool Get(const GUID& guid, Ref<Asset>* outAsset);
		static bool Exists(const Path& path);
		static bool GetRuntimeAssetData(const Path& path, Ref<ScopedDataBuffer>* outData);
		static bool IsLoaded(const GUID& guid);

		// Some asset types can only be created on the main thread (entity assets)
		static bool RequiresMainThread(AssetType type);

		struct AssetPackDependency
		{
			GUID ID;
			size_t DataSize = 0; // Size of the asset's data in the asset pack
			bool bRequiresMainThread = false;
		};

		// Game builds only (thread safe)
		// Returns all assets of the asset pack that `node` references (including indirectly through other assets)
		// They're ordered so that an asset comes after the assets it references, so loading them in this order loads one asset at a time.
		// Note: Scene assets are ignored. Also finds references in C# public fields
		static std::vector<AssetPackDependency> GatherAssetPackDependencies(const YAML::Node& node);

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

		// Finds everything in `node` that looks like a GUID.
		// That's how assets reference other assets (including C# public fields). Not everything found is a GUID of an asset, so the results must be checked
		static void CollectGUIDs(const YAML::Node& node, std::vector<GUID>& outGUIDs);

		// Removes an asset from the project without deleting its file: the file is moved into `destinationFolder`, keeping its path.
		// For example, "Content/Textures/a.egasset" is moved to "<destinationFolder>/Content/Textures/a.egasset".
		// It can be restored by moving the file back and reopening the project
		static bool MoveOutOfProject(const Ref<Asset>& asset, const Path& destinationFolder);

		static const AssetsMap& GetAssets() { return s_Assets; }

		// @excludedAssets. Optional. Assets that aren't packed (see `AssetUsage`)
		static ScopedDataBuffer BuildAssetPack(const ankerl::unordered_dense::set<GUID>* excludedAssets = nullptr);

		static const char* GetAssetPackExtension() { return ".egpack"; }

		static const Ref<AssetTextureCube>& GetPreviewSkybox() { return s_Skybox; }
		static const Ref<AssetStaticMesh>& GetPreviewSphere() { return s_Sphere; }
		static const Ref<AssetStaticMesh>& GetPreviewCube() { return s_Cube; }

	private:
		// Same as `Register`, but expects `s_Mutex` to be locked by the caller. `asset` must not be null
		static Ref<Asset> Register_Internal(const Ref<Asset>& asset);

		static void Unregister_Internal(const Ref<Asset>& asset);

		// Game only. Deserializes an asset from the asset pack and registers it.
		// If another thread is already loading the same asset, waits for it and returns its result instead of loading it twice.
		// Returns null on failure
		static Ref<Asset> LoadFromAssetPack(const Path& path, const Ref<ScopedDataBuffer>& assetData);

	private:
		static AssetsMap s_Assets;
		static AssetsMapByGUID s_AssetsByGUID;
		static ankerl::unordered_dense::map<GUID, std::function<void(const Ref<Asset>&)>> s_Callbacks;

		// Used to store assets that are created at runtime (using C#). They're deleted once the scene is closed
		static AssetsMapByGUID s_RuntimeAssets;

		// Engine-only assets that's used for asset previews
		static Ref<AssetTextureCube> s_Skybox;
		static Ref<AssetStaticMesh> s_Sphere;
		static Ref<AssetStaticMesh> s_Cube;
	};
}
