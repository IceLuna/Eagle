#pragma once

#include "Vulkan.h"

namespace Eagle
{
	class VulkanPipelineCache
	{
	public:
		static void Init();
		static void Shutdown();

		static VkPipelineCache GetCache();
	};
}
