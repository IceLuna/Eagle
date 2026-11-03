#include "egpch.h"
#include "Asset.h"
#include "AssetImporter.h"
#include "AssetManager.h"

#include "Eagle/Renderer/MaterialSystem.h"
#include "Eagle/Renderer/TextureCompressor.h"
#include "Eagle/Renderer/VidWrappers/Texture.h"

#include "Eagle/Classes/StaticMesh.h"
#include "Eagle/Classes/SkeletalMesh.h"
#include "Eagle/Animation/Animation.h"
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
#include "Eagle/SceneSequence/SequenceTrack.h"

#include "Eagle/Physics/PhysXCookingFactory.h"

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

		void RestoreVirtualBones(const BoneNode& node, BoneNode& dstRoot)
		{
			bool bHasAnyVirtualChild = false;
			for (auto& child : node.Children)
			{
				if (child.bVirtualBone)
				{
					bHasAnyVirtualChild = true;
					break;
				}
			}

			if (bHasAnyVirtualChild)
			{
				BoneNode* foundNode = nullptr;
				if (dstRoot.FindNode(node.GetNameHash(), &foundNode))
				{
					for (auto& child : node.Children)
					{
						if (!child.bVirtualBone)
							continue;

						// Copy the virtual bone
						foundNode->Children.emplace_back(child);
					}
				}
			}

			// We don't need to check children on virtual bones since they're all appended at once during the virtual bone copy
			if (!node.bVirtualBone)
			{
				for (auto& child : node.Children)
				{
					RestoreVirtualBones(child, dstRoot);
				}
			}
		}
	}

	Ref<Scene> AssetEntity::s_EntityAssetsScene;

	Asset::Asset(const Path& path, const Path& pathToRaw, AssetType type, GUID guid, bool bAlwaysIncludeInGameBuilds, const DataBuffer& rawData)
		: m_Path(path), m_PathToRaw(pathToRaw), m_GUID(guid), m_Type(type), bAlwaysIncludeInGameBuilds(bAlwaysIncludeInGameBuilds)
	{
		if (rawData.Size)
		{
			m_RawData.Allocate(rawData.Size);
			m_RawData.Write(rawData.Data, rawData.Size);
		}
	}

	AssetTexture2D::AssetTexture2D(const Path& path, const Path& pathToRaw, GUID guid, bool bAlwaysIncludeInGameBuilds, const DataBuffer& rawData, std::vector<ScopedDataBuffer>&& compressedDataPerMip,
		const Ref<Texture2D>& texture, AssetTexture2DFormat format, TextureCompressor::Quality compression, bool bNormalMap)
	: Asset(path, pathToRaw, AssetType::Texture2D, guid, bAlwaysIncludeInGameBuilds, rawData)
		, m_CompressedDataPerMip(std::move(compressedDataPerMip))
		, m_Texture(texture)
	{
		EG_CORE_ASSERT(m_Texture);

		m_Settings.Format = format;
		m_Settings.Compression = compression;
		m_Settings.bNormalMap = bNormalMap;
		m_Settings.MipsCount = m_Texture->GetMipsCount();
		m_Settings.Anisotropy = m_Texture->GetAnisotropy();
		m_Settings.Filter = m_Texture->GetFilterMode();
		m_Settings.Address = m_Texture->GetAddressMode();
	}

	void AssetAnimationGraph::Compile()
	{
		AnimationGraphEditor editor{ Cast<AssetAnimationGraph>(shared_from_this()) };
		editor.Compile();
	}

	void AssetTexture2D::SetSettings(const Settings& settings)
	{
		EG_CORE_ASSERT(m_Texture);

		Settings newSettings = settings;
		newSettings.MipsCount = glm::clamp(settings.MipsCount, 1u, CalculateMipCount(m_Texture->GetSize()));
		if (newSettings == m_Settings)
			return;

		// Compressed mips are made by the compressor, so changing their count requires compressing again.
		// If the texture isn't compressed and stays that way, its mips can be generated from the uploaded data
		const bool bCompressed = newSettings.Compression != TextureCompressor::Quality::Disabled;
		const bool bMipsChanged = m_Texture->GetMipsCount() != newSettings.MipsCount;
		const bool bUploadData = m_Settings.Format != newSettings.Format || m_Settings.Compression != newSettings.Compression
			|| (bCompressed && (m_Settings.bNormalMap != newSettings.bNormalMap || bMipsChanged));

		m_Settings.Format = newSettings.Format;
		m_Settings.Compression = newSettings.Compression;
		m_Settings.bNormalMap = newSettings.bNormalMap;
		if (bUploadData)
			UpdateTextureData_Internal(newSettings.MipsCount);
		else if (bMipsChanged)
			m_Texture->GenerateMips(newSettings.MipsCount);

		if (m_Texture->GetAnisotropy() != newSettings.Anisotropy)
			m_Texture->SetAnisotropy(newSettings.Anisotropy);
		if (m_Texture->GetFilterMode() != newSettings.Filter)
			m_Texture->SetFilterMode(newSettings.Filter);
		if (m_Texture->GetAddressMode() != newSettings.Address)
			m_Texture->SetAddressMode(newSettings.Address);

		// What was actually applied (`UpdateTextureData_Internal()` keeps `m_Settings.Compression` up to date)
		m_Settings.MipsCount = m_Texture->GetMipsCount();
		m_Settings.Anisotropy = m_Texture->GetAnisotropy();
		m_Settings.Filter = m_Texture->GetFilterMode();
		m_Settings.Address = m_Texture->GetAddressMode();
	}

	void AssetTexture2D::SetCompression(TextureCompressor::Quality compression, uint32_t mipsCount)
	{
		Settings settings = m_Settings;
		settings.Compression = compression;
		settings.MipsCount = mipsCount;
		SetSettings(settings);
	}

	void AssetTexture2D::SetIsNormalMap(bool bNormalMap)
	{
		Settings settings = m_Settings;
		settings.bNormalMap = bNormalMap;
		SetSettings(settings);
	}

	void AssetTexture2D::SetFormat(AssetTexture2DFormat format)
	{
		Settings settings = m_Settings;
		settings.Format = format;
		SetSettings(settings);
	}

	void AssetTexture2D::SetMipsCount(uint32_t mipsCount)
	{
		Settings settings = m_Settings;
		settings.MipsCount = mipsCount;
		SetSettings(settings);
	}

	void AssetTexture2D::SetAnisotropy(float anisotropy)
	{
		Settings settings = m_Settings;
		settings.Anisotropy = anisotropy;
		SetSettings(settings);
	}

	void AssetTexture2D::SetFilterMode(FilterMode filterMode)
	{
		Settings settings = m_Settings;
		settings.Filter = filterMode;
		SetSettings(settings);
	}

	void AssetTexture2D::SetAddressMode(AddressMode addressMode)
	{
		Settings settings = m_Settings;
		settings.Address = addressMode;
		SetSettings(settings);
	}

	void AssetTexture2D::UpdateTextureData_Internal(uint32_t mipsCount)
	{
		// If it's a compressed format, load the image data, compress it, upload to the GPU and generate mips.
		// If the generation of compressed data has failed because the hardware doesn't support it, fallback to regular format

		const auto& rawData = GetRawData();
		auto& compression = m_Settings.Compression;
		const AssetTexture2DFormat format = m_Settings.Format;
		if (compression != TextureCompressor::Quality::Disabled)
		{
			const uint32_t targetNumChannels = AssetTextureFormatToChannels(format, compression);
			auto compressedData = TextureCompressor::Compress(rawData.GetDataBuffer(), targetNumChannels, mipsCount, compression, m_Settings.bNormalMap);
			if (compressedData)
			{
				m_Texture->SetData(compressedData.DataPerMip, compressedData.Format);
				m_CompressedDataPerMip = std::move(compressedData.DataPerMip);
			}
			else
			{
				EG_CORE_ERROR("Failed to generate compressed texture: {}", GetPath());
				compression = TextureCompressor::Quality::Disabled;
			}
		}

		if (compression == TextureCompressor::Quality::Disabled)
		{
			const int desiredChannels = AssetTextureFormatToChannels(format, compression);
			int width = 0, height = 0, channels = 0;

			m_CompressedDataPerMip.clear();
			ScopedDataBuffer imageData = Utils::LoadTextureFromMemory(rawData, &width, &height, &channels, desiredChannels);
			if (imageData)
			{
				const ImageFormat imageFormat = AssetTextureFormatToImageFormat(format);
				m_Texture->SetData(imageData.GetDataBuffer(), imageFormat);
			}
			else
			{
				EG_CORE_ERROR("Failed to load the image: {}", GetPath());
			}

			if (m_Texture->GetMipsCount() != mipsCount)
				m_Texture->GenerateMips(mipsCount);
		}
	}

	AssetTextureCube::AssetTextureCube(const Path& path, const Path& pathToRaw, GUID guid, bool bAlwaysIncludeInGameBuilds, const DataBuffer& rawData, const Ref<TextureCube>& texture, AssetTextureCubeFormat format)
		: Asset(path, pathToRaw, AssetType::TextureCube, guid, bAlwaysIncludeInGameBuilds, rawData)
		, m_Texture(texture)
	{
		EG_CORE_ASSERT(m_Texture);

		m_Settings.Format = format;
		m_Settings.LayerSize = m_Texture->GetSize().x;
		m_Settings.PrefilterSize = m_Texture->GetPrefilterSize();
		m_Settings.bCompressed = m_Texture->IsCompressed();
	}

	bool AssetTextureCube::SetSettings(const Settings& settings)
	{
		EG_CORE_ASSERT(m_Texture);

		bool bResult = true;
		if (m_Settings.Format != settings.Format || m_Settings.bCompressed != settings.bCompressed)
		{
			int width, height, channels;
			const ImageFormat desiredFormat = AssetTextureFormatToImageFormat(settings.Format);
			ScopedDataBuffer imageData = Utils::LoadHDRTextureFromMemory(m_RawData, &width, &height, &channels, desiredFormat);
			if (imageData)
			{
				m_Texture->SetData(imageData.GetDataBuffer(), desiredFormat, settings.bCompressed);
				m_Settings.Format = settings.Format;
				m_Settings.bCompressed = settings.bCompressed;
			}
			else
			{
				EG_CORE_ERROR("Failed to change the format or compression of TextureCube asset. Failed to load the texture data from memory: {} - {}", m_Path, Utils::GetEnumName(settings.Format));
				bResult = false;
			}
		}

		if (m_Texture->GetSize().x != settings.LayerSize)
			m_Texture->SetLayerSize(settings.LayerSize);
		if (m_Texture->GetPrefilterSize() != settings.PrefilterSize)
			m_Texture->SetPrefilterSize(settings.PrefilterSize);
		m_Settings.LayerSize = m_Texture->GetSize().x;
		m_Settings.PrefilterSize = m_Texture->GetPrefilterSize();

		return bResult;
	}

	void AssetTextureCube::SetLayerSize(uint32_t layerSize)
	{
		Settings settings = m_Settings;
		settings.LayerSize = layerSize;
		SetSettings(settings);
	}

	void AssetTextureCube::SetPrefilterSize(uint32_t prefilter)
	{
		Settings settings = m_Settings;
		settings.PrefilterSize = prefilter;
		SetSettings(settings);
	}

	bool AssetTextureCube::SetFormat(AssetTextureCubeFormat format)
	{
		Settings settings = m_Settings;
		settings.Format = format;
		return SetSettings(settings);
	}

	bool AssetTextureCube::SetCompressed(bool bCompress)
	{
		Settings settings = m_Settings;
		settings.bCompressed = bCompress;
		return SetSettings(settings);
	}

	void Asset::AddOnAssetModifiedCallback(const GUID& id, const std::function<void()>& func)
	{
		std::scoped_lock lock(m_Mutex);
		EG_CORE_ASSERT(m_Callbacks.find(id) == m_Callbacks.end());
		m_Callbacks[id] = func;
	}

	void Asset::OnModified()
	{
		// Keeps the asset alive until every callback has run, since one of them might drop the last reference to it
		const Ref<Asset> self = shared_from_this();

		AssetManager::OnModified(self);

		// Callbacks are allowed to add or remove listeners (including themselves) and to destroy other listeners.
		// So, save the IDs, and look each one up again right before calling it
		std::vector<GUID> ids;
		{
			std::scoped_lock lock(m_Mutex);
			ids.reserve(m_Callbacks.size());
			for (const auto& [id, _] : m_Callbacks)
				ids.push_back(id);
		}

		for (const GUID& id : ids)
		{
			std::function<void()> func;
			{
				std::scoped_lock lock(m_Mutex);
				auto it = m_Callbacks.find(id);
				if (it == m_Callbacks.end())
					continue;
				func = it->second;
			}
			func();
		}
	}

	Ref<Asset> Asset::Create(const Path& path)
	{
		if (!std::filesystem::exists(path))
		{
			EG_CORE_ERROR("Failed to load an asset. It doesn't exist: {}", path);
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

	void Asset::Reload(const Ref<Asset>& asset, bool bReloadRawData)
	{
		const AssetType assetType = asset->GetAssetType();
		const Path& assetPath = asset->GetPath();

		if (assetType == AssetType::Scene)
		{
			EG_CORE_ERROR("Reloading scene assets is not supported! {}", assetPath);
			return;
		}

		Ref<Asset> reloaded = Serializer::DeserializeAsset(assetPath, bReloadRawData);

		if (!reloaded || reloaded->GetAssetType() != assetType)
		{
			EG_CORE_ERROR("Asset reload failed: {}", asset->GetPath());
			return;
		}

		if (bReloadRawData)
		{
			// Keep the materials
			if (assetType == AssetType::StaticMesh)
			{
				Ref<AssetStaticMesh> reloadedMesh = Cast<AssetStaticMesh>(reloaded);
				Ref<AssetStaticMesh> oldMesh = Cast<AssetStaticMesh>(asset);
				const uint32_t matCount = glm::min(reloadedMesh->GetMesh()->GetMaterialSlotsCount(), oldMesh->GetMesh()->GetMaterialSlotsCount());
				for (uint32_t i = 0; i < matCount; ++i)
				{
					reloadedMesh->GetMesh()->SetMaterialAsset(i, oldMesh->GetMesh()->GetMaterialAsset(i));
				}
				PhysXCookingFactory::DeleteCached(oldMesh);
			}
			else if (assetType == AssetType::SkeletalMesh)
			{
				Ref<AssetSkeletalMesh> reloadedMesh = Cast<AssetSkeletalMesh>(reloaded);
				Ref<AssetSkeletalMesh> oldMesh = Cast<AssetSkeletalMesh>(asset);
				const uint32_t matCount = glm::min(reloadedMesh->GetMesh()->GetMaterialSlotsCount(), oldMesh->GetMesh()->GetMaterialSlotsCount());
				for (uint32_t i = 0; i < matCount; ++i)
				{
					reloadedMesh->GetMesh()->SetMaterialAsset(i, oldMesh->GetMesh()->GetMaterialAsset(i));
				}

				// Restore virtual bones
				{
					auto& dstRoot = reloadedMesh->GetMesh()->GetSkeletalMeshInfo().RootBone;
					const auto& root = oldMesh->GetMesh()->GetSkeletalMeshInfo().RootBone;
					Utils::RestoreVirtualBones(root, dstRoot);

					// Restoring virtual bones adds children to the tree (possibly reallocating `Children` vectors),
					// so the animation system's cached bone pointers must be rebuilt
					reloadedMesh->GetMesh()->GetSkeletalMeshInfo().BuildFlattenedBones();
				}
				PhysXCookingFactory::DeleteCached(oldMesh);
			}
		}

		Asset& reloadedRaw = *reloaded.get();
		*asset = std::move(reloadedRaw);

		asset->SetDirty(true);
		asset->OnModified();

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
				: AssetMaterial(path, guid, false, material) {}
		};

		return MakeRef<LocalAssetMaterial>("", GUID(), material);
	}
	
	void AssetAudio::SetSettings(const Settings& settings)
	{
		if (m_SoundGroup != settings.SoundGroup)
			SetSoundGroupAsset(settings.SoundGroup);

		m_Audio->SetSettings(settings.Audio);
	}

	void AssetAudio::SetSoundGroupAsset(const Ref<AssetSoundGroup>& soundGroup)
	{
		m_SoundGroup = soundGroup;
		if (m_SoundGroup)
			m_Audio->SetSoundGroup(m_SoundGroup->GetSoundGroup());
		else
			m_Audio->SetSoundGroup(nullptr);
	}

	const AssetAnimation::Settings& AssetAnimation::GetSettings() const
	{
		return m_Animation->Settings;
	}

	void AssetAnimation::SetSettings(const Settings& settings)
	{
		m_Animation->Settings = settings;
	}

	const AssetMaterial::Settings& AssetMaterial::GetSettings() const
	{
		return m_Material->GetSettings();
	}

	void AssetMaterial::SetSettings(const Settings& settings)
	{
		m_Material->SetSettings(settings);
	}

	Ref<AssetPhysicsMaterial> AssetPhysicsMaterial::Create(const Ref<PhysicsMaterial>& material)
	{
		class LocalAssetPhysicsMaterial : public AssetPhysicsMaterial
		{
		public:
			LocalAssetPhysicsMaterial(const Path& path, GUID guid, const Ref<PhysicsMaterial>& material)
				: AssetPhysicsMaterial(path, guid, false, material) {}
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

	Ref<AssetEntity> AssetEntity::Create(const Path& saveTo, const std::string& filename, Entity entity)
	{
		class LocalAssetEntity : public AssetEntity
		{
		public:
			LocalAssetEntity(const Path& path, GUID guid, const Ref<Entity>& entity)
				: AssetEntity(path, guid, false, entity) {
			}
		};

		const Path pathToAsset = Utils::GetUniqueAssetFilepath(saveTo, filename);
		Entity newEntity = AssetEntity::GetScene()->CreateFromEntity(entity, false);
		newEntity.SetParent(Entity::Null); // Removing the parent since it won't be in the asset
		newEntity.SetWorldLocation(glm::vec3(0)); // Doesn't make sense to copy the location to the asset
		const GUID& assetGUID = newEntity.GetGUID();

		Ref<AssetEntity> result = MakeRef<LocalAssetEntity>(pathToAsset, assetGUID, MakeRef<Entity>(newEntity));
		FileSystem::Write(pathToAsset, Serializer::SerializeAssetEntity(result));
		AssetManager::Register(result);

		return result;
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
				: AssetParticleSystem(path, guid, false, emitters) {}
		};

		return MakeRef<LocalAssetParticleSystem>("", GUID{}, std::vector<ParticleEmitter>{});
	}
	
	Ref<AssetParticleSystem> AssetParticleSystem::Copy(const Ref<AssetParticleSystem>& asset)
	{
		class LocalAssetParticleSystem : public AssetParticleSystem
		{
		public:
			LocalAssetParticleSystem(const Path& path, GUID guid, bool bAlwaysIncludeInGameBuilds, const std::vector<ParticleEmitter>& emitters)
				: AssetParticleSystem(path, guid, bAlwaysIncludeInGameBuilds, emitters) {}
		};

		return MakeRef<LocalAssetParticleSystem>(asset->GetPath(), asset->GetGUID(), asset->IsAlwaysIncludedInGameBuilds(), asset->GetEmitters());
	}

	void AssetAnimationBlendSpace::SetPointsData(const std::vector<BlendSpaceVertex>& pointsData)
	{
		SetPointsData_Internal(pointsData);
		SetDirty(true);
		OnModified();
	}

	void AssetAnimationBlendSpace::SetSettings(const Settings& settings)
	{
		if (m_Settings == settings)
			return;

		m_Settings = settings;
		SetPointsData_Internal(m_Settings.Points);
		SetDirty(true);
		OnModified();
	}

	void AssetAnimationBlendSpace::SetPointsData_Internal(const std::vector<BlendSpaceVertex>& pointsData)
	{
		auto& points = m_Settings.Points;
		if (&points != &pointsData)
			points = pointsData;

		for (auto& pointData : points)
		{
			pointData.Coord.x = glm::clamp(pointData.Coord.x, m_Settings.Horizontal.Min, m_Settings.Horizontal.Max);
			pointData.Coord.y = glm::clamp(pointData.Coord.y, m_Settings.Vertical.Min, m_Settings.Vertical.Max);
		}
		Triangulate();
	}

	void AssetAnimationBlendSpace::Triangulate()
	{
		auto& points = m_Settings.Points;
		std::vector<Delaunay::Vertex> vertices(points.size());
		for (size_t i = 0; i < points.size(); ++i)
		{
			auto& point = points[i];

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
	
	AssetStaticMesh::~AssetStaticMesh()
	{
		if (m_Mesh)
			m_Mesh->RemoveOnMaterialPropertyModifiedCallback(m_GUID);
	}

	AssetStaticMesh& AssetStaticMesh::operator=(Asset&& other) noexcept
	{
		if (this == &other)
			return *this;

		if (m_Mesh)
			m_Mesh->RemoveOnMaterialPropertyModifiedCallback(m_GUID);

		Asset::operator=(std::move(other));

		AssetStaticMesh&& meshAsset = (AssetStaticMesh&&)other;
		meshAsset.m_Mesh->RemoveOnMaterialPropertyModifiedCallback(m_GUID);
		m_Mesh = std::move(meshAsset.m_Mesh);
		SetSourceMeshData(meshAsset.IsCombinedMesh(), meshAsset.GetSourceMeshName(), meshAsset.GetSourceMeshIndex(), meshAsset.WasLocationReset());
		AddOnMaterialPropertyModifiedCallback();

		return *this;
	}

	AssetStaticMesh::Settings AssetStaticMesh::GetSettings() const
	{
		Settings settings;
		const uint32_t count = m_Mesh->GetMaterialSlotsCount();
		settings.Materials.reserve(count);
		for (uint32_t i = 0; i < count; ++i)
			settings.Materials.push_back(m_Mesh->GetMaterialAsset(i));

		return settings;
	}

	void AssetStaticMesh::SetSettings(const Settings& settings)
	{
		const uint32_t count = glm::min(m_Mesh->GetMaterialSlotsCount(), uint32_t(settings.Materials.size()));
		for (uint32_t i = 0; i < count; ++i)
		{
			if (m_Mesh->GetMaterialAsset(i) != settings.Materials[i])
				m_Mesh->SetMaterialAsset(i, settings.Materials[i]);
		}
	}

	void AssetStaticMesh::AddOnMaterialPropertyModifiedCallback()
	{
		m_Mesh->AddOnMaterialPropertyModifiedCallback(m_GUID, [this]()
		{
			OnModified();
		});
	}
	
	AssetSkeletalMesh::~AssetSkeletalMesh()
	{
		if (m_Mesh)
			m_Mesh->RemoveOnMaterialPropertyModifiedCallback(m_GUID);
	}

	AssetSkeletalMesh& AssetSkeletalMesh::operator=(Asset&& other) noexcept
	{
		if (this == &other)
			return *this;

		if (m_Mesh)
			m_Mesh->RemoveOnMaterialPropertyModifiedCallback(m_GUID);

		Asset::operator=(std::move(other));

		AssetSkeletalMesh&& meshAsset = (AssetSkeletalMesh&&)other;
		meshAsset.m_Mesh->RemoveOnMaterialPropertyModifiedCallback(m_GUID);
		m_Mesh = std::move(meshAsset.m_Mesh);
		SetSourceMeshData(meshAsset.IsCombinedMesh(), meshAsset.GetSourceMeshName(), meshAsset.GetSourceMeshIndex(), meshAsset.WasLocationReset());
		AddOnMaterialPropertyModifiedCallback();

		return *this;
	}

	AssetSkeletalMesh::Settings AssetSkeletalMesh::GetSettings() const
	{
		return m_Mesh->GetSettings();
	}

	void AssetSkeletalMesh::SetSettings(const Settings& settings)
	{
		m_Mesh->SetSettings(settings);
	}

	void AssetSkeletalMesh::AddOnMaterialPropertyModifiedCallback()
	{
		m_Mesh->AddOnMaterialPropertyModifiedCallback(m_GUID, [this]()
		{
			OnModified();
		});
	}

	AssetSceneSequence::AssetSceneSequence(const Path& path, GUID guid, bool bAlwaysIncludeInGameBuilds, std::vector<Ref<SequenceTrack>>&& tracks, float duration, float frameRate, bool bLooping)
		: Asset(path, {}, AssetType::SceneSequence, guid, bAlwaysIncludeInGameBuilds, {})
	{
		m_Settings.Duration = glm::max(0.01f, duration);
		m_Settings.FrameRate = glm::clamp(frameRate, 1.f, 240.f);
		m_Settings.bLooping = bLooping;
		SetTracks_Internal(std::move(tracks));
	}

	void AssetSceneSequence::SetSettings(const Settings& settings)
	{
		m_Settings = settings;
		m_Settings.Duration = glm::max(0.01f, settings.Duration);
		m_Settings.FrameRate = glm::clamp(settings.FrameRate, 1.f, 240.f);
		SetDirty(true);
	}

	void AssetSceneSequence::SetTracks_Internal(std::vector<Ref<SequenceTrack>>&& tracks)
	{
		m_Tracks = std::move(tracks);
		m_CameraCutTracks.clear();
		m_CameraTracks.clear();
		m_PostProcessTracks.clear();
		m_EventTracks.clear();
		for (const auto& track : m_Tracks)
		{
			if (track->GetType() == SequenceTrackType::CameraCuts)
			{
				const SequenceCameraCutTrack* cameraCut = (const SequenceCameraCutTrack*)track.get();
				m_CameraCutTracks.push_back(cameraCut);
			}
			else if (track->GetType() == SequenceTrackType::Camera)
			{
				const SequenceCameraTrack* camera = (const SequenceCameraTrack*)track.get();
				m_CameraTracks.push_back(camera);
			}
			else if (track->GetType() == SequenceTrackType::PostProcess)
			{
				const SequencePostProcessTrack* postProcess = (const SequencePostProcessTrack*)track.get();
				m_PostProcessTracks.push_back(postProcess);
			}
			else if (track->GetType() == SequenceTrackType::Event)
			{
				const SequenceEventTrack* events = (const SequenceEventTrack*)track.get();
				m_EventTracks.push_back(events);
			}
		}
	}

	void AssetSceneSequence::SetTracks(std::vector<Ref<SequenceTrack>>&& tracks)
	{
		SetTracks_Internal(std::move(tracks));
		SetDirty(true);
	}

	void AssetSceneSequence::AddTrack(const Ref<SequenceTrack>& track)
	{
		if (!track)
			return;

		m_Tracks.push_back(track);
		if (track->GetType() == SequenceTrackType::CameraCuts)
		{
			const SequenceCameraCutTrack* cameraCut = (const SequenceCameraCutTrack*)track.get();
			m_CameraCutTracks.push_back(cameraCut);
		}
		else if (track->GetType() == SequenceTrackType::Camera)
		{
			const SequenceCameraTrack* camera = (const SequenceCameraTrack*)track.get();
			m_CameraTracks.push_back(camera);
		}
		else if (track->GetType() == SequenceTrackType::PostProcess)
		{
			const SequencePostProcessTrack* postProcess = (const SequencePostProcessTrack*)track.get();
			m_PostProcessTracks.push_back(postProcess);
		}
		else if (track->GetType() == SequenceTrackType::Event)
		{
			const SequenceEventTrack* events = (const SequenceEventTrack*)track.get();
			m_EventTracks.push_back(events);
		}
		SetDirty(true);
	}

	bool AssetSceneSequence::RemoveTrack(const GUID& id)
	{
		for (auto it = m_Tracks.begin(); it != m_Tracks.end(); ++it)
		{
			if ((*it)->GetID() == id)
			{
				const SequenceTrackType type = (*it)->GetType();
				if (type == SequenceTrackType::CameraCuts)
				{
					const SequenceCameraCutTrack* cameraCut = (const SequenceCameraCutTrack*)(*it).get();
					std::erase(m_CameraCutTracks, cameraCut);
				}
				else if (type == SequenceTrackType::Camera)
				{
					const SequenceCameraTrack* camera = (const SequenceCameraTrack*)(*it).get();
					std::erase(m_CameraTracks, camera);
				}
				else if (type == SequenceTrackType::PostProcess)
				{
					const SequencePostProcessTrack* postProcess = (const SequencePostProcessTrack*)(*it).get();
					std::erase(m_PostProcessTracks, postProcess);
				}
				else if (type == SequenceTrackType::Event)
				{
					const SequenceEventTrack* events = (const SequenceEventTrack*)(*it).get();
					std::erase(m_EventTracks, events);
				}
				m_Tracks.erase(it);
				SetDirty(true);
				return true;
			}
		}
		return false;
	}

	Ref<SequenceTrack> AssetSceneSequence::FindTrack(const GUID& id) const
	{
		for (const auto& track : m_Tracks)
			if (track->GetID() == id)
				return track;
		return {};
	}

	void AssetSceneSequence::Evaluate(SequenceEvalContext& context) const
	{
		// Several camera tracks can exist, but only one of them is live at a time
		const SequenceCameraTrack* activeCamera = ResolveActiveCamera(context.Time);

		for (const auto& track : m_Tracks)
		{
			if (!track || !track->IsEnabled())
				continue;

			if (track->GetType() == SequenceTrackType::Camera && track.get() != activeCamera)
				continue;

			// Post process tracks are handled below: whether they apply depends on the live camera
			if (track->GetType() == SequenceTrackType::PostProcess)
				continue;

			track->Evaluate(context);
		}

		// Evaluate post processing tracks.
		{
			// Gathers the rendering settings overridden at `context.Time` into `context.PostProcess`
			// Evaluated in track order, so when two active tracks key the same property,
			// the one further down the list wins
			for (const auto& track : m_PostProcessTracks)
			{
				if (!track || !track->IsEnabled())
					continue;

				EG_CORE_ASSERT(track->GetType() == SequenceTrackType::PostProcess);

				// Entering the track applies its properties, leaving it stops overriding them,
				// which restores the scene's own settings
				if (!track->IsActiveAt(context.Time))
					continue;

				// A track tied to a camera only applies while that camera is the live one.
				// That's what lets one track hold the depth of field for the first camera
				// and another hold it for the second
				const GUID& cameraID = track->GetCameraTrackID();
				if (!cameraID.IsNull() && (!activeCamera || activeCamera->GetID() != cameraID))
					continue;

				track->Evaluate(context);
			}
		}
	}

	void AssetSceneSequence::GatherEvents(const SequenceEventWindow& window, std::vector<SequenceEvent>& outEvents) const
	{
		if (!window.bValid)
			return;

		const size_t firstNewEvent = outEvents.size();
		for (const auto& track : m_EventTracks)
		{
			if (track && track->IsEnabled())
				track->GatherEvents(window, outEvents);
		}

		// Fire in the order playback reached them, not in track order. Matters when a single frame
		// covers several keys, and when the window wrapped around the end of the sequence
		const float duration = m_Settings.Duration;
		std::sort(outEvents.begin() + firstNewEvent, outEvents.end(), [&window, duration](const SequenceEvent& a, const SequenceEvent& b)
		{
			return window.GetTravelDistance(a.Time, duration) < window.GetTravelDistance(b.Time, duration);
		});
	}

	const SequenceCameraTrack* AssetSceneSequence::ResolveActiveCamera(float time) const
	{
		auto isUsableCamera = [](const SequenceCameraTrack* camera) -> const SequenceCameraTrack*
		{
			if (!camera || !camera->IsEnabled())
				return nullptr;

			EG_CORE_ASSERT(camera->GetType() == SequenceTrackType::Camera);
			return camera->HasTransformKeys() ? camera : nullptr;
		};

		// 1. Explicit cuts. Only the first enabled cuts track with keys counts.
		// A cut pointing at a deleted or disabled camera falls through to the automatic rule
		for (const auto& track : m_CameraCutTracks)
		{
			if (!track || !track->IsEnabled())
				continue;

			EG_CORE_ASSERT(track->GetType() == SequenceTrackType::CameraCuts);
			const GUID cameraID = track->GetCameraAt(time);
			if (cameraID.IsNull())
				continue;

			for (const auto& candidate : m_CameraTracks)
			{
				if (candidate && candidate->GetID() == cameraID)
				{
					if (const SequenceCameraTrack* camera = isUsableCamera(candidate))
						return camera;
					break;
				}
			}
			break;
		}

		// 2. Automatic. `>=` comparisons let tracks further down the list win ties
		const SequenceCameraTrack* covering = nullptr;
		float coveringStart = std::numeric_limits<float>::lowest();
		const SequenceCameraTrack* lastEnded = nullptr;
		float lastEnd = std::numeric_limits<float>::lowest();
		const SequenceCameraTrack* firstUpcoming = nullptr;
		float firstStart = std::numeric_limits<float>::max();

		for (const auto& track : m_CameraTracks)
		{
			const SequenceCameraTrack* camera = isUsableCamera(track);
			float start = 0.f, end = 0.f;
			if (!camera || !camera->GetShotRange(&start, &end))
				continue;

			if (time >= start && time <= end)
			{
				if (start >= coveringStart)
				{
					covering = camera;
					coveringStart = start;
				}
			}
			else if (end < time)
			{
				if (end >= lastEnd)
				{
					lastEnded = camera;
					lastEnd = end;
				}
			}
			else if (start < firstStart)
			{
				firstUpcoming = camera;
				firstStart = start;
			}
		}

		if (covering)
			return covering;
		return lastEnded ? lastEnded : firstUpcoming;
	}
}
