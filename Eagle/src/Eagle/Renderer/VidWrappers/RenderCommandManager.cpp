#include "egpch.h"
#include "RenderCommandManager.h"

#include "Eagle/Renderer/RenderManager.h"
#include "Platform/Vulkan/VulkanCommandManager.h"

namespace Eagle
{
	Ref<CommandManager> CommandManager::Create(CommandQueueFamily queueFamily, bool bAllowReuse)
	{
		switch (RenderManager::GetAPI())
		{
			case RendererAPIType::Vulkan:  return MakeRef<VulkanCommandManager>(queueFamily, bAllowReuse);
		}
		EG_CORE_ASSERT(false, "Unknown RendererAPI");
		return nullptr;
	}
}
