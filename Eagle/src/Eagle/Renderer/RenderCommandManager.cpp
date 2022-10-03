#include "egpch.h"
#include "RenderCommandManager.h"

#include "Renderer.h"
#include "Platform/Vulkan/VulkanCommandManager.h"

namespace Eagle
{
	Ref<CommandManager> CommandManager::Create(CommandQueueFamily queueFamily, bool bAllowReuse)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPIType::Vulkan:  return MakeRef<VulkanCommandManager>(queueFamily, bAllowReuse);
		}
		EG_CORE_ASSERT(false, "Unknown RendererAPI");
		return nullptr;
	}
}
