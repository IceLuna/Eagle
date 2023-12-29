#pragma once

#include "Eagle/Renderer/VidWrappers/Texture.h"
#include "VulkanImage.h"

namespace Eagle
{
	class VulkanTexture2D : public Texture2D, public std::enable_shared_from_this<VulkanTexture2D>
	{
	public:
		VulkanTexture2D(ImageFormat format, glm::uvec2 size, const void* data = nullptr, const Texture2DSpecifications& specs = {}, const std::string& debugName = "");

		bool IsLoaded() const override { return m_bIsLoaded; }

		void SetAnisotropy(float anisotropy) override;
		void SetFilterMode(FilterMode filterMode) override;
		void SetAddressMode(AddressMode addressMode) override;
		void GenerateMips(uint32_t mipsCount) override;

	private:
		void CreateImageFromData();

	private:
		std::string m_DebugName;
		bool m_bIsLoaded = false;
	};
}
