#pragma once

#include "Eagle/Core/DataBuffer.h"
#include "Eagle/Core/GUID.h"
#include "Eagle/Renderer/RendererUtils.h"

namespace YAML
{
	class Node;
}

namespace Eagle
{
	class Texture2D;
	class TextureCube;
	class Material;
	class StaticMesh;
	class Audio;
	class SoundGroup;
	class Font;
	class PhysicsMaterial;
	class Entity;
	class Scene;

	enum class AssetType
	{
		None,
		Texture2D,
		TextureCube,
		Mesh,
		Audio,
		SoundGroup,
		Font,
		Material,
		PhysicsMaterial,
		Entity,
		Scene
	};

	enum class AssetTexture2DFormat
	{
		RGBA8,
		RG8,
		R8,

		Default = RGBA8
	};

	enum class AssetTextureCubeFormat
	{
		RGBA32,
		RGBA16,
		RG32,
		RG16,
		R32,
		R16,
		R11G11B10_Float,

		Default = RGBA32
	};

	static constexpr uint32_t AssetTextureFormatToChannels(AssetTexture2DFormat format)
	{
		using Format = AssetTexture2DFormat;
		switch (format)
		{
			case Format::RGBA8: return 4u;
			case Format::RG8: return 2u;
			case Format::R8: return 1u;
		}

		EG_CORE_ASSERT(!"Invalid format");
		return 4;
	}

	static constexpr ImageFormat AssetTextureFormatToImageFormat(AssetTexture2DFormat format)
	{
		using Format = AssetTexture2DFormat;
		switch (format)
		{
		case Format::RGBA8: return ImageFormat::R8G8B8A8_UNorm;
		case Format::RG8: return ImageFormat::R8G8_UNorm;
		case Format::R8: return ImageFormat::R8_UNorm;
		}

		EG_CORE_ASSERT(!"Invalid format");
		return ImageFormat::Unknown;
	}

	static constexpr uint32_t AssetTextureFormatToChannels(AssetTextureCubeFormat format)
	{
		using Format = AssetTextureCubeFormat;
		switch (format)
		{
		case Format::RGBA32:
		case Format::RGBA16: return 4u;

		case Format::RG32:
		case Format::RG16: return 2u;

		case Format::R32:
		case Format::R16: return 1u;

		case Format::R11G11B10_Float: return 3u;
		}

		EG_CORE_ASSERT(!"Invalid format");
		return 4;
	}

	static constexpr ImageFormat AssetTextureFormatToImageFormat(AssetTextureCubeFormat format)
	{
		using Format = AssetTextureCubeFormat;
		switch (format)
		{
		case Format::RGBA32: return ImageFormat::R32G32B32A32_Float;
		case Format::RGBA16: return ImageFormat::R16G16B16A16_Float;

		case Format::RG32: return ImageFormat::R32G32_Float;
		case Format::RG16: return ImageFormat::R16G16_Float;

		case Format::R32: return ImageFormat::R32_Float;
		case Format::R16: return ImageFormat::R16_Float;

		case Format::R11G11B10_Float: return ImageFormat::R11G11B10_Float;
		}

		EG_CORE_ASSERT(!"Invalid format");
		return ImageFormat::Unknown;
	}

	static bool IsFloat16Format(AssetTextureCubeFormat format)
	{
		using Format = AssetTextureCubeFormat;
		switch (format)
		{
		case Format::RGBA16:
		case Format::RG16:
		case Format::R16: return true;
		}
		return false;
	}

	static constexpr const char* GetAssetDragDropCellTag(AssetType format)
	{
		switch (format)
		{
		case AssetType::Texture2D:
			return "TEXTURE_CELL";
		case AssetType::TextureCube:
			return "TEXTURE_CUBE_CELL";
		case AssetType::Mesh:
			return "MESH_CELL";
		case AssetType::Audio:
			return "SOUND_CELL";
		case AssetType::Font:
			return "FONT_CELL";
		case AssetType::Material:
			return "MATERIAL_CELL";
		case AssetType::PhysicsMaterial:
			return "PHYSICS_MATERIAL_CELL";
		case AssetType::SoundGroup:
			return "SOUND_GROUP_CELL";
		case AssetType::Entity:
			return "ENTITY_CELL";
		case AssetType::Scene:
			return "SCENE_CELL";
		default:
			return "INVALID_CELL";
		}
	}

