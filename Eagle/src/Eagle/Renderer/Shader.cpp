#include "egpch.h"
#include "Shader.h"

#include "Renderer.h"
#include "Platform/OpenGL/OpenGLShader.h"

namespace Eagle
{
	Ref<Shader> Shader::Create(const std::string& vertexSource, const std::string& fragmentSource)
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::API::None:
			EG_CORE_ASSERT(false, "RendererAPI::None currently is not supported!");
			return nullptr;

		case RendererAPI::API::OpenGL:
			return MakeRef<OpenGLShader>(vertexSource, fragmentSource);
		}
		EG_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}
}