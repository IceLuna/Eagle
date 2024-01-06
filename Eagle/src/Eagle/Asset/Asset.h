#pragma once

#include "Eagle/Core/DataBuffer.h"
#include "Eagle/Core/GUID.h"
#include "Eagle/Renderer/RendererUtils.h"

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
		Scene // Not needed?
	};

	enum class AssetTexture2DFormat
	{
		RGBA8_UNorm, RGBA8_UNorm_SRGB, RGBA8_UInt, RGBA8_SNorm, RGBA8_SInt,
		RG8_UNorm, RG8_UNorm_SRGB, RG8_UInt, RG8_SNorm, RG8_SInt,
		R8_UNorm_SRGB, R8_UNorm, R8_UInt, R8_SNorm, R8_SInt,

		// TODO: check if these should be here
		BC1_UNorm, BC1_UNorm_SRGB,
		BC2_UNorm, BC2_UNorm_SRGB,
		BC3_UNorm, BC3_UNorm_SRGB,
		BC4_UNorm, BC4_SNorm,
		BC5_UNorm, BC5_SNorm,
		BC6H_UFloat16, BC6H_SFloat16,
		BC7_UNorm, BC7_UNorm_SRGB,

		Default = RGBA8_UNorm
	};

	enum class AssetTextureCubeFormat
	{
		RGBA32_Float, RGBA32_UInt, RGBA32_SInt,
		RGBA16_Float, RGBA16_UNorm, RGBA16_UInt, RGBA16_SNorm, RGBA16_SInt,
		RGBA8_UNorm, RGBA8_UNorm_SRGB, RGBA8_UInt, RGBA8_SNorm, RGBA8_SInt,
		RG32_Float, RG32_UInt, RG32_SInt,
		RG16_Float, RG16_UNorm, RG16_UInt, RG16_SNorm, RG16_SInt,
		RG8_UNorm, RG8_UNorm_SRGB, RG8_UInt, RG8_SNorm, RG8_SInt,
		R32_Float, R32_UInt, R32_SInt,
		R16_Float, R16_UNorm, R16_UInt, R16_SNorm, R16_SInt,
		R8_UNorm_SRGB, R8_UNorm, R8_UInt, R8_SNorm, R8_SInt,
		R11G11B10_Float,

		BC1_UNorm, BC1_UNorm_SRGB,
		BC2_UNorm, BC2_UNorm_SRGB,
		BC3_UNorm, BC3_UNorm_SRGB,
		BC4_UNorm, BC4_SNorm,
		BC5_UNorm, BC5_SNorm,
		BC6H_UFloat16, BC6H_SFloat16,
		BC7_UNorm, BC7_UNorm_SRGB,

		Default = RGBA32_Float
	};

	static constexpr uint32_t AssetTextureFormatToChannels(AssetTexture2DFormat format)
	{
		using Format = AssetTexture2DFormat;
		switch (format)
		{
		case Format::RGBA8_UNorm:
		case Format::RGBA8_UNorm_SRGB:
		case Format::RGBA8_UInt:
		case Format::RGBA8_SNorm:
		case Format::RGBA8_SInt: return 4u;

		case Format::RG8_UNorm:
		case Format::RG8_UNorm_SRGB:
		case Format::RG8_UInt:
		case Format::RG8_SNorm:
		case Format::RG8_SInt: return 2u;
		case Format::R8_UNorm_SRGB:
		case Format::R8_UNorm:
		case Format::R8_UInt:
		case Format::R8_SNorm:
		case Format::R8_SInt: return 1u;

			// TODO: BC formats
		}

		assert(!"Invalid format");
		return 4;
	}

	static constexpr ImageFormat AssetTextureFormatToImageFormat(AssetTexture2DFormat format)
	{
		using Format = AssetTexture2DFormat;
		switch (format)
		{
		case Format::RGBA8_UNorm: return ImageFormat::R8G8B8A8_UNorm;
		case Format::RGBA8_UNorm_SRGB: return ImageFormat::R8G8B8A8_UNorm_SRGB;
		case Format::RGBA8_UInt: return ImageFormat::R8G8B8A8_UInt;
		case Format::RGBA8_SNorm: return ImageFormat::R8G8B8A8_SNorm;
		case Format::RGBA8_SInt: return ImageFormat::R8G8B8A8_SInt;

		case Format::RG8_UNorm: return ImageFormat::R8G8_UNorm;
		case Format::RG8_UNorm_SRGB: return ImageFormat::R8G8_UNorm_SRGB;
		case Format::RG8_UInt: return ImageFormat::R8G8_UInt;
		case Format::RG8_SNorm: return ImageFormat::R8G8_SNorm;
		case Format::RG8_SInt: return ImageFormat::R8G8_SInt;

		case Format::R8_UNorm_SRGB: return ImageFormat::R8_UNorm_SRGB;
		case Format::R8_UNorm: return ImageFormat::R8_UNorm;
		case Format::R8_UInt: return ImageFormat::R8_UInt;
		case Format::R8_SNorm: return ImageFormat::R8_SNorm;
		case Format::R8_SInt: return ImageFormat::R8_SInt;

		case Format::BC1_UNorm: return ImageFormat::BC1_UNorm;
		case Format::BC1_UNorm_SRGB: return ImageFormat::BC1_UNorm_SRGB;
		case Format::BC2_UNorm: return ImageFormat::BC2_UNorm;
		case Format::BC2_UNorm_SRGB: return ImageFormat::BC2_UNorm_SRGB;
		case Format::BC3_UNorm: return ImageFormat::BC3_UNorm;
		case Format::BC3_UNorm_SRGB: return ImageFormat::BC3_UNorm_SRGB;
		case Format::BC4_UNorm: return ImageFormat::BC4_UNorm;
		case Format::BC4_SNorm: return ImageFormat::BC4_SNorm;
		case Format::BC5_UNorm: return ImageFormat::BC5_UNorm;
		case Format::BC5_SNorm: return ImageFormat::BC5_SNorm;
		case Format::BC6H_UFloat16: return ImageFormat::BC6H_UFloat16;
		case Format::BC6H_SFloat16: return ImageFormat::BC6H_SFloat16;
		case Format::BC7_UNorm: return ImageFormat::BC7_UNorm;
		case Format::BC7_UNorm_SRGB: return ImageFormat::BC7_UNorm_SRGB;
		}

		assert(!"Invalid format");
		return ImageFormat::Unknown;
	}

	static constexpr uint32_t AssetTextureFormatToChannels(AssetTextureCubeFormat format)
	{
		using Format = AssetTextureCubeFormat;
		switch (format)
		{
		case Format::RGBA32_Float:
		case Format::RGBA32_UInt:
		case Format::RGBA32_SInt:

		case Format::RGBA16_Float:
		case Format::RGBA16_UNorm:
		case Format::RGBA16_UInt:
		case Format::RGBA16_SNorm:
		case Format::RGBA16_SInt:

		case Format::RGBA8_UNorm:
		case Format::RGBA8_UNorm_SRGB:
		case Format::RGBA8_UInt:
		case Format::RGBA8_SNorm:
		case Format::RGBA8_SInt: return 4u;

		case Format::RG32_Float:
		case Format::RG32_UInt:
		case Format::RG32_SInt:

		case Format::RG16_Float:
		case Format::RG16_UNorm:
		case Format::RG16_UInt:
		case Format::RG16_SNorm:
		case Format::RG16_SInt:

		case Format::RG8_UNorm:
		case Format::RG8_UNorm_SRGB:
		case Format::RG8_UInt:
		case Format::RG8_SNorm:
		case Format::RG8_SInt: return 2u;

		case Format::R32_Float:
		case Format::R32_UInt:
		case Format::R32_SInt:

		case Format::R16_Float:
		case Format::R16_UNorm:
		case Format::R16_UInt:
		case Format::R16_SNorm:
		case Format::R16_SInt:

		case Format::R8_UNorm_SRGB:
		case Format::R8_UNorm:
		case Format::R8_UInt:
		case Format::R8_SNorm:
		case Format::R8_SInt: return 1u;

		case Format::R11G11B10_Float: return 3u;

			// TODO: BC formats
		}

		assert(!"Invalid format");
		return 4;
	}

	static constexpr ImageFormat AssetTextureFormatToImageFormat(AssetTextureCubeFormat format)
	{
		using Format = AssetTextureCubeFormat;
		switch (format)
		{
		case Format::RGBA32_Float: return ImageFormat::R32G32B32A32_Float;
		case Format::RGBA32_UInt: return ImageFormat::R32G32B32A32_UInt;
		case Format::RGBA32_SInt: return ImageFormat::R32G32B32A32_SInt;

		case Format::RGBA16_Float: return ImageFormat::R16G16B16A16_Float;
		case Format::RGBA16_UNorm: return ImageFormat::R16G16B16A16_UNorm;
		case Format::RGBA16_UInt: return ImageFormat::R16G16B16A16_UInt;
		case Format::RGBA16_SNorm: return ImageFormat::R16G16B16A16_SNorm;
		case Format::RGBA16_SInt: return ImageFormat::R16G16B16A16_SInt;

		case Format::RGBA8_UNorm: return ImageFormat::R8G8B8A8_UNorm;
		case Format::RGBA8_UNorm_SRGB: return ImageFormat::R8G8B8A8_UNorm_SRGB;
		case Format::RGBA8_UInt: return ImageFormat::R8G8B8A8_UInt;
		case Format::RGBA8_SNorm: return ImageFormat::R8G8B8A8_SNorm;
		case Format::RGBA8_SInt: return ImageFormat::R8G8B8A8_SInt;

		case Format::RG32_Float: return ImageFormat::R32G32_Float;
		case Format::RG32_UInt: return ImageFormat::R32G32_UInt;
		case Format::RG32_SInt: return ImageFormat::R32G32_SInt;

		case Format::RG16_Float: return ImageFormat::R16G16_Float;
		case Format::RG16_UNorm: return ImageFormat::R16G16_UNorm;
		case Format::RG16_UInt: return ImageFormat::R16G16_UInt;
		case Format::RG16_SNorm: return ImageFormat::R16G16_SNorm;
		case Format::RG16_SInt: return ImageFormat::R16G16_SInt;

		case Format::RG8_UNorm: return ImageFormat::R8G8_UNorm;
		case Format::RG8_UNorm_SRGB: return ImageFormat::R8G8_UNorm_SRGB;
		case Format::RG8_UInt: return ImageFormat::R8G8_UInt;
		case Format::RG8_SNorm: return ImageFormat::R8G8_SNorm;
		case Format::RG8_SInt: return ImageFormat::R8G8_SInt;

		case Format::R32_Float: return ImageFormat::R32_Float;
		case Format::R32_UInt: return ImageFormat::R32_UInt;
		case Format::R32_SInt: return ImageFormat::R32_SInt;

		case Format::R16_Float: return ImageFormat::R16_Float;
		case Format::R16_UNorm: return ImageFormat::R16_UNorm;
		case Format::R16_UInt: return ImageFormat::R16_UInt;
		case Format::R16_SNorm: return ImageFormat::R16_SNorm;
		case Format::R16_SInt: return ImageFormat::R16_SInt;

		case Format::R8_UNorm_SRGB: return ImageFormat::R8_UNorm_SRGB;
		case Format::R8_UNorm: return ImageFormat::R8_UNorm;
		case Format::R8_UInt: return ImageFormat::R8_UInt;
		case Format::R8_SNorm: return ImageFormat::R8_SNorm;
		case Format::R8_SInt: return ImageFormat::R8_SInt;

		case Format::R11G11B10_Float: return ImageFormat::R11G11B10_Float;

		case Format::BC1_UNorm: return ImageFormat::BC1_UNorm;
		case Format::BC1_UNorm_SRGB: return ImageFormat::BC1_UNorm_SRGB;
		case Format::BC2_UNorm: return ImageFormat::BC2_UNorm;
		case Format::BC2_UNorm_SRGB: return ImageFormat::BC2_UNorm_SRGB;
		case Format::BC3_UNorm: return ImageFormat::BC3_UNorm;
		case Format::BC3_UNorm_SRGB: return ImageFormat::BC3_UNorm_SRGB;
		case Format::BC4_UNorm: return ImageFormat::BC4_UNorm;
		case Format::BC4_SNorm: return ImageFormat::BC4_SNorm;
		case Format::BC5_UNorm: return ImageFormat::BC5_UNorm;
		case Format::BC5_SNorm: return ImageFormat::BC5_SNorm;
		case Format::BC6H_UFloat16: return ImageFormat::BC6H_UFloat16;
		case Format::BC6H_SFloat16: return ImageFormat::BC6H_SFloat16;
		case Format::BC7_UNorm: return ImageFormat::BC7_UNorm;
		case Format::BC7_UNorm_SRGB: return ImageFormat::BC7_UNorm_SRGB;
		}

		assert(!"Invalid format");
		return ImageFormat::Unknown;
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
		Asset(const Path& path, const Path& pathToRaw, AssetType type, GUID guid, const DataBuffer& rawData)
			: m_Path(path), m_PathToRaw(pathToRaw), m_GUID(guid), m_Type(type)
		{
			if (rawData.Size)
			{
				m_RawData.Allocate(rawData.Size);
				m_RawData.Write(rawData.Data, rawData.Size);
			}
		}

	protected:
		Path m_Path;
		Path m_PathToRaw;
		GUID m_GUID;
		AssetType m_Type = AssetType::None;
		ScopedDataBuffer m_RawData;
		bool bDirty = false; // Used for indication in UI that this assets needs to be saved
	};

	class AssetTexture2D : public Asset
	{
	public:
		const Ref<Texture2D>& GetTexture() const { return m_Texture; }
		AssetTexture2DFormat GetFormat() const { return m_Format; }

		AssetTexture2D& operator=(Asset&& other) noexcept override
		{
			if (this == &other)
				return *this;

			Asset::operator=(std::move(other));
				
			AssetTexture2D&& textureAsset = (AssetTexture2D&&)other;
			m_Texture = std::move(textureAsset.m_Texture);
			m_Format = std::move(textureAsset.m_Format);

			return *this;
		}

		// @path. Path to an `.egasset` file
		static Ref<AssetTexture2D> Create(const Path& path);

	protected:
		AssetTexture2D(const Path& path, const Path& pathToRaw, GUID guid, const DataBuffer& rawData, const Ref<Texture2D>& texture, AssetTexture2DFormat format)
			: Asset(path, pathToRaw, AssetType::Texture2D, guid, rawData), m_Texture(texture),	m_Format(format) {}

	private:
		Ref<Texture2D> m_Texture;
		AssetTexture2DFormat m_Format;
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

	protected:
		AssetPhysicsMaterial(const Path& path, GUID guid, const Ref<PhysicsMaterial>& material)
			: Asset(path, {}, AssetType::PhysicsMaterial, guid, {}), m_Material(material) {}

	private:
		Ref<PhysicsMaterial> m_Material;
	};
}
