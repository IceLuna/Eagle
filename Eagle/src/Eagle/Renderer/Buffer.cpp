#include "egpch.h"
#include "Buffer.h"
#include "RendererContext.h"
#include "Platform/Vulkan/VulkanBuffer.h"

namespace Eagle
{
	Ref<Buffer> Buffer::Create(const BufferSpecifications& specs, const std::string& debugName)
	{
		switch (RendererContext::Current())
		{
			case RendererAPIType::Vulkan: return MakeRef<VulkanBuffer>(specs, debugName);
		}

		EG_CORE_ASSERT(false, "Unknown renderer API");
		return nullptr;
	}
}
