#include "egpch.h"
#include "RenderCommand.h"

#include "Platform/OpenGL/OpenGLRendererAPI.h"

namespace Eagle
{
	Ref<RendererAPI> RenderCommand::s_RendererAPI = MakeRef<OpenGLRendererAPI>();
}
