#include "egpch.h"

#include "WindowsWindow.h"
#include <Windows.h>

#include "Eagle/Events/ApplicationEvent.h"
#include "Eagle/Events/KeyEvent.h"
#include "Eagle/Events/MouseEvent.h"

#include "Platform/OpenGL/OpenGLContext.h"
#include "Eagle/Renderer/Renderer.h"

#include "GLFW/glfw3.h"

namespace Eagle
{
	static bool s_GLFWInitialized = false;
	float Window::s_HighDPIScaleFactor = 1.0f;

	static void GLFWErrorCallback(int error, const char* description)
	{
		EG_CORE_ERROR("GLFW Error ({0}): {1}", error, description);
	}

	WindowsWindow::WindowsWindow(const WindowProps& props)
	{
		Init(props);
	}

	WindowsWindow::~WindowsWindow()
	{
		Shutdown();
	}

	void WindowsWindow::Init(const WindowProps& props)
	{
		m_Data.Title = props.Title;
		m_Data.Width = props.Width;
		m_Data.Height = props.Height;

		EG_CORE_INFO("Creating window {0}", props.Title);

		#ifdef EG_DIST
			::ShowWindow(::GetConsoleWindow(), SW_HIDE);
		#else
			::ShowWindow(::GetConsoleWindow(), SW_RESTORE);
		#endif


		if (!s_GLFWInitialized)
		{
			int success = glfwInit();
			EG_CORE_ASSERT(success, "Could not initialize GLFW!");
			glfwSetErrorCallback(GLFWErrorCallback);
			s_GLFWInitialized = true;
		}

		#if defined(EG_DEBUG)
			if (Renderer::GetAPI() == RendererAPI::API::OpenGL)
				glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
		#endif

			GLFWmonitor* monitor = glfwGetPrimaryMonitor();
			float xscale, yscale;
			glfwGetMonitorContentScale(monitor, &xscale, &yscale);

			if (xscale > 1.0f || yscale > 1.0f)
			{
				s_HighDPIScaleFactor = yscale;
				glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
			}
			glfwWindowHint(GLFW_SAMPLES, 4);
			if (props.Fullscreen)
			{
				m_Window = glfwCreateWindow(props.Width, props.Height, props.Title.c_str(), glfwGetPrimaryMonitor(), nullptr); //Fullscreen
			}
			else
			{
				m_Window = glfwCreateWindow(props.Width, props.Height, props.Title.c_str(), nullptr, nullptr);
			}

		m_Context = MakeRef<OpenGLContext>(m_Window);
		m_Context->Init();

		glfwSetWindowUserPointer(m_Window, &m_Data);
		SetVSync(true);

		// Set GLFW callbacks
		SetupGLFWCallbacks();
	}

	void WindowsWindow::SetFocus(bool focus)
	{
		if (focus)
		{
			glfwFocusWindow(m_Window);
		}
	}

	void WindowsWindow::SetWindowSize(int width, int height)
	{
		glfwSetWindowSize(m_Window, width, height);
	}

	void WindowsWindow::SetWindowPos(int x, int y)
	{
		glfwSetWindowPos(m_Window, x, y);
	}

	glm::vec2 WindowsWindow::GetWindowSize() const
	{
		int w = 0, h = 0; 
		glfwGetWindowSize(m_Window, &w, &h); 
		return { w, h };
	}

	glm::vec2 WindowsWindow::GetWindowPos() const
	{
		int x = 0, y = 0; 
		glfwGetWindowPos(m_Window, &x, &y); 
		return { x, y };
	}

	void WindowsWindow::Shutdown()
	{
		glfwDestroyWindow(m_Window);
		m_Window = nullptr;
	}

	void WindowsWindow::OnUpdate()
	{
		glfwPollEvents();
		m_Context->SwapBuffers();
	}

	void WindowsWindow::SetVSync(bool enable)
	{
		m_Data.VSync = enable;
		glfwSwapInterval((int)m_Data.VSync);
	}

	void WindowsWindow::SetupGLFWCallbacks() const
	{
		glfwSetWindowSizeCallback(m_Window, [](GLFWwindow* window, int width, int height)
		{
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);

			data.Width = width;
			data.Height = height;

			WindowResizeEvent event(width, height);
			data.EventCallback(event);
		});

		glfwSetWindowCloseCallback(m_Window, [](GLFWwindow* window)
		{
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);

			WindowCloseEvent event;
			data.EventCallback(event);
		});

		glfwSetKeyCallback(m_Window, [](GLFWwindow* window, int key, int scancode, int action, int mods)
		{
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);

			switch (action)
			{
			case GLFW_PRESS:
			{
				KeyPressedEvent event(key, 0);
				data.EventCallback(event);
				break;
			}
			case GLFW_RELEASE:
			{
				KeyReleasedEvent event(key);
				data.EventCallback(event);
				break;
			}
			case GLFW_REPEAT:
			{
				KeyPressedEvent event(key, 1);
				data.EventCallback(event);
				break;
			}
			}
		});

		glfwSetCharCallback(m_Window, [](GLFWwindow* window, unsigned int keyCode)
		{
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
			KeyTypedEvent event(keyCode);
			data.EventCallback(event);
		});

		glfwSetMouseButtonCallback(m_Window, [](GLFWwindow* window, int button, int action, int mods)
		{
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);

			switch (action)
			{
				case GLFW_PRESS:
				{
					MouseButtonPressedEvent event(button);
					data.EventCallback(event);
					break;
				}
				case GLFW_RELEASE:
				{
					MouseButtonReleasedEvent event(button);
					data.EventCallback(event);
					break;
				}
			}
		});

		glfwSetScrollCallback(m_Window, [](GLFWwindow* window, double xOffset, double yOffset)
		{
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);

			MouseScrolledEvent event((float)xOffset, (float)yOffset);
			data.EventCallback(event);
		});

		glfwSetCursorPosCallback(m_Window, [](GLFWwindow* window, double xPos, double yPos)
		{
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);

			MouseMovedEvent event((float)xPos, (float)yPos);
			data.EventCallback(event);
		});
	}
}
