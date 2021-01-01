#include "egpch.h"
#include "VertexArray.h"

#include "Renderer.h"
#include "Platform/OpenGL/OpenGLVertexArray.h"

namespace Eagle
{
	Ref<VertexArray> VertexArray::Create()
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::API::None:
			EG_CORE_ASSERT(false, "RendererAPI::None currently is not supported!");
			return nullptr;

		case RendererAPI::API::OpenGL:
			return MakeRef<OpenGLVertexArray>();
		}
		EG_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}
}