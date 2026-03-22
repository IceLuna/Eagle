#include "egpch.h"
#include "Asset.h"
#include "AssetImporter.h"
#include "AssetManager.h"

#include "Eagle/Renderer/MaterialSystem.h"
#include "Eagle/Renderer/TextureCompressor.h"
#include "Eagle/Renderer/VidWrappers/Texture.h"

#include "Eagle/Core/Entity.h"
#include "Eagle/Core/Scene.h"
#include "Eagle/Core/Serializer.h"
#include "Eagle/UI/Editors/AnimationGraphEditor.h"
#include "Eagle/Audio/Sound.h"
#include "Eagle/Audio/SoundGroup.h"
#include "Eagle/Utils/Utils.h"
#include "Eagle/Utils/YamlUtils.h"
#include "Eagle/Utils/PlatformUtils.h"
#include "Eagle/Utils/SerializerUtils.h"
#include "Eagle/Components/Components.h"

namespace Eagle
{
	namespace Utils
	{
		template <typename Comp>
		void InvalidateCollisionGroups(const Ref<Scene>& scene, uint32_t validMasks)
		{
			auto view = scene->GetAllEntitiesWith<Comp>();
			for (auto entityID : view)
			{
				auto& comp = view.get<Comp>(entityID);
				comp.SetCollisionGroup(CollisionGroup(uint32_t(comp.GetCollisionGroup()) & validMasks));
				comp.SetInteractingCollisionGroup(CollisionGroup(uint32_t(comp.GetInteractingCollisionGroup()) & validMasks));
			}
		}
	}

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

	AssetTexture2D::AssetTexture2D(const Path& path, const Path& pathToRaw, GUID guid, const DataBuffer& rawData, std::vector<ScopedDataBuffer>&& compressedDataPerMip,
		const Ref<Texture2D>& texture, AssetTexture2DFormat format, TextureCompressor::Quality compression, bool bNormalMap)
	: Asset(path, pathToRaw, AssetType::Texture2D, guid, rawData)
		, m_CompressedDataPerMip(std::move(compressedDataPerMip))
		, m_Texture(texture)
		, m_Format(format)
		, m_Compression(compression)
		, bNormalMap(bNormalMap)
	{}

	void AssetAnimationGraph::Compile()
	{
		AnimationGraphEditor editor{ Cast<AssetAnimationGraph>(shared_from_this()) };
		editor.Compile();
	}

	void AssetTexture2D::SetCompression(TextureCompressor::Quality compression, uint32_t mipsCount)
	{
		EG_CORE_ASSERT(m_Texture);

		// If compression state didn't change and it's not compressed,
		// we can just generate mips. Otherwise, we need to upload the new data
		if (m_Compression == compression && m_Compression == TextureCompressor::Quality::Disabled)
		{
			if (m_Texture->GetMipsCount() != mipsCount)
				m_Texture->GenerateMips(mipsCount);
		}
		else
		{
			m_Compression = compression;
			UpdateTextureData_Internal(mipsCount);
		}
	}

	void AssetTexture2D::SetIsNormalMap(bool bNormalMap)
	{
		EG_CORE_ASSERT(m_Texture);

		if (bNormalMap == this->bNormalMap)
			return;

		this->bNormalMap = bNormalMap;
		if (m_Compression != TextureCompressor::Quality::Disabled) // Only affects compressed textures
			UpdateTextureData_Internal(m_Texture->GetMipsCount());
	}

	void AssetTexture2D::SetFormat(AssetTexture2DFormat format)
	{
		EG_CORE_ASSERT(m_Texture);

		if (m_Format == format)
			return;

		m_Format = format;
		UpdateTextureData_Internal(m_Texture->GetMipsCount());
	}

