#pragma once

#include "Eagle/Renderer/VidWrappers/Texture.h"
#include "VulkanImage.h"

namespace Eagle
{
	class VulkanFramebuffer;

	class VulkanTextureCube : public TextureCube
	{
	public:
		VulkanTextureCube(const Ref<Texture2D>& texture, uint32_t layerSize);
		VulkanTextureCube(const Path& filepath, uint32_t layerSize);

	private:
		void GenerateIBL();

	private:
		std::array<Ref<VulkanFramebuffer>, 6> m_IrradianceFramebuffers;
		std::vector<std::array<Ref<VulkanFramebuffer>, 6>> m_PrefilterFramebuffers;
	};
}