	class Asset
	{
	public:
		virtual ~Asset() = default;

		const Path& GetPath() const { return m_Path; }
		const Path& GetPathToRaw() const { return m_PathToRaw; }
		GUID GetGUID() const { return m_GUID; }
		AssetType GetAssetType() const { return m_Type; }
		const ScopedDataBuffer& GetRawData() const { return m_RawData; }

		void SetDirty(bool dirty) { bDirty = dirty; }
		bool IsDirty() const { return bDirty; }

		virtual Asset& operator=(Asset&& other) noexcept
		{
			if (this == &other)
				return *this;

			m_Path = std::move(other.m_Path);
			m_PathToRaw = std::move(other.m_PathToRaw);
			m_GUID = std::move(other.m_GUID);
			m_Type = std::move(other.m_Type);
			m_RawData = std::move(other.m_RawData);

			return *this;
		}

		// This function can be used if you don't care about the asset type and you just want to load it
		// @path. Path to an `.egasset` file
		static Ref<Asset> Create(const Path& path);

		static void Save(const Ref<Asset>& asset);

		// @bReloadRawData. When set to true, raw data will be reloaded (for example, from the original .png file)
		static void Reload(Ref<Asset>& asset, bool bReloadRawData);

		static const char* GetExtension() { return ".egasset"; }

	protected:
		Asset(const Path& path, const Path& pathToRaw, AssetType type, GUID guid, const DataBuffer& rawData);

	protected:
		Path m_Path;
		Path m_PathToRaw;
		GUID m_GUID;
		AssetType m_Type = AssetType::None;
		ScopedDataBuffer m_RawData;
		bool bDirty = false; // Used for indication in UI that this assets needs to be saved

		friend class AssetManager;
	};

	class AssetTexture2D : public Asset
	{
	public:
		// Can be used to generate new mips: asset->SetIsCompressed(asset->IsCompressed(), newMipsCount);
		void SetIsCompressed(bool bCompressed, uint32_t mipsCount);
		void SetIsNormalMap(bool bNormalMap);
		void SetNeedsAlpha(bool bNeedAlpha);

		const ScopedDataBuffer& GetKTXData() const { return m_KtxData; }
		const Ref<Texture2D>& GetTexture() const { return m_Texture; }
		AssetTexture2DFormat GetFormat() const { return m_Format; }
		bool IsCompressed() const { return bCompressed; }
		bool IsNormalMap() const { return bNormalMap; }
		bool DoesNeedAlpha() const { return bNeedAlpha; }

		AssetTexture2D& operator=(Asset&& other) noexcept override
		{
			if (this == &other)
				return *this;

			Asset::operator=(std::move(other));
				
			AssetTexture2D&& textureAsset = (AssetTexture2D&&)other;
			m_KtxData = std::move(textureAsset.m_KtxData);
			m_Texture = std::move(textureAsset.m_Texture);
			m_Format = std::move(textureAsset.m_Format);
			bCompressed = std::move(textureAsset.bCompressed);
			bNormalMap = std::move(textureAsset.bNormalMap);
			bNeedAlpha = std::move(textureAsset.bNeedAlpha);

			return *this;
		}

		// @path. Path to an `.egasset` file
		static Ref<AssetTexture2D> Create(const Path& path);

		static AssetType GetAssetType_Static() { return AssetType::Texture2D; }

	protected:
		AssetTexture2D(const Path& path, const Path& pathToRaw, GUID guid, const DataBuffer& rawData, const DataBuffer& ktxData, const Ref<Texture2D>& texture,
			AssetTexture2DFormat format, bool bCompressed, bool bNormalMap, bool bNeedAlpha);

		void SetKTXData(const DataBuffer& buffer)
		{
			if (buffer.Data)
				m_KtxData = DataBuffer::Copy(buffer.Data, buffer.Size);
			else
				m_KtxData.Release();
		}

		void UpdateTextureData_Internal(bool bCompressed, uint32_t mipsCount);

	private:
		ScopedDataBuffer m_KtxData;
		Ref<Texture2D> m_Texture;
		AssetTexture2DFormat m_Format;
		bool bCompressed = true;
		bool bNormalMap = false;
		bool bNeedAlpha = false;
	};

	class AssetTextureCube : public Asset
	{
	public:
		const Ref<TextureCube>& GetTexture() const { return m_Texture; }
		AssetTextureCubeFormat GetFormat() const { return m_Format; }