	void AssetTexture2D::UpdateTextureData_Internal(uint32_t mipsCount)
	{
		// If it's a compressed format, load the image data, compress it, upload to the GPU and generate mips.
		// If the generation of compressed data has failed because the hardware doesn't support it, fallback to regular format

		const auto& rawData = GetRawData();
		if (m_Compression != TextureCompressor::Quality::Disabled)
		{
			const uint32_t targetNumChannels = AssetTextureFormatToChannels(m_Format, m_Compression);
			auto compressedData = TextureCompressor::Compress(rawData.GetDataBuffer(), targetNumChannels, mipsCount, m_Compression, bNormalMap);
			if (compressedData)
			{
				m_Texture->SetData(compressedData.DataPerMip, compressedData.Format);
				m_CompressedDataPerMip = std::move(compressedData.DataPerMip);
			}
			else
			{
				EG_CORE_ERROR("Failed to generate compressed texture: {}", GetPath().u8string());
				m_Compression = TextureCompressor::Quality::Disabled;
			}
		}

		if (m_Compression == TextureCompressor::Quality::Disabled)
		{
			const int desiredChannels = AssetTextureFormatToChannels(m_Format, m_Compression);
			int width = 0, height = 0, channels = 0;

			m_CompressedDataPerMip.clear();
			ScopedDataBuffer imageData = Utils::LoadTextureFromMemory(rawData, &width, &height, &channels, desiredChannels);
			if (imageData)
			{
				const ImageFormat imageFormat = AssetTextureFormatToImageFormat(m_Format);
				m_Texture->SetData(imageData.GetDataBuffer(), imageFormat);
			}
			else
			{
				EG_CORE_ERROR("Failed to load the image: {}", GetPath().u8string());
			}

			if (m_Texture->GetMipsCount() != mipsCount)
				m_Texture->GenerateMips(mipsCount);
		}
	}

	void AssetTextureCube::SetLayerSize(uint32_t layerSize)
	{
		m_Texture->SetLayerSize(layerSize);
	}

	void AssetTextureCube::SetPrefilterSize(uint32_t prefilter)
	{
		m_Texture->SetPrefilterSize(prefilter);
	}

	bool AssetTextureCube::SetFormat(AssetTextureCubeFormat format)
	{
		if (m_Format == format)
			return false;

		int width, height, channels;
		const ImageFormat desiredFormat = AssetTextureFormatToImageFormat(format);
		ScopedDataBuffer imageData = Utils::LoadHDRTextureFromMemory(m_RawData, &width, &height, &channels, desiredFormat);
		if (!imageData)
		{
			EG_CORE_ERROR("Failed to change format of TextureCube asset. Failed to load the texture data from memory: {} - {}", m_Path.u8string(), Utils::GetEnumName(format));
			return false;
		}

		m_Texture->SetData(imageData.GetDataBuffer(), desiredFormat);

		m_Format = format;

		return true;
	}

	void Asset::OnModified()
	{
		AssetManager::OnModified(shared_from_this());

		for (auto& [_, func] : m_Callbacks)
			func();
	}

	Ref<Asset> Asset::Create(const Path& path)
	{
		if (!std::filesystem::exists(path))
		{
			EG_CORE_ERROR("Failed to load an asset. It doesn't exist: {}", path.u8string());
			return {};
		}

		return Serializer::DeserializeAsset(path);
	}

	Ref<Asset> Asset::Create(const DataBuffer& data, const Path& path)
	{
		return Serializer::DeserializeAsset(data, path);
	}

	void Asset::Save(const Ref<Asset>& asset)
	{
		if (asset->GetAssetType() == AssetType::Scene)
		{
			EG_CORE_ERROR("Error saving an asset. This API doesn't support saving scenes! `SceneSerializer` must be used");
			return;
		}

		ScopedDataBuffer data = Serializer::SerializeAsset(asset);
		FileSystem::Write(asset->GetPath(), data);
		asset->SetDirty(false);
	}

