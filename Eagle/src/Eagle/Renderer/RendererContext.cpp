#include "egpch.h"
#include "RendererContext.h"
#include "Platform/Vulkan/VulkanContext.h"

namespace Eagle
{
	RendererAPIType RendererContext::s_API = RendererAPIType::None;

	Ref<RendererContext> RendererContext::Create()
	{
		switch (RendererContext::Current())
		{
			case RendererAPIType::Vulkan: return MakeRef<VulkanContext>();
		}

		EG_CORE_ASSERT(false, "Unknown Renderer API");

		return nullptr;
	}
}