		AssetTextureCube& operator=(Asset&& other) noexcept override
		{
			if (this == &other)
				return *this;

			Asset::operator=(std::move(other));

			AssetTextureCube&& textureAsset = (AssetTextureCube&&)other;
			m_Texture = std::move(textureAsset.m_Texture);
			m_Format = std::move(textureAsset.m_Format);

			return *this;
		}

		// @path. Path to an `.egasset` file
		static Ref<AssetTextureCube> Create(const Path& path);

		static AssetType GetAssetType_Static() { return AssetType::TextureCube; }

	protected:
		AssetTextureCube(const Path& path, const Path& pathToRaw, GUID guid, const DataBuffer& rawData, const Ref<TextureCube>& texture, AssetTextureCubeFormat format)
			: Asset(path, pathToRaw, AssetType::TextureCube, guid, rawData), m_Texture(texture), m_Format(format) {}

	private:
		Ref<TextureCube> m_Texture;
		AssetTextureCubeFormat m_Format;
	};

	class AssetMesh : public Asset
	{
	public:
		const Ref<StaticMesh>& GetMesh() const { return m_Mesh; }
		uint32_t GetMeshIndex() const { return m_MeshIndex; }

		AssetMesh& operator=(Asset&& other) noexcept override
		{
			if (this == &other)
				return *this;

			Asset::operator=(std::move(other));

			AssetMesh&& meshAsset = (AssetMesh&&)other;
			m_Mesh = std::move(meshAsset.m_Mesh);
			m_MeshIndex = std::move(meshAsset.m_MeshIndex);

			return *this;
		}

		// @path. Path to an `.egasset` file
		static Ref<AssetMesh> Create(const Path& path);

		static AssetType GetAssetType_Static() { return AssetType::Mesh; }

	protected:
		AssetMesh(const Path& path, const Path& pathToRaw, GUID guid, const Ref<StaticMesh>& mesh, uint32_t meshIndex)
			: Asset(path, pathToRaw, AssetType::Mesh, guid, {}), m_Mesh(mesh), m_MeshIndex(meshIndex) {}

	private:
		Ref<StaticMesh> m_Mesh;
		uint32_t m_MeshIndex = 0u; // Index of a mesh within a mesh file, since it can contain multiple of them
	};

	class AssetSoundGroup : public Asset
	{
	public:
		const Ref<SoundGroup>& GetSoundGroup() const { return m_SoundGroup; }

		AssetSoundGroup& operator=(Asset&& other) noexcept override
		{
			if (this == &other)
				return *this;

			Asset::operator=(std::move(other));

			AssetSoundGroup&& soundAsset = (AssetSoundGroup&&)other;
			m_SoundGroup = std::move(soundAsset.m_SoundGroup);

			return *this;
		}

		// @path. Path to an `.egasset` file
		static Ref<AssetSoundGroup> Create(const Path& path);

		static AssetType GetAssetType_Static() { return AssetType::SoundGroup; }

	protected:
		AssetSoundGroup(const Path& path, GUID guid, const Ref<SoundGroup>& group)
			: Asset(path, {}, AssetType::SoundGroup, guid, {}), m_SoundGroup(group) {}

	private:
		Ref<SoundGroup> m_SoundGroup;
	};

	class AssetAudio : public Asset
	{
	public:
		const Ref<Audio>& GetAudio() const { return m_Audio; }
		const Ref<AssetSoundGroup>& GetSoundGroupAsset() const { return m_SoundGroup; }

		void SetSoundGroupAsset(const Ref<AssetSoundGroup>& soundGroup);

		AssetAudio& operator=(Asset&& other) noexcept override
		{
			if (this == &other)
				return *this;

			Asset::operator=(std::move(other));

			AssetAudio&& soundAsset = (AssetAudio&&)other;
			m_Audio = std::move(soundAsset.m_Audio);
			m_SoundGroup = std::move(soundAsset.m_SoundGroup);

			return *this;
		}

		// @path. Path to an `.egasset` file
		static Ref<AssetAudio> Create(const Path& path);

		static AssetType GetAssetType_Static() { return AssetType::Audio; }

	protected:
		AssetAudio(const Path& path, const Path& pathToRaw, GUID guid, const DataBuffer& rawData, const Ref<Audio>& audio, const Ref<AssetSoundGroup>& soundGroup)
			: Asset(path, pathToRaw, AssetType::Audio, guid, rawData), m_Audio(audio)
		{
			SetSoundGroupAsset(soundGroup);
		}

	private:
		Ref<Audio> m_Audio;
		Ref<AssetSoundGroup> m_SoundGroup;
	};

	class AssetFont : public Asset
	{
	public:
		const Ref<Font>& GetFont() const { return m_Font; }

		AssetFont& operator=(Asset&& other) noexcept override
		{
			if (this == &other)
				return *this;

			Asset::operator=(std::move(other));

			AssetFont&& fontAsset = (AssetFont&&)other;
			m_Font = std::move(fontAsset.m_Font);

			return *this;
		}

		// @path. Path to an `.egasset` file
		static Ref<AssetFont> Create(const Path& path);

		static AssetType GetAssetType_Static() { return AssetType::Font; }

	protected:
		AssetFont(const Path& path, const Path& pathToRaw, GUID guid, const DataBuffer& rawData, const Ref<Font>& font)
			: Asset(path, pathToRaw, AssetType::Font, guid, rawData), m_Font(font) {}

	private:
		Ref<Font> m_Font;
	};

	class AssetMaterial : public Asset
	{
	public:
		const Ref<Material>& GetMaterial() const { return m_Material; }

		AssetMaterial& operator=(Asset&& other) noexcept override
		{
			if (this == &other)
				return *this;

			Asset::operator=(std::move(other));

			AssetMaterial&& materialAsset = (AssetMaterial&&)other;
			m_Material = std::move(materialAsset.m_Material);

			return *this;
		}

		// @path. Path to an `.egasset` file
		static Ref<AssetMaterial> Create(const Path& path);

		static AssetType GetAssetType_Static() { return AssetType::Material; }

	protected:
		AssetMaterial(const Path& path, GUID guid, const Ref<Material>& material)
			: Asset(path, {}, AssetType::Material, guid, {}), m_Material(material) {}

	private:
		Ref<Material> m_Material;
	};

	class AssetPhysicsMaterial : public Asset
	{
	public:
		const Ref<PhysicsMaterial>& GetMaterial() const { return m_Material; }

		AssetPhysicsMaterial& operator=(Asset&& other) noexcept override
		{
			if (this == &other)
				return *this;

			Asset::operator=(std::move(other));

			AssetPhysicsMaterial&& materialAsset = (AssetPhysicsMaterial&&)other;
			m_Material = std::move(materialAsset.m_Material);

			return *this;
		}

		// @path. Path to an `.egasset` file
		static Ref<AssetPhysicsMaterial> Create(const Path& path);

		static AssetType GetAssetType_Static() { return AssetType::PhysicsMaterial; }

	protected:
		AssetPhysicsMaterial(const Path& path, GUID guid, const Ref<PhysicsMaterial>& material)
			: Asset(path, {}, AssetType::PhysicsMaterial, guid, {}), m_Material(material) {}

	private:
		Ref<PhysicsMaterial> m_Material;
	};

	class AssetEntity : public Asset
	{
	public:
		const Ref<Entity>& GetEntity() const { return m_Entity; }

		AssetEntity& operator=(Asset&& other) noexcept override
		{
			if (this == &other)
				return *this;

			Asset::operator=(std::move(other));

			AssetEntity&& entityAsset = (AssetEntity&&)other;
			m_Entity = std::move(entityAsset.m_Entity);

			return *this;
		}

		// @path. Path to an `.egasset` file
		static Ref<AssetEntity> Create(const Path& path);

		static Entity CreateEntity(GUID guid);

		static AssetType GetAssetType_Static() { return AssetType::Entity; }

	protected:
		AssetEntity(const Path& path, GUID guid, const Ref<Entity>& entity)
			: Asset(path, {}, AssetType::Entity, guid, {}), m_Entity(entity) {}

	private:
		Ref<Entity> m_Entity;
		static Ref<Scene> s_EntityAssetsScene;

		friend class AssetManager;
	};

	class AssetScene : public Asset
	{
	public:
		// @path. Path to an `.egasset` file
		static Ref<AssetScene> Create(const Path& path);
		static Ref<AssetScene> Create(const Path& path, const YAML::Node& data);

		static AssetType GetAssetType_Static() { return AssetType::Scene; }

	protected:
		AssetScene(const Path& path, GUID guid)
			: Asset(path, {}, AssetType::Scene, guid, {}) {}

		friend class AssetManager;
	};
}