	void Asset::Reload(Ref<Asset>& asset, bool bReloadRawData)
	{
		const AssetType assetType = asset->GetAssetType();
		const Path& assetPath = asset->GetPath();

		if (assetType == AssetType::Scene)
		{
			EG_CORE_ERROR("Reloading scene assets is not supported! {}", assetPath.u8string());
			return;
		}

		Ref<Asset> reloaded = Serializer::DeserializeAsset(assetPath, bReloadRawData);

		if (!reloaded)
			return;

		if (bReloadRawData)
		{
			asset->SetDirty(true);
			asset->OnModified();
		}

		Asset& reloadedRaw = *reloaded.get();
		*asset = std::move(reloadedRaw);

		// TODO: Use `OnModified`
		if (assetType == AssetType::Texture2D || assetType == AssetType::Material)
			MaterialSystem::SetDirty();
		else if (assetType == AssetType::StaticMesh)
		{
			if (auto& scene = Scene::GetCurrentScene())
				scene->SetStaticMeshesDirty(true);
		}
		else if (assetType == AssetType::SkeletalMesh)
		{
			if (auto& scene = Scene::GetCurrentScene())
				scene->SetSkeletalMeshesDirty(true);
		}
		else if (assetType == AssetType::Font)
		{
			if (auto& scene = Scene::GetCurrentScene())
				scene->SetTextsDirty(true);
		}
	}

	Ref<AssetMaterial> AssetMaterial::Create(const Ref<Material>& material)
	{
		class LocalAssetMaterial : public AssetMaterial
		{
		public:
			LocalAssetMaterial(const Path& path, GUID guid, const Ref<Material>& material)
				: AssetMaterial(path, guid, material) {}
		};

		return MakeRef<LocalAssetMaterial>("", GUID(), material);
	}
	
	void AssetAudio::SetSoundGroupAsset(const Ref<AssetSoundGroup>& soundGroup)
	{
		m_SoundGroup = soundGroup;
		if (m_SoundGroup)
			m_Audio->SetSoundGroup(m_SoundGroup->GetSoundGroup());
		else
			m_Audio->SetSoundGroup(nullptr);
	}

	Ref<AssetPhysicsMaterial> AssetPhysicsMaterial::Create(const Ref<PhysicsMaterial>& material)
	{
		class LocalAssetPhysicsMaterial : public AssetPhysicsMaterial
		{
		public:
			LocalAssetPhysicsMaterial(const Path& path, GUID guid, const Ref<PhysicsMaterial>& material)
				: AssetPhysicsMaterial(path, guid, material) {}
		};

		return MakeRef<LocalAssetPhysicsMaterial>("", GUID(), material);
	}
	
	void AssetEntity::InvalidateCollisionGroups(uint32_t validMasks)
	{
		if (!s_EntityAssetsScene)
			return;

		Utils::InvalidateCollisionGroups<BoxColliderComponent>(s_EntityAssetsScene, validMasks);
		Utils::InvalidateCollisionGroups<SphereColliderComponent>(s_EntityAssetsScene, validMasks);
		Utils::InvalidateCollisionGroups<CapsuleColliderComponent>(s_EntityAssetsScene, validMasks);
		Utils::InvalidateCollisionGroups<MeshColliderComponent>(s_EntityAssetsScene, validMasks);
	}
	
	Entity AssetEntity::CreateEntity(GUID guid)
	{
		return s_EntityAssetsScene->CreateEntityWithGUID(guid, "Root Entity");
	}
	
	void AssetParticleSystem::SetEmitters(const std::vector<ParticleEmitter>& emitters)
	{
		m_Emitters = emitters;
		for (auto& emitter : m_Emitters)
		{
			emitter.AnimationImagesNum = glm::max(glm::uvec2(1u), emitter.AnimationImagesNum);
		}

		SetDirty(true);
		OnModified();
	}

	void AssetParticleSystem::SetEmitters(std::vector<ParticleEmitter>&& emitters)
	{
		m_Emitters = std::move(emitters);
		for (auto& emitter : m_Emitters)
		{
			emitter.AnimationImagesNum = glm::max(glm::uvec2(1u), emitter.AnimationImagesNum);
		}

		SetDirty(true);
		OnModified();
	}
	
