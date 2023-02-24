#include "egpch.h"
#include "VulkanGPUTimings.h"

#ifdef EG_GPU_TIMINGS

#include "Eagle/Renderer/Renderer.h"
#include "VulkanContext.h"

namespace Eagle
{
	VulkanGPUTiming::VulkanGPUTiming()
	{
		m_Device = VulkanContext::GetDevice()->GetVulkanDevice();

		VkQueryPoolCreateInfo ci{};
		ci.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
		ci.queryType = VK_QUERY_TYPE_TIMESTAMP;
		ci.queryCount = 2 * RendererConfig::FramesInFlight;

		VK_CHECK(vkCreateQueryPool(m_Device, &ci, nullptr, &m_Pool));
	}

	VulkanGPUTiming::~VulkanGPUTiming()
	{
		Renderer::SubmitResourceFree([device = m_Device, pool = m_Pool]()
		{
			vkDestroyQueryPool(device, pool, nullptr);
		});
		m_Pool = VK_NULL_HANDLE;
	}
	
	void VulkanGPUTiming::QueryTiming(uint32_t frameInFlight)
	{
		m_Timing = 0.f;
		std::uint64_t timings[2] = {};

		VkResult res = vkGetQueryPoolResults(m_Device, m_Pool, 2 * frameInFlight, 2, sizeof(timings), timings, sizeof(*timings), VK_QUERY_RESULT_64_BIT);
		if (res != VK_NOT_READY)
		{
			m_Timing = float(timings[1] - timings[0]) * VulkanContext::GetDevice()->GetPhysicalDevice()->GetProperties().limits.timestampPeriod * 1e-6f;
		}
	}
}

#endif
