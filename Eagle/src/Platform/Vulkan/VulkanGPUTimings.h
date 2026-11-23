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

	protected:
		virtual void RecreatePool() override;
		virtual bool ReadTimings(uint32_t frameIndex, uint32_t count, float& outTiming) override;

		void CreatePool();
		void DestroyPool();

	protected:
		VkDevice m_Device;
		VkQueryPool m_Pool = VK_NULL_HANDLE;
		std::vector<uint64_t> m_Results;
	};
}

#endif
