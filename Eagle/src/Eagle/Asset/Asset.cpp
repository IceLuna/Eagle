#include "egpch.h"
#include "Asset.h"
#include "AssetImporter.h"

#include "Eagle/Renderer/MaterialSystem.h"
#include "Eagle/Core/Scene.h"
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
			case AssetType::Mesh: return AssetMesh::Create(path);
			case AssetType::Audio: return AssetAudio::Create(path);
			case AssetType::Font: return AssetFont::Create(path);
			case AssetType::Material: return AssetMaterial::Create(path);
			case AssetType::PhysicsMaterial: return AssetPhysicsMaterial::Create(path);
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
			if (assetType == AssetType::Texture2D || assetType == AssetType::Material)
				MaterialSystem::SetDirty();
			else if (assetType == AssetType::Mesh)
			{
				if (auto& scene = Scene::GetCurrentScene())
					scene->SetMeshesDirty(true);
			}
			else if (assetType == AssetType::Font)
			{
				if (auto& scene = Scene::GetCurrentScene())
					scene->SetTextsDirty(true);
			}
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
	
	Ref<AssetMesh> AssetMesh::Create(const Path& path)
	{
		if (!std::filesystem::exists(path))
		{
			EG_CORE_ERROR("Failed to load an asset. It doesn't exist: {}", path);
			return {};
		}

		YAML::Node data = YAML::LoadFile(path.string());
		return Serializer::DeserializeAssetMesh(data, path);
	}
	
	Ref<AssetAudio> AssetAudio::Create(const Path& path)
	{
		if (!std::filesystem::exists(path))
		{
			EG_CORE_ERROR("Failed to load an asset. It doesn't exist: {}", path);
			return {};
		}

		YAML::Node data = YAML::LoadFile(path.string());
		return Serializer::DeserializeAssetAudio(data, path);
	}

	Ref<AssetFont> AssetFont::Create(const Path& path)
	{
		if (!std::filesystem::exists(path))
		{
			EG_CORE_ERROR("Failed to load an asset. It doesn't exist: {}", path);
			return {};
		}

		YAML::Node data = YAML::LoadFile(path.string());
		return Serializer::DeserializeAssetFont(data, path);
	}
}
