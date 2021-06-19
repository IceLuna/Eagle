#include "egpch.h"
#include "Cubemap.h"
#include "Renderer.h"

#include "Platform/OpenGL/OpenGLCubemap.h"

namespace Eagle
{
	Ref<Cubemap> Cubemap::Create(const std::array<Ref<Texture>, 6> textures)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:
				EG_CORE_ASSERT(false, "RendererAPI::None currently is not supported!");
				return nullptr;

			case RendererAPI::API::OpenGL:
				return MakeRef<OpenGLCubemap>(textures);
		}
		EG_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}
}
