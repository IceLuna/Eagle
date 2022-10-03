#include "egpch.h"
#include "Semaphore.h"

#include "RendererAPI.h"
#include "Platform/Vulkan/VulkanSemaphore.h"

namespace Eagle
{
	Ref<Semaphore> Semaphore::Create()
	{
		switch (RendererAPI::Current())
		{
		case RendererAPIType::Vulkan: return MakeRef<VulkanSemaphore>();
		}

		EG_CORE_ASSERT(false, "Unknown renderer API");
		return nullptr;
	}
}
