#pragma once

#include "Vulkan.h"
#include "VulkanContext.h"
#include "VulkanUtils.h"
#include "Eagle/Renderer/Sampler.h"

namespace Eagle
{
	class VulkanSampler : public Sampler
	{
	public:
		VulkanSampler(FilterMode filterMode, AddressMode addressMode, CompareOperation compareOp, float minLod, float maxLod, float maxAnisotropy = 1.f)
			: Sampler(filterMode, addressMode, compareOp, minLod, maxLod, maxAnisotropy)
			, m_Device(VulkanContext::GetDevice()->GetVulkanDevice())
		{
			VkSamplerAddressMode vkAddressMode = AddressModeToVulkan(m_AddressMode);

			VkSamplerCreateInfo info{};
			info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			info.addressModeU = vkAddressMode;
			info.addressModeV = vkAddressMode;
			info.addressModeW = vkAddressMode;
			info.anisotropyEnable = m_MaxAnisotropy > 1.f ? VK_TRUE : VK_FALSE;
			info.maxAnisotropy = m_MaxAnisotropy;
			info.minLod = m_MinLod;
			info.maxLod = m_MaxLod;
			info.borderColor = BorderColorForAddressMode(m_AddressMode);
			info.compareOp = CompareOpToVulkan(m_CompareOp);
			info.compareEnable = m_CompareOp != CompareOperation::Never;
			FilterModeToVulkan(m_FilterMode, &info.minFilter, &info.magFilter, &info.mipmapMode);

			VK_CHECK(vkCreateSampler(m_Device, &info, nullptr, &m_Sampler));
		}

		virtual ~VulkanSampler()
		{
			if (m_Sampler)
			{
				Renderer::SubmitResourceFree([device = m_Device, sampler = m_Sampler]()
				{
					vkDestroySampler(device, sampler, nullptr);
				});
			}
		}

		void* GetHandle() const override { return m_Sampler; }

	private:
		VkDevice m_Device = VK_NULL_HANDLE;
		VkSampler m_Sampler = VK_NULL_HANDLE;
	};
}
