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

#include <stb_image.h>

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

	AssetAnimationGraph::AssetAnimationGraph(const Path& path, GUID guid, const Ref<AnimationGraph>& graph, const GraphEditorSerializationData& data)
		: Asset(path, {}, AssetType::AnimationGraph, guid, {}), m_Graph(graph), m_Data(data)
	{}

	void AssetAnimationGraph::Compile()
	{
		AnimationGraphEditor editor{ Cast<AssetAnimationGraph>(shared_from_this()) };
		editor.Compile();
	}

	void AssetTexture2D::SetIsCompressed(bool bCompressed, uint32_t mipsCount)
	{
		EG_CORE_ASSERT(m_Texture);

		if (bCompressed == this->bCompressed && m_Texture->GetMipsCount() == mipsCount)
			return;

		// DONT update bCompressed state here: // this->bCompressed = bCompressed;
		// It's done in `UpdateTextureData_Internal`
		UpdateTextureData_Internal(bCompressed, mipsCount);
	}

	void AssetTexture2D::SetIsNormalMap(bool bNormalMap)
	{
		EG_CORE_ASSERT(m_Texture);

		if (bNormalMap == this->bNormalMap)
			return;

		this->bNormalMap = bNormalMap;
		if (bCompressed) // Only affects compressed textures
			UpdateTextureData_Internal(bCompressed, m_Texture->GetMipsCount());
	}

	void AssetTexture2D::SetNeedsAlpha(bool bNeedAlpha)
	{
		EG_CORE_ASSERT(m_Texture);

		if (bNeedAlpha == this->bNeedAlpha)
			return;

		this->bNeedAlpha = bNeedAlpha;
		if (bCompressed) // Only affects compressed textures
			UpdateTextureData_Internal(bCompressed, m_Texture->GetMipsCount());
	}

	void AssetTexture2D::SetFormat(AssetTexture2DFormat format)
	{
		if (bCompressed || m_Format == format)
			return;

		m_Format = format;

		const int desiredChannels = AssetTextureFormatToChannels(m_Format);
		int width = 0, height = 0, channels = 0;

		void* stbiImageData = stbi_load_from_memory((uint8_t*)m_RawData.Data(), (int)m_RawData.Size(), &width, &height, &channels, desiredChannels);
		if (stbiImageData)
		{
			const ImageFormat imageFormat = AssetTextureFormatToImageFormat(m_Format);
			m_Texture->SetData(stbiImageData, imageFormat);

			stbi_image_free(stbiImageData);
		}
		else
			EG_CORE_ERROR("Failed to change the format of Texture 2D: {}", GetPath().u8string());
	}

	void AssetTexture2D::UpdateTextureData_Internal(bool bCompressed, uint32_t mipsCount)
	{
		// If it's a compressed format, try to load the image data, compress it, upload to the GPU and generate mips.
		// If the generation of compressed data has failed because the hardware doesn't support it:
		//	1) Save new KTX data that contains new mips, so that the asset stores that info for users that support it;
		//	2) And fallback to automatic mips generation

		const bool bCompressionChanged = this->bCompressed != bCompressed;
		this->bCompressed = bCompressed;
		bool bFailedToGenerateMips = false;

		if (bCompressed)
		{
			m_Format = AssetTexture2DFormat::RGBA8;

			constexpr int desiredChannels = 4;
			const auto& rawData = GetRawData();
			int width = 0, height = 0, channels = 0;

			void* stbiImageData = stbi_load_from_memory((uint8_t*)rawData.Data(), (int)rawData.Size(), &width, &height, &channels, desiredChannels);
			if (stbiImageData)
			{
				const glm::uvec2 baseTextureSize = m_Texture->GetSize();

				const size_t textureMemSize = desiredChannels * width * height;
				const void* compressedTextureHandle = TextureCompressor::Compress(DataBuffer(stbiImageData, textureMemSize),
					baseTextureSize, mipsCount, bNormalMap, bNeedAlpha);

				if (compressedTextureHandle)
				{
					SetKTXData(TextureCompressor::GetKTX2Data(compressedTextureHandle));

					if (TextureCompressor::IsCompressedSuccessfully(compressedTextureHandle))
					{
						const ImageFormat imageFormat = TextureCompressor::GetFormat(compressedTextureHandle);
						const uint32_t mipsCount = TextureCompressor::GetMipsCount(compressedTextureHandle);
						std::vector<DataBuffer> dataPerMip(mipsCount);
						for (uint32_t i = 0; i < mipsCount; ++i)
							dataPerMip[i] = TextureCompressor::GetMipData(compressedTextureHandle, i);
						m_Texture->GenerateMips(dataPerMip, imageFormat);
					}
					else
						bFailedToGenerateMips = true;

					TextureCompressor::Destroy(compressedTextureHandle);
				}
				else
					EG_CORE_ERROR("Failed to generate compressed texture container: {}", GetPath().u8string());

				stbi_image_free(stbiImageData);
			}
			else
			{
				// Failed to load the image. Don't set `bFailedToGenerateMips` to `true` so that the asset data doesn't update.
				EG_CORE_ERROR("Failed to load the image to compress the texture: {}", GetPath().u8string());
			}
		}
		else
		{
			if (bCompressionChanged)
			{
				const auto assetFormat = GetFormat();
				const int desiredChannels = AssetTextureFormatToChannels(assetFormat);
				int width = 0, height = 0, channels = 0;

				void* stbiImageData = stbi_load_from_memory((uint8_t*)m_RawData.Data(), (int)m_RawData.Size(), &width, &height, &channels, desiredChannels);
				if (stbiImageData)
				{
					SetKTXData(DataBuffer(nullptr, 0));

					const ImageFormat imageFormat = AssetTextureFormatToImageFormat(assetFormat);
					m_Texture->SetData(stbiImageData, imageFormat);

					stbi_image_free(stbiImageData);
				}
				else
					EG_CORE_ERROR("Failed to load the image: {}", GetPath().u8string());
			}
		}

		if (!bCompressed || bFailedToGenerateMips)
		{
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
		const int desiredChannels = AssetTextureFormatToChannels(format);
		void* stbiImageData = stbi_loadf_from_memory((uint8_t*)m_RawData.Data(), (int)m_RawData.Size(), &width, &height, &channels, desiredChannels);

		if (!stbiImageData)
		{
			EG_CORE_ERROR("Failed to change format of TextureCube asset. stbi_loadf_from_memory failed: {}", Utils::GetEnumName(format));
			return false;
		}

		const ImageFormat imageFormat = AssetTextureFormatToImageFormat(format);
		const bool bFloat16 = IsFloat16Format(format);
		void* imageData = stbiImageData;
		if (bFloat16)
		{
			const size_t pixels = size_t(width) * height * desiredChannels;
			imageData = malloc(pixels * sizeof(uint16_t));
			uint16_t* imageData16 = (uint16_t*)imageData;
			float* stbiImageData32 = (float*)stbiImageData;

			for (size_t i = 0; i < pixels; ++i)
				imageData16[i] = Utils::ToFloat16(stbiImageData32[i]);
		}
		else if (format == AssetTextureCubeFormat::R11G11B10)
		{
			const size_t pixels = size_t(width) * height;
			imageData = malloc(pixels * sizeof(uint32_t));
			uint32_t* imageData32 = (uint32_t*)imageData;
			float* stbiImageData32 = (float*)stbiImageData;
			for (size_t i = 0; i < pixels; ++i)
			{
				glm::vec3 rgb = glm::vec3(stbiImageData32[i * 3], stbiImageData32[i * 3 + 1], stbiImageData32[i * 3 + 2]);
				imageData32[i] = Utils::ToR11G11B10(rgb);
			}
		}

		m_Texture->SetData(imageData, imageFormat);

		if (stbiImageData != imageData)
			free(imageData);
		stbi_image_free(stbiImageData);

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
			m_Audio->SetSoundGroup(SoundGroup::GetMasterGroup());
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
		m_PointsData = pointsData;

		for (auto& pointData : m_PointsData)
		{
			pointData.Vertex.Coord.x = glm::clamp(pointData.Vertex.Coord.x, m_Horizontal.Min, m_Horizontal.Max);
			pointData.Vertex.Coord.y = glm::clamp(pointData.Vertex.Coord.y, m_Vertical.Min, m_Vertical.Max);
		}
		Triangulate();
	}

	void AssetAnimationBlendSpace::Triangulate()
	{
		std::vector<Delaunay::Vertex> vertices(m_PointsData.size());
		for (size_t i = 0; i < m_PointsData.size(); ++i)
		{
			vertices[i] = m_PointsData[i].Vertex;
			vertices[i].UserData = &m_PointsData[i];
		}
		m_Triangulation = Delaunay::Triangulate(vertices);
	}
}
