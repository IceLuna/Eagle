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
		Texture() = default;
		Texture(ImageFormat format, glm::uvec3 size)
			: m_Format(format), m_Size(size) {}

	public:
		virtual ~Texture();

		Ref<Image>& GetImage() { return m_Image; }
		const Ref<Image>& GetImage() const { return m_Image; }
		Ref<Sampler>& GetSampler() { return m_Sampler; }
		const Ref<Sampler>& GetSampler() const { return m_Sampler; }

		ImageFormat GetFormat() const { return m_Format; }
		const glm::uvec3& GetSize() const { return m_Size; }
		uint32_t GetWidth() const { return m_Size.x; }
		uint32_t GetHeight() const { return m_Size.y; }
		uint32_t GetDepth() const { return m_Size.z; }

		const GUID& GetGUID() const { return m_GUID; }

	protected:
		Ref<Image> m_Image = nullptr;
		Ref<Sampler> m_Sampler = nullptr;
		GUID m_GUID;
		ImageFormat m_Format = ImageFormat::Unknown;
		glm::uvec3 m_Size = glm::uvec3(0, 0, 0);
	};

	struct Texture2DSpecifications
	{
		FilterMode FilterMode = FilterMode::Bilinear;
		AddressMode AddressMode = AddressMode::Wrap;
		SamplesCount SamplesCount = SamplesCount::Samples1;
		float MaxAnisotropy = 1.f;
		uint32_t MipsCount = 1;
	};

	class Texture2D : public Texture
	{
	protected:
		Texture2D(ImageFormat format, glm::uvec2 size, const Texture2DSpecifications& specs)
			: Texture(format, { size, 1u })
			, m_Specs(specs) {}

	public:
		// Dont forget to update `m_Specs`
		virtual void SetAnisotropy(float anisotropy) = 0;
		virtual void SetFilterMode(FilterMode filterMode) = 0;
		virtual void SetAddressMode(AddressMode addressMode) = 0;
		virtual void GenerateMips(uint32_t mipsCount) = 0;
		// @format. Set to unknown if you don't want to change it
		virtual void GenerateMips(const std::vector<DataBuffer>& dataPerMip, ImageFormat format = ImageFormat::Unknown) = 0;

		// Can be used to set data of the base level. Mips will be autogenerated if necessary
		virtual void SetData(const void* data, ImageFormat format) = 0;

		float GetAnisotropy() const { return m_Specs.MaxAnisotropy; }
		FilterMode GetFilterMode() const { return m_Specs.FilterMode; }
		AddressMode GetAddressMode() const { return m_Specs.AddressMode; }
		uint32_t GetMipsCount() const { return m_Specs.MipsCount; }

		virtual bool IsLoaded() const = 0;

		// Returns the GPU memory usage of the base mip
		size_t GetMemSize() const { return m_ImageData[0].Size(); }

	public:
		static Ref<Texture2D> Create(const Path& path, const Texture2DSpecifications& specs = {});

		// @name is assigned to m_Path so that in TextureLibrary we can differentiate it from other manually created textures
		static Ref<Texture2D> Create(const std::string& name, ImageFormat format, glm::uvec2 size, const void* data = nullptr, const Texture2DSpecifications& properties = {});
		// @dataPerMip. Data to upload to each mip of a GPU texture
		static Ref<Texture2D> Create(const std::string& name, ImageFormat format, glm::uvec2 size, const std::vector<DataBuffer>& dataPerMip, const Texture2DSpecifications& properties = {});

		//TODO: Remove
		static Ref<Texture2D> DummyTexture;
		static Ref<Texture2D> WhiteTexture;
		static Ref<Texture2D> BlackTexture;
		static Ref<Texture2D> GrayTexture;
		static Ref<Texture2D> RedTexture;
		static Ref<Texture2D> GreenTexture;
		static Ref<Texture2D> BlueTexture;
		static Ref<Texture2D> NoneIconTexture;
		static Ref<Texture2D> PointLightIcon;
		static Ref<Texture2D> DirectionalLightIcon;
		static Ref<Texture2D> SpotLightIcon;

	protected:
		Texture2DSpecifications m_Specs;
		std::vector<ScopedDataBuffer> m_ImageData; // Note: the size of this vector will be `1` if the mips were autogenerated
	};

	class Framebuffer;
	class TextureCube : public Texture
	{
	public:
		TextureCube(ImageFormat format, uint32_t layerSize)
			: Texture(format, glm::uvec3(layerSize, layerSize, 1u)) {}

		TextureCube(const Ref<Texture2D>& texture, uint32_t layerSize)
			: Texture(texture->GetFormat(), glm::uvec3(layerSize, layerSize, 1))
			, m_Texture2D(texture)
		{}

		virtual void SetLayerSize(uint32_t layerSize) = 0;

		const Ref<Texture2D>& GetTexture2D() const { return m_Texture2D; };

		Ref<Image>& GetIrradianceImage() { return m_IrradianceImage; }
		const Ref<Image>& GetIrradianceImage() const { return m_IrradianceImage; }
		const Ref<Image>& GetPrefilterImage() const { return m_PrefilterImage; }
		const Ref<Sampler>& GetPrefilterImageSampler() const { return m_PrefilterImageSampler; }

		bool IsLoaded() const { return m_Loaded; }

		static Ref<TextureCube> Create(const std::string& name, ImageFormat format, const void* data, glm::uvec2 size, uint32_t layerSize);
		static Ref<TextureCube> Create(const Ref<Texture2D>& texture, uint32_t layerSize);

		static constexpr uint32_t SkyboxSize = 1024;
		static constexpr uint32_t IrradianceSize = 32;
		static constexpr uint32_t PrefilterSize = 512;

	protected:
		Ref<Image> m_IrradianceImage;
		Ref<Image> m_PrefilterImage;
		Ref<Sampler> m_PrefilterImageSampler;
		Ref<Texture2D> m_Texture2D; // Null for game builds
		bool m_Loaded = false; // Set to false during IBL generation. Set to true, when IBL data is generated and ready to be used
	};
}
