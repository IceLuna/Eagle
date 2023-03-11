#pragma once

#include "Eagle/Core/GUID.h"
#include "Image.h"
#include "Sampler.h"
#include <glm/glm.hpp>

namespace Eagle
{
	class Texture
	{
	protected:
		Texture(const Path& path)
			: m_Path(path) {}
		Texture(ImageFormat format, glm::uvec3 size)
			: m_Path(), m_Format(format), m_Size(size) {}

		bool Load(const Path& path);

	public:
		virtual ~Texture() = default;

		virtual bool IsLoaded() const = 0;

		Ref<Image>& GetImage() { return m_Image; }
		const Ref<Image>& GetImage() const { return m_Image; }
		Ref<Sampler>& GetSampler() { return m_Sampler; }
		const Ref<Sampler>& GetSampler() const { return m_Sampler; }

		ImageFormat GetFormat() const { return m_Format; }
		const glm::uvec3& GetSize() const { return m_Size; }
		uint32_t GetWidth() const { return m_Size.x; }
		uint32_t GetHeight() const { return m_Size.y; }
		uint32_t GetDepth() const { return m_Size.z; }

		const Path& GetPath() const { return m_Path; }
		const GUID& GetGUID() const { return m_GUID; }

	protected:
		Path m_Path;
		Ref<Image> m_Image = nullptr;
		Ref<Sampler> m_Sampler = nullptr;
		ScopedDataBuffer m_ImageData;
		GUID m_GUID;
		ImageFormat m_Format = ImageFormat::Unknown;
		glm::uvec3 m_Size = glm::uvec3(0, 0, 0);

		friend class RenderManager;
	};

	struct Texture2DSpecifications
	{
		FilterMode FilterMode = FilterMode::Bilinear;
		AddressMode AddressMode = AddressMode::Wrap;
		SamplesCount SamplesCount = SamplesCount::Samples1;
		float MaxAnisotropy = 1.f;
		bool bGenerateMips = false;
	};

	class Texture2D : public Texture
	{
	protected:
		Texture2D(const Path& path, const Texture2DSpecifications& specs = {}) : Texture(path), m_Specs(specs) {}
		Texture2D(ImageFormat format, glm::uvec2 size, const Texture2DSpecifications& specs)
			: Texture(format, { size, 1u })
			, m_Specs(specs) {}

	public:
		static Ref<Texture2D> Create(const Path& path, const Texture2DSpecifications& specs = {}, bool bAddToLib = true);
		static Ref<Texture2D> Create(ImageFormat format, glm::uvec2 size, const void* data = nullptr, const Texture2DSpecifications& specs = {}, bool bAddToLib = true);

		//TODO: Remove
		static Ref<Texture2D> DummyTexture;
		static Ref<Texture2D> WhiteTexture;
		static Ref<Texture2D> BlackTexture;
		static Ref<Texture2D> NoneIconTexture;
		static Ref<Texture2D> PointLightIcon;
		static Ref<Texture2D> DirectionalLightIcon;
		static Ref<Texture2D> SpotLightIcon;

	protected:
		Texture2DSpecifications m_Specs;
	};

	class Framebuffer;
	class TextureCube : public Texture
	{
	public:
		TextureCube(const Path& path, uint32_t layerSize) : Texture(path)
		{
			m_Size = glm::uvec3(layerSize, layerSize, 1);
		}

		TextureCube(const Ref<Texture2D>& texture, uint32_t layerSize) 
			: Texture("")
			, m_Texture2D(texture)
		{
			m_Path = m_Texture2D->GetPath();
			m_Size = glm::uvec3(layerSize, layerSize, 1);
		}

		const Ref<Texture2D>& GetTexture2D() const { return m_Texture2D; };
		const Ref<Framebuffer>& GetFramebuffer(uint32_t layerIndex) { EG_ASSERT(layerIndex < 6); return m_Framebuffers[layerIndex]; }

		Ref<Image>& GetIrradianceImage() { return m_IrradianceImage; }
		const Ref<Image>& GetIrradianceImage() const { return m_IrradianceImage; }
		const Ref<Image>& GetPrefilterImage() const { return m_PrefilterImage; }
		const Ref<Sampler>& GetPrefilterImageSampler() const { return m_PrefilterImageSampler; }

		bool IsLoaded() const override { return m_Texture2D->IsLoaded(); }

		static Ref<TextureCube> Create(const Path& path, uint32_t layerSize, bool bAddToLibrary = true);
		static Ref<TextureCube> Create(const Ref<Texture2D>& texture, uint32_t layerSize, bool bAddToLibrary = true);

		static constexpr uint32_t SkyboxSize = 1024;
		static constexpr uint32_t IrradianceSize = 32;
		static constexpr uint32_t PrefilterSize = 512;

	protected:
		Ref<Image> m_IrradianceImage;
		Ref<Image> m_PrefilterImage;
		Ref<Sampler> m_PrefilterImageSampler;
		Ref<Texture2D> m_Texture2D;
		std::array<Ref<Framebuffer>, 6> m_Framebuffers;
	};

	class TextureLibrary
	{
	public:
		static void Add(const Ref<Texture>& texture)
		{
			s_Textures.push_back(texture);
			s_TexturePathHashes.push_back(std::hash<Path>()(texture->GetPath()));
#ifdef EG_DEBUG
			s_TexturePaths.push_back(texture->GetPath());
#endif
		}
		static bool Get(const Path& path, Ref<Texture>* outTexture);
		static bool Get(const GUID& guid, Ref<Texture>* outTexture);
		static bool Exist(const Path& path);
		static bool Exist(const GUID& guid);

		static const std::vector<Ref<Texture>>& GetTextures() { return s_Textures; }

	private:
		TextureLibrary() = default;
		TextureLibrary(const TextureLibrary&) = default;

		//TODO: Move to AssetManager::Shutdown()
		static void Clear() { s_Textures.clear(); s_TexturePathHashes.clear(); }
		friend class RenderManager;
		
		static std::vector<Ref<Texture>> s_Textures;
		static std::vector<size_t> s_TexturePathHashes;

#ifdef EG_DEBUG
		static std::vector<Path> s_TexturePaths;
#endif
	};
}
