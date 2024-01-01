#include "egpch.h"
#include "Asset.h"
#include "AssetImporter.h"

#include "Eagle/Renderer/MaterialSystem.h"
#include "Eagle/Core/Serializer.h"
#include "Eagle/Utils/Utils.h"
#include "Eagle/Utils/YamlUtils.h"

#include <stb_image.h>

namespace Eagle
{
	Ref<Asset> Asset::Create(const Path& path)
	{
		if (!std::filesystem::exists(path))
		{
			EG_CORE_ERROR("Failed to load an asset. It doesn't exist: {}", path);
			return {};
		}

		AssetType type = AssetType::None;

		// Read the asset type
		{
			YAML::Node data = YAML::LoadFile(path.string());
			if (auto node = data["Type"])
				type = Utils::GetEnumFromName<AssetType>(node.as<std::string>());
		}

		if (type == AssetType::None)
		{
			EG_CORE_ERROR("Failed to load an asset. Unknown asset type: {}", path);
			return {};
		}

		switch (type)
		{
			case AssetType::Texture2D: return AssetTexture2D::Create(path);
			case AssetType::TextureCube: return AssetTextureCube::Create(path);
			case AssetType::Material: return AssetMaterial::Create(path);
		}

		EG_CORE_ASSERT(!"Unknown type");
		return {};
	}

	void Asset::Save(const Ref<Asset>& asset)
	{
		YAML::Emitter out;
		Serializer::SerializeAsset(out, asset);

		std::ofstream fout(asset->GetPath());
		fout << out.c_str();
	}

	void Asset::Reload(Ref<Asset>& asset, bool bReloadRawData)
	{
		const Path& assetPath = asset->GetPath();
		YAML::Node data = YAML::LoadFile(assetPath.string());
		Ref<Asset> reloaded = Serializer::DeserializeAsset(data, assetPath, bReloadRawData);

		if (reloaded)
		{
			if (bReloadRawData)
			{
				// TODO: Mark an asset as dirty
			}

			Asset& reloadedRaw = *reloaded.get();
			*asset = std::move(reloadedRaw);

			const AssetType assetType = asset->GetAssetType();
			// Checking for mesh is a hack, because, currently, triggering materials update is enough to trigger geometry update
			if (assetType == AssetType::Texture2D || assetType == AssetType::Material || assetType == AssetType::Mesh)
				MaterialSystem::SetDirty();
		}
	}

	Ref<AssetTexture2D> AssetTexture2D::Create(const Path& path)
	{
		if (!std::filesystem::exists(path))
		{
			EG_CORE_ERROR("Failed to load an asset. It doesn't exist: {}", path);
			return {};
		}

		YAML::Node data = YAML::LoadFile(path.string());
		return Serializer::DeserializeAssetTexture2D(data, path);
	}
	
	Ref<AssetTextureCube> AssetTextureCube::Create(const Path& path)
	{
		if (!std::filesystem::exists(path))
		{
			EG_CORE_ERROR("Failed to load an asset. It doesn't exist: {}", path);
			return {};
		}

		YAML::Node data = YAML::LoadFile(path.string());
		return Serializer::DeserializeAssetTextureCube(data, path);
	}
	
	Ref<AssetMaterial> AssetMaterial::Create(const Path& path)
	{
		if (!std::filesystem::exists(path))
		{
			EG_CORE_ERROR("Failed to load an asset. It doesn't exist: {}", path);
			return {};
		}

		YAML::Node data = YAML::LoadFile(path.string());
		return Serializer::DeserializeAssetMaterial(data, path);
	}
}