	Ref<AssetParticleSystem> AssetParticleSystem::Create()
	{
		class LocalAssetParticleSystem : public AssetParticleSystem
		{
		public:
			LocalAssetParticleSystem(const Path& path, GUID guid, const std::vector<ParticleEmitter>& emitters)
				: AssetParticleSystem(path, guid, emitters) {}
		};

		return MakeRef<LocalAssetParticleSystem>("", GUID{}, std::vector<ParticleEmitter>{});
	}
	
	Ref<AssetParticleSystem> AssetParticleSystem::Copy(const Ref<AssetParticleSystem>& asset)
	{
		class LocalAssetParticleSystem : public AssetParticleSystem
		{
		public:
			LocalAssetParticleSystem(const Path& path, GUID guid, const std::vector<ParticleEmitter>& emitters)
				: AssetParticleSystem(path, guid, emitters) {}
		};

		return MakeRef<LocalAssetParticleSystem>(asset->GetPath(), asset->GetGUID(), asset->GetEmitters());
	}

	void AssetAnimationBlendSpace::SetPointsData(const std::vector<BlendSpaceVertex>& pointsData)
	{
		SetPointsData_Internal(pointsData);
		SetDirty(true);
		OnModified();
	}

	void AssetAnimationBlendSpace::SetPointsData_Internal(const std::vector<BlendSpaceVertex>& pointsData)
	{
		m_PointsData = pointsData;

		for (auto& pointData : m_PointsData)
		{
			pointData.Coord.x = glm::clamp(pointData.Coord.x, m_Horizontal.Min, m_Horizontal.Max);
			pointData.Coord.y = glm::clamp(pointData.Coord.y, m_Vertical.Min, m_Vertical.Max);
		}
		Triangulate();
	}

	void AssetAnimationBlendSpace::Triangulate()
	{
		std::vector<Delaunay::Vertex> vertices(m_PointsData.size());
		for (size_t i = 0; i < m_PointsData.size(); ++i)
		{
			auto& point = m_PointsData[i];

			point.Index = uint32_t(i);
			vertices[i].Coord = point.Coord;
			vertices[i].UserData = &point;
		}
		m_Triangulation = Delaunay::Triangulate(vertices);
	}

	static bool GetBehaviorClassNodeData_Internal(const AIBehaviorNode& node, const GUID& id, AIBehaviorNode* outData)
	{
		if (node.Data.ID == id)
		{
			*outData = node;
			return true;
		}

		for (const auto& child : node.Children)
		{
			if (GetBehaviorClassNodeData_Internal(child, id, outData))
			{
				return true;
			}
		}

		return false;
	}

	AssetBehaviorGraph::~AssetBehaviorGraph()
	{
		ScriptEngine::RemoveOnAppAssemblyReloadedCallback(m_GUID);
	}

	bool AssetBehaviorGraph::GetClassNodeData(const GUID& id, AIBehaviorNode* outData) const
	{
		return GetBehaviorClassNodeData_Internal(m_Root, id, outData);
	}

	void AssetBehaviorGraph::RegisterCallback()
	{
		ScriptEngine::RemoveOnAppAssemblyReloadedCallback(m_GUID);

		ScriptEngine::AddOnAppAssemblyReloadedCallback(m_GUID, [this]()
		{
			ScriptEngine::UpdateAIBehaviorNodePublicFields(m_Root);
		});
	}
	
	void AssetStaticMesh::AddOnMaterialPropertyModifiedCallback()
	{
		m_Mesh->AddOnMaterialPropertyModifiedCallback(m_GUID, [this]()
		{
			OnModified();
		});
	}
	
	void AssetSkeletalMesh::AddOnMaterialPropertyModifiedCallback()
	{
		m_Mesh->AddOnMaterialPropertyModifiedCallback(m_GUID, [this]()
		{
			OnModified();
		});
	}
}
