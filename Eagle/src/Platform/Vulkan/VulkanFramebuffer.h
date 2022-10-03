#pragma once

#include "Eagle/Renderer/Framebuffer.h"
#include "Vulkan.h"

namespace Eagle
{
	class VulkanFramebuffer : public Framebuffer
	{
	public:
		VulkanFramebuffer(const std::vector<Ref<Image>>& images, glm::uvec2 size, const void* renderPassHandle);
		virtual ~VulkanFramebuffer();

		void* GetHandle() const override { return m_Framebuffer; }

	private:
		VkDevice m_Device = VK_NULL_HANDLE;
		VkFramebuffer m_Framebuffer = VK_NULL_HANDLE;
	};
}
