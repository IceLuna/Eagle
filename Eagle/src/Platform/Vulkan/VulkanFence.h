#pragma once

#include "Vulkan.h"
#include "VulkanContext.h"
#include "Eagle/Renderer/VidWrappers/Fence.h"

namespace Eagle
{
	class VulkanFence : public Fence
	{
	public:
		VulkanFence(bool bSignaled = false)
		{
			m_Device = VulkanContext::GetDevice()->GetVulkanDevice();

			VkFenceCreateInfo ci{};
			ci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
			ci.flags = bSignaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0;

			VK_CHECK(vkCreateFence(m_Device, &ci, nullptr, &m_Fence));
		}

		virtual ~VulkanFence()
		{
			if (m_Fence)
			{
				Renderer::SubmitResourceFree([device = m_Device, fence = m_Fence]() {
					vkDestroyFence(device, fence, nullptr);
				});
				m_Fence = VK_NULL_HANDLE;
			}
		}

		void* GetHandle() const override { return m_Fence; }
		bool IsSignaled() const override
		{
			VkResult result = vkGetFenceStatus(m_Device, m_Fence);
			if (result == VK_SUCCESS)
				return true;
			else if (result == VK_NOT_READY)
				return false;

			EG_CORE_ASSERT(false);
			return false;
		}
		void Reset() override { VK_CHECK(vkResetFences(m_Device, 1, &m_Fence)); }
		void Wait(uint64_t timeout = UINT64_MAX) const override { VK_CHECK(vkWaitForFences(m_Device, 1, &m_Fence, VK_FALSE, timeout)); }

	private:
		VkDevice m_Device;
		VkFence m_Fence = VK_NULL_HANDLE;
	};
}
