#pragma once

#include "Eagle/Renderer/VidWrappers/Texture.h"
#include "VulkanImage.h"

namespace Eagle
{
	class VulkanTexture2D : public Texture2D, public std::enable_shared_from_this<VulkanTexture2D>
	{
	public:
		VulkanTexture2D(ImageFormat format, glm::uvec2 size, const std::string& debugName = "");
		VulkanTexture2D(ImageFormat format, glm::uvec2 size, const void* data = nullptr, const Texture2DSpecifications& specs = {}, const std::string& debugName = "");
		VulkanTexture2D(ImageFormat format, glm::uvec2 size, const std::vector<ScopedDataBuffer>& dataPerMip, const Texture2DSpecifications& specs = {}, const std::string& debugName = "");

		bool IsLoaded() const override { return m_bIsLoaded; }

		void SetAnisotropy(float anisotropy) override;
		void SetFilterMode(FilterMode filterMode) override;
		void SetAddressMode(AddressMode addressMode) override;
		void GenerateMips(uint32_t mipsCount) override;
		void SetData(DataBuffer data, ImageFormat format) override;
		void SetData(const std::vector<ScopedDataBuffer>& dataPerMip, ImageFormat format) override;

		void CreateImageFromData(bool bAutogenerateMips);
	private:
		Ref<Image> CreateImage();

	private:
		// Temporary data storage. It'll be freed after its uploaded to the GPU
		std::vector<ScopedDataBuffer> m_ImageData;
		std::string m_DebugName;
		bool m_bIsLoaded = false;
	};
}
