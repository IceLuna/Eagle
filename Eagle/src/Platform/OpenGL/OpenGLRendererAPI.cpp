#include "egpch.h"
#include "OpenGLRendererAPI.h"

#include <glad/glad.h>

namespace Eagle
{

	static void OpenGLMessageCallback(unsigned source, unsigned type, unsigned id, unsigned severity, int length, const char* message, const void* userParam)
	{
		switch (severity)
		{
			case GL_DEBUG_SEVERITY_HIGH:         EG_CORE_CRITICAL(message); return;
			case GL_DEBUG_SEVERITY_MEDIUM:       EG_CORE_ERROR(message); return;
			case GL_DEBUG_SEVERITY_LOW:          EG_CORE_WARN(message); return;
			case GL_DEBUG_SEVERITY_NOTIFICATION: EG_CORE_TRACE(message); return;
		}

		EG_CORE_ASSERT(false, "Unknown severity level!");
	}
	
	void OpenGLRendererAPI::SetClearColor(const glm::vec4& color)
	{
		glClearColor(color.r, color.g, color.b, color.a);
	}

	void OpenGLRendererAPI::Clear()
	{
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}

	void OpenGLRendererAPI::ClearDepthBuffer()
	{
		glClear(GL_DEPTH_BUFFER_BIT);
	}

	void OpenGLRendererAPI::Init()
	{
		//glEnable(GL_BLEND);
		//glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_CULL_FACE);
		glEnable(GL_MULTISAMPLE);
		glEnable(GL_DEPTH_TEST);
		glLineWidth(2.f);

		#ifdef EG_DEBUG
			glEnable(GL_DEBUG_OUTPUT);
			glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
			glDebugMessageCallback(OpenGLMessageCallback, nullptr);

			glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, NULL, GL_FALSE);
		#endif
	}

	void OpenGLRendererAPI::SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
	{
		glViewport(x, y, width, height);
	}

	void OpenGLRendererAPI::SetDepthMask(bool depthMask)
	{
		glDepthMask(depthMask);
	}

	void OpenGLRendererAPI::SetDepthFunc(DepthFunc func)
	{
		switch (func)
		{
			case DepthFunc::LESS:
				glDepthFunc(GL_LESS);
				break;

			case DepthFunc::LEQUAL:
				glDepthFunc(GL_LEQUAL);
				break;
		}
	}

	void OpenGLRendererAPI::DrawIndexed(const Ref<VertexArray>& vertexArray)
	{
		vertexArray->Bind();
		glDrawElements(GL_TRIANGLES, vertexArray->GetIndexBuffer()->GetCount(), GL_UNSIGNED_INT, nullptr);
	}

	//Draw COUNT elements from Bound VA.
	void OpenGLRendererAPI::DrawIndexed(uint32_t count)
	{
		glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, nullptr);
	}

	void OpenGLRendererAPI::DrawLines(const Ref<VertexArray>& vertexArray, uint32_t vertexCount)
	{
		vertexArray->Bind();
		glDrawArrays(GL_LINES, 0, vertexCount);
	}
}