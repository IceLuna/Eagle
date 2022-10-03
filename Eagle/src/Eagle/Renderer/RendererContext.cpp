#include "egpch.h"
#include "RendererContext.h"
#include "RendererAPI.h"
#include "Platform/Vulkan/VulkanContext.h"

namespace Eagle
{
	Ref<RendererContext> RendererContext::Create()
	{
		switch (RendererAPI::Current())
		{
			case RendererAPIType::Vulkan: return MakeRef<VulkanContext>();
		}

		EG_CORE_ASSERT(false, "Unknown Renderer API");

		return nullptr;
	}
}
