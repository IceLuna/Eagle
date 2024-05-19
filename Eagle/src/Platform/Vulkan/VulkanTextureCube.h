#pragma once

#include "Eagle/Renderer/VidWrappers/Texture.h"
#include "VulkanImage.h"

namespace Eagle
{
	class VulkanFramebuffer;

	class VulkanTextureCube : public TextureCube, public std::enable_shared_from_this<VulkanTextureCube>
	{
	public:
		VulkanTextureCube(const Ref<Texture2D>& texture, uint32_t layerSize, uint32_t prefilterSize);
		VulkanTextureCube(const std::string& name, ImageFormat format, const void* data, glm::uvec2 size, uint32_t layerSize, uint32_t prefilterSize);

		void SetLayerSize(uint32_t layerSize) override;
		void SetPrefilterSize(uint32_t prefilterSize) override;
		void GenerateIBL();

	private:
		Ref<Sampler> m_CubemapSampler;
		std::array<Ref<Framebuffer>, 6> m_Framebuffers;
		std::array<Ref<VulkanFramebuffer>, 6> m_IrradianceFramebuffers;
		std::vector<std::array<Ref<VulkanFramebuffer>, 6>> m_PrefilterFramebuffers;
	};
}
