#include "egpch.h"
#include "Asset.h"
#include "AssetImporter.h"

#include "Eagle/Renderer/MaterialSystem.h"
#include "Eagle/Core/Entity.h"
#include "Eagle/Core/Scene.h"
#include "Eagle/Core/Serializer.h"
#include "Eagle/Audio/Sound.h"
#include "Eagle/Audio/SoundGroup.h"
#include "Eagle/Utils/Utils.h"
#include "Eagle/Utils/YamlUtils.h"

#include <stb_image.h>

namespace Eagle
{
	Ref<Scene> AssetEntity::s_EntityAssetsScene;

	Asset::Asset(const Path& path, const Path& pathToRaw, AssetType type, GUID guid, const DataBuffer& rawData)
		: m_Path(path), m_PathToRaw(pathToRaw), m_GUID(guid), m_Type(type)
	{
		if (rawData.Size)
		{
			m_RawData.Allocate(rawData.Size);
			m_RawData.Write(rawData.Data, rawData.Size);
		}
	}

	AssetTexture2D::AssetTexture2D(const Path& path, const Path& pathToRaw, GUID guid, const DataBuffer& rawData, const DataBuffer& ktxData,
		const Ref<Texture2D>& texture, AssetTexture2DFormat format, bool bCompressed, bool bNormalMap, bool bNeedAlpha)
	: Asset(path, pathToRaw, AssetType::Texture2D, guid, rawData), m_Texture(texture),
		m_Format(format), bCompressed(bCompressed), bNormalMap(bNormalMap), bNeedAlpha(bNeedAlpha)
	{
		if (ktxData.Size)
		{
			m_KtxData.Allocate(ktxData.Size);
			m_KtxData.Write(ktxData.Data, ktxData.Size);
		}
	}

	Ref<Asset> Asset::Create(const Path& path)
	{
		if (!std::filesystem::exists(path))
		{
			EG_CORE_ERROR("Failed to load an asset. It doesn't exist: {}", path.u8string());
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
			EG_CORE_ERROR("Failed to load an asset. Unknown asset type: {}", path.u8string());
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
			case AssetType::SoundGroup: return AssetSoundGroup::Create(path);
			case AssetType::Entity: return AssetEntity::Create(path);
			case AssetType::Scene: return AssetScene::Create(path);
		}

		EG_CORE_ASSERT(!"Unknown type");
		return {};
	}

	void Asset::Save(const Ref<Asset>& asset)
	{
		if (asset->GetAssetType() == AssetType::Scene)
		{
			EG_CORE_ERROR("Error saving an asset. Saving scene assets is not supported!");
			return;
		}

		YAML::Emitter out;
		Serializer::SerializeAsset(out, asset);

		std::ofstream fout(asset->GetPath());
		fout << out.c_str();

		asset->SetDirty(false);
	}

