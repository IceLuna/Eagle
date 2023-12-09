#include "egpch.h"

#include "WindowsWindow.h"
#include <Windows.h>

#include "Eagle/Events/ApplicationEvent.h"
#include "Eagle/Events/KeyEvent.h"
#include "Eagle/Events/MouseEvent.h"

#include "Eagle/Renderer/RenderManager.h"
#include "Eagle/Renderer/RendererContext.h"
#include "Platform/Vulkan/VulkanContext.h"

#include "GLFW/glfw3.h"
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <stb_image.h>

namespace Eagle
{
	static bool s_GLFWInitialized = false;
	float Window::s_HighDPIScaleFactor = 1.0f;

	static void GLFWErrorCallback(int error, const char* description)
	{
		EG_CORE_ERROR("GLFW Error ({0}): {1}", error, description);
	}

	WindowsWindow::WindowsWindow(const WindowProps& props) : Window(props)
	{
		EG_CORE_INFO("Creating window {0}", m_Props.Title);
		m_WindowData.Props = &m_Props;

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

		if (RenderManager::GetAPI() == RendererAPIType::Vulkan)
			glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

		GLFWmonitor* monitor = glfwGetPrimaryMonitor();
		float xscale, yscale;
		glfwGetMonitorContentScale(monitor, &xscale, &yscale);

		if (xscale > 1.0f || yscale > 1.0f)
		{
			s_HighDPIScaleFactor = yscale;
			glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
		}

		if (m_Props.Fullscreen)
			m_Window = glfwCreateWindow(m_Props.Width, m_Props.Height, m_Props.Title.c_str(), glfwGetPrimaryMonitor(), nullptr);
		else
			m_Window = glfwCreateWindow(m_Props.Width, m_Props.Height, m_Props.Title.c_str(), nullptr, nullptr);

		auto& rendererContext = Application::Get().GetRenderContext();
		auto vulkanContext = Cast<VulkanContext>(rendererContext);
		m_Swapchain = MakeRef<VulkanSwapchain>(VulkanContext::GetInstance(), m_Window);
		vulkanContext->InitDevices(m_Swapchain->GetSurface(), true);
		m_Swapchain->Init(VulkanContext::GetDevice(), m_Props.VSync);

		SetWindowIcon("assets/textures/Editor/icon.png");

		glfwSetWindowUserPointer(m_Window, &m_WindowData);

		// Set GLFW callbacks
		SetupGLFWCallbacks();
	}

	WindowsWindow::~WindowsWindow()
	{
		Shutdown();
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

	void WindowsWindow::SetWindowMaximized(bool bMaximize)
	{
		auto func = bMaximize ? glfwMaximizeWindow : glfwRestoreWindow;
		func(m_Window);
	}

	void WindowsWindow::SetWindowPos(int x, int y)
	{
		glfwSetWindowPos(m_Window, x, y);
	}

	void WindowsWindow::SetWindowTitle(const std::string& title)
	{
		m_Props.Title = title;
		glfwSetWindowTitle(m_Window, title.c_str());
	}

	void WindowsWindow::SetWindowIcon(const Path& iconPath)
	{
		int width, height, channels;
		stbi_set_flip_vertically_on_load(0);
		std::wstring pathString = iconPath.wstring();

		char cpath[2048];
		WideCharToMultiByte(65001 /* UTF8 */, 0, pathString.c_str(), -1, cpath, 2048, NULL, NULL);
		stbi_uc* data = stbi_load(cpath, &width, &height, &channels, 0);

		if (data)
		{
			GLFWimage images[1];
			images[0].pixels = data;
			images[0].width = width;
			images[0].height = height;
			glfwSetWindowIcon(m_Window, 1, images);
		}

		stbi_image_free(data);
	}

	void WindowsWindow::SetFullscreen(bool bFullscreen)
	{
		if (m_Props.Fullscreen == bFullscreen)
			return;

		m_Props.Fullscreen = bFullscreen;
		GLFWmonitor* monitor = bFullscreen ? glfwGetPrimaryMonitor() : nullptr;
		const glm::ivec2 windowSize = GetWindowSize();
		const glm::ivec2 windowPos = GetWindowPos();
		glfwSetWindowMonitor(m_Window, monitor, windowPos.x, windowPos.y, windowSize.x, windowSize.y, GLFW_DONT_CARE);
	}

	glm::vec2 WindowsWindow::GetWindowSize() const
	{
		int w = 0, h = 0; 
		glfwGetWindowSize(m_Window, &w, &h); 
		return { w, h };
	}

	bool WindowsWindow::IsMaximized() const
	{
		return glfwGetWindowAttrib(m_Window, GLFW_MAXIMIZED);
	}

	glm::vec2 WindowsWindow::GetWindowPos() const
	{
		int x = 0, y = 0; 
		glfwGetWindowPos(m_Window, &x, &y); 
		return { x, y };
	}

	void WindowsWindow::Shutdown()
	{
		m_Swapchain.reset();

		glfwDestroyWindow(m_Window);
		glfwTerminate();

		m_Window = nullptr;
		s_GLFWInitialized = false;
	}

	void WindowsWindow::ProcessEvents()
	{
		glfwPollEvents();
	}

	inline void* WindowsWindow::GetNativeWindow() const
	{
		return glfwGetWin32Window(m_Window);
	}

	void WindowsWindow::SetVSync(bool enable)
	{
		m_Props.VSync = enable;
		m_Swapchain->SetVSyncEnabled(enable);
	}

	void WindowsWindow::SetupGLFWCallbacks() const
	{
		glfwSetWindowSizeCallback(m_Window, [](GLFWwindow* window, int width, int height)
		{
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);

			data.Props->Width = width;
			data.Props->Height = height;

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
				KeyPressedEvent event((Key)key, 0);
				data.EventCallback(event);
				break;
			}
			case GLFW_RELEASE:
			{
				KeyReleasedEvent event((Key)key);
				data.EventCallback(event);
				break;
			}
			case GLFW_REPEAT:
			{
				KeyPressedEvent event((Key)key, 1);
				data.EventCallback(event);
				break;
			}
			}
		});

		glfwSetCharCallback(m_Window, [](GLFWwindow* window, unsigned int keyCode)
		{
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
			KeyTypedEvent event((Key)keyCode);
			data.EventCallback(event);
		});

		glfwSetMouseButtonCallback(m_Window, [](GLFWwindow* window, int button, int action, int mods)
		{
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);

			switch (action)
			{
				case GLFW_PRESS:
				{
					MouseButtonPressedEvent event((Mouse)button);
					data.EventCallback(event);
					break;
				}
				case GLFW_RELEASE:
				{
					MouseButtonReleasedEvent event((Mouse)button);
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
