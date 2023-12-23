#include "egpch.h"
#include "Fence.h"

#include "Platform/Vulkan/VulkanFence.h"

namespace Eagle
{
	Ref<Fence> Fence::Create(bool bSignaled)
	{
		switch (RendererContext::Current())
		{
			case RendererAPIType::Vulkan: return MakeRef<VulkanFence>(bSignaled);
		}

		EG_CORE_ASSERT(false, "Unknown renderer API");
		return nullptr;
	}
}