	void Asset::Reload(Ref<Asset>& asset, bool bReloadRawData)
	{
		const AssetType assetType = asset->GetAssetType();
		const Path& assetPath = asset->GetPath();

		if (assetType == AssetType::Scene)
		{
			EG_CORE_ERROR("Reloading scene assets is not supported! {}", assetPath);
			return;
		}

		YAML::Node data = YAML::LoadFile(assetPath.string());
		Ref<Asset> reloaded = Serializer::DeserializeAsset(data, assetPath, bReloadRawData);

		if (!reloaded)
			return;

		if (bReloadRawData)
			asset->SetDirty(true);

		Asset& reloadedRaw = *reloaded.get();
		*asset = std::move(reloadedRaw);

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

	Ref<AssetTexture2D> AssetTexture2D::Create(const Path& path)
	{
		if (!std::filesystem::exists(path))
		{
			EG_CORE_ERROR("Failed to load an asset. It doesn't exist: {}", path.u8string());
			return {};
		}

		YAML::Node data = YAML::LoadFile(path.string());
		return Serializer::DeserializeAssetTexture2D(data, path);
	}
	
	Ref<AssetTextureCube> AssetTextureCube::Create(const Path& path)
	{
		if (!std::filesystem::exists(path))
		{
			EG_CORE_ERROR("Failed to load an asset. It doesn't exist: {}", path.u8string());
			return {};
		}

		YAML::Node data = YAML::LoadFile(path.string());
		return Serializer::DeserializeAssetTextureCube(data, path);
	}
	
	Ref<AssetMaterial> AssetMaterial::Create(const Path& path)
	{
		if (!std::filesystem::exists(path))
		{
			EG_CORE_ERROR("Failed to load an asset. It doesn't exist: {}", path.u8string());
			return {};
		}

		YAML::Node data = YAML::LoadFile(path.string());
		return Serializer::DeserializeAssetMaterial(data, path);
	}
	
	Ref<AssetMesh> AssetMesh::Create(const Path& path)
	{
		if (!std::filesystem::exists(path))
		{
			EG_CORE_ERROR("Failed to load an asset. It doesn't exist: {}", path.u8string());
			return {};
		}

		YAML::Node data = YAML::LoadFile(path.string());
		return Serializer::DeserializeAssetMesh(data, path);
	}
	
	void AssetAudio::SetSoundGroupAsset(const Ref<AssetSoundGroup>& soundGroup)
	{
		m_SoundGroup = soundGroup;
		if (m_SoundGroup)
			m_Audio->SetSoundGroup(m_SoundGroup->GetSoundGroup());
		else
			m_Audio->SetSoundGroup(SoundGroup::GetMasterGroup());
	}

	Ref<AssetAudio> AssetAudio::Create(const Path& path)
	{
		if (!std::filesystem::exists(path))
		{
			EG_CORE_ERROR("Failed to load an asset. It doesn't exist: {}", path.u8string());
			return {};
		}

		YAML::Node data = YAML::LoadFile(path.string());
		return Serializer::DeserializeAssetAudio(data, path);
	}

	Ref<AssetFont> AssetFont::Create(const Path& path)
	{
		if (!std::filesystem::exists(path))
		{
			EG_CORE_ERROR("Failed to load an asset. It doesn't exist: {}", path.u8string());
			return {};
		}

		YAML::Node data = YAML::LoadFile(path.string());
		return Serializer::DeserializeAssetFont(data, path);
	}
	
	Ref<AssetPhysicsMaterial> AssetPhysicsMaterial::Create(const Path& path)
	{
		if (!std::filesystem::exists(path))
		{
			EG_CORE_ERROR("Failed to load an asset. It doesn't exist: {}", path.u8string());
			return {};
		}

		YAML::Node data = YAML::LoadFile(path.string());
		return Serializer::DeserializeAssetPhysicsMaterial(data, path);
	}
	
	Ref<AssetSoundGroup> AssetSoundGroup::Create(const Path& path)
	{
		if (!std::filesystem::exists(path))
		{
			EG_CORE_ERROR("Failed to load an asset. It doesn't exist: {}", path.u8string());
			return {};
		}

		YAML::Node data = YAML::LoadFile(path.string());
		return Serializer::DeserializeAssetSoundGroup(data, path);
	}
	
	Ref<AssetEntity> AssetEntity::Create(const Path& path)
	{
		if (!std::filesystem::exists(path))
		{
			EG_CORE_ERROR("Failed to load an asset. It doesn't exist: {}", path.u8string());
			return {};
		}

		YAML::Node data = YAML::LoadFile(path.string());
		return Serializer::DeserializeAssetEntity(data, path);
	}
	
	Entity AssetEntity::CreateEntity(GUID guid)
	{
		return s_EntityAssetsScene->CreateEntityWithGUID(guid);
	}
	
	Ref<AssetScene> AssetScene::Create(const Path& path)
	{
		if (!std::filesystem::exists(path))
		{
			EG_CORE_ERROR("Failed to load an asset. It doesn't exist: {}", path.u8string());
			return {};
		}

		YAML::Node data = YAML::LoadFile(path.string());
		return AssetScene::Create(path, data);
	}

	Ref<AssetScene> AssetScene::Create(const Path& path, const YAML::Node& data)
	{
		auto node = data["GUID"];

		if (!node)
		{
			EG_CORE_ERROR("Failed to load a scene. Invalid format: {}", path.u8string());
			return {};
		}

		class LocalAssetScene: public AssetScene
		{
		public:
			LocalAssetScene(const Path& path, GUID guid)
				: AssetScene(path, guid) {}
		};

		return MakeRef<LocalAssetScene>(path, node.as<GUID>());
	}
}
