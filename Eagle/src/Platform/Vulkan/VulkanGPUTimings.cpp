#include "egpch.h"
#include "VulkanGPUTimings.h"

#ifdef EG_GPU_TIMINGS

#include "Eagle/Renderer/RenderManager.h"
#include "VulkanContext.h"

namespace Eagle
{
	VulkanGPUTiming::VulkanGPUTiming()
	{
		m_Device = VulkanContext::GetDevice()->GetVulkanDevice();
		CreatePool();
	}

	VulkanGPUTiming::~VulkanGPUTiming()
	{
		DestroyPool();
	}

	void VulkanGPUTiming::CreatePool()
	{
		VkQueryPoolCreateInfo ci{};
		ci.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
		ci.queryType = VK_QUERY_TYPE_TIMESTAMP;
		ci.queryCount = 2 * m_QueriesPerFrame * RendererConfig::FramesInFlight;

		VK_CHECK(vkCreateQueryPool(m_Device, &ci, nullptr, &m_Pool));
		vkResetQueryPool(m_Device, m_Pool, 0, ci.queryCount);
	}

	void VulkanGPUTiming::DestroyPool()
	{
		RenderManager::SubmitResourceFree([device = m_Device, pool = m_Pool]()
		{
			vkDestroyQueryPool(device, pool, nullptr);
		});
		m_Pool = VK_NULL_HANDLE;
	}

	void VulkanGPUTiming::RecreatePool()
	{
		DestroyPool();
		CreatePool();
	}

	bool VulkanGPUTiming::ReadTimings(uint32_t frameIndex, uint32_t count, float& outTiming)
	{
		m_Results.resize(2 * size_t(count));

		const uint32_t firstQuery = 2 * frameIndex * m_QueriesPerFrame;
		VkResult res = vkGetQueryPoolResults(m_Device, m_Pool, firstQuery, 2 * count, m_Results.size() * sizeof(uint64_t), m_Results.data(), sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);
		if (res != VK_SUCCESS)
			return false;

		uint64_t ticks = 0;
		for (uint32_t i = 0; i < count; ++i)
			ticks += m_Results[2 * i + 1] - m_Results[2 * i];

		outTiming = float(ticks) * VulkanContext::GetDevice()->GetPhysicalDevice()->GetProperties().limits.timestampPeriod * 1e-6f;
		return true;
	}
}

#endif
