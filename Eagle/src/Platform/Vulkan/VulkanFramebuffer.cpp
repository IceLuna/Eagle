#include "egpch.h"
#include "VulkanFramebuffer.h"
#include "VulkanContext.h"
#include "VulkanImage.h"
#include "VulkanUtils.h"

namespace Eagle
{
	VulkanFramebuffer::VulkanFramebuffer(const std::vector<Ref<Image>>& images, glm::uvec2 size, const void* renderPassHandle)
		: Framebuffer(images, size, renderPassHandle)
	{
		assert(m_Images.size());

		m_Device = VulkanContext::GetDevice()->GetVulkanDevice();

		const size_t imagesCount = m_Images.size();
		std::vector<VkImageView> imageViews(imagesCount);
		for (size_t i = 0; i < imagesCount; ++i)
			imageViews[i] = (VkImageView)m_Images[i]->GetImageViewHandle();

		VkFramebufferCreateInfo framebufferCI{};
		framebufferCI.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferCI.attachmentCount = (uint32_t)imageViews.size();
		framebufferCI.pAttachments = imageViews.data();
		framebufferCI.renderPass = (VkRenderPass)m_RenderPassHandle;
		framebufferCI.width = m_Size.x;
		framebufferCI.height = m_Size.y;
		framebufferCI.layers = 1;
		VK_CHECK(vkCreateFramebuffer(m_Device, &framebufferCI, nullptr, &m_Framebuffer));
	}

	VulkanFramebuffer::VulkanFramebuffer(const Ref<Image>& image, const ImageView& imageView, glm::uvec2 size, const void* renderPassHandle)
		: Framebuffer(image, imageView, size, renderPassHandle)
	{
		assert(m_Images.size());

		m_Device = VulkanContext::GetDevice()->GetVulkanDevice();

		VkImageView vkImageView = (VkImageView)image->GetImageViewHandle(imageView, true);

		VkFramebufferCreateInfo framebufferCI{};
		framebufferCI.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferCI.attachmentCount = 1u;
		framebufferCI.pAttachments = &vkImageView;
		framebufferCI.renderPass = (VkRenderPass)m_RenderPassHandle;
		framebufferCI.width = m_Size.x;
		framebufferCI.height = m_Size.y;
		framebufferCI.layers = 1;
		VK_CHECK(vkCreateFramebuffer(m_Device, &framebufferCI, nullptr, &m_Framebuffer));
	}

	VulkanFramebuffer::~VulkanFramebuffer()
	{
		if (m_Framebuffer)
		{
			RenderManager::SubmitResourceFree([device = m_Device, framebuffer = m_Framebuffer]()
			{
				vkDestroyFramebuffer(device, framebuffer, nullptr);
			});
			m_Framebuffer = VK_NULL_HANDLE;
		}
	}
}
