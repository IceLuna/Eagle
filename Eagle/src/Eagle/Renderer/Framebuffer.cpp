#include "egpch.h"
#include "Framebuffer.h"

#include "Eagle/Renderer/Renderer.h"

#include "Platform/OpenGL/OpenGLFramebuffer.h"

namespace Eagle
{
	Ref<Framebuffer> Framebuffer::Create(const FramebufferSpecification& spec)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:
				EG_CORE_ASSERT(false, "RendererAPI::None currently is not supported!");
				return nullptr;

			case RendererAPI::API::OpenGL:
				return MakeRef<OpenGLFramebuffer>(spec);
		}
		EG_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}
}