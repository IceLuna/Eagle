#pragma once

#ifdef EG_GPU_TIMINGS

#include "Vulkan.h"
#include "Eagle/Debug/GPUTimings.h"

namespace Eagle
{
	class VulkanGPUTiming : public RHIGPUTiming
	{
	public:
		VulkanGPUTiming();
		virtual ~VulkanGPUTiming();

		virtual void* GetQueryPoolHandle() override { return m_Pool; }
		virtual void QueryTiming(uint32_t frameInFlight) override;

	protected:
		VkDevice m_Device;
		VkQueryPool m_Pool = VK_NULL_HANDLE;
	};
}

#endif
