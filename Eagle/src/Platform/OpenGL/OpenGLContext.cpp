#include "egpch.h"

#include "OpenGLContext.h"
#include <GLFW/glfw3.h>
#include <glad/glad.h>

namespace Eagle
{
	OpenGLContext::OpenGLContext(GLFWwindow* windowHandle)
		: m_WindowHandle(windowHandle)
	{
		EG_CORE_ASSERT(m_WindowHandle, "OpenGL WindowHandle is null");
	}

	void OpenGLContext::Init()
	{
		glfwMakeContextCurrent(m_WindowHandle);

		int status = gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
		EG_CORE_ASSERT(status, "Failed to initialize Glad!");

		EG_CORE_INFO("OpenGL Info:");
		EG_CORE_INFO("\tVendor: {0}", glGetString(GL_VENDOR));
		EG_CORE_INFO("\tRenderer: {0}", glGetString(GL_RENDERER));
		EG_CORE_INFO("\tVersion: {0}", glGetString(GL_VERSION));
	}

	void OpenGLContext::SwapBuffers()
	{
		glfwSwapBuffers(m_WindowHandle);
	}
}