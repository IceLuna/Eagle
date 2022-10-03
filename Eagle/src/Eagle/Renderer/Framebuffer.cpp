#include "egpch.h"
#include "Framebuffer.h"

#include "Eagle/Renderer/Renderer.h"
#include "Platform/Vulkan/VulkanFramebuffer.h"

namespace Eagle
{
	Ref<Framebuffer> Framebuffer::Create(const std::vector<Ref<Image>>& images, glm::uvec2 size, const void* renderPassHandle)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPIType::Vulkan:
				return MakeRef<VulkanFramebuffer>(images, size, renderPassHandle);
		}
		EG_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}
}
