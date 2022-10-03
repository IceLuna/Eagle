#pragma once

#include "Vulkan.h"
#include "VulkanContext.h"
#include "Eagle/Renderer/Semaphore.h"

namespace Eagle
{
	class VulkanSemaphore : public Semaphore
	{
	public:
		VulkanSemaphore()
		{
			m_Device = VulkanContext::GetDevice()->GetVulkanDevice();
			VkSemaphoreCreateInfo ci{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
			VK_CHECK(vkCreateSemaphore(m_Device, &ci, nullptr, &m_Semaphore));
		}

		virtual ~VulkanSemaphore()
		{
			if (m_Semaphore)
			{
				Renderer::SubmitResourceFree([device = m_Device, semaphore = m_Semaphore]()
				{
					vkDestroySemaphore(device, semaphore, nullptr);
				});
			}
		}

		void* GetHandle() const override { return m_Semaphore; }

	private:
		VkDevice m_Device;
		VkSemaphore m_Semaphore = VK_NULL_HANDLE;
	};
}
