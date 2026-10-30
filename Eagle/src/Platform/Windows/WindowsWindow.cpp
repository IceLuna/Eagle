#include "egpch.h"

#include "WindowsWindow.h"
#include <Windows.h>
#include <dwmapi.h>

#include "Eagle/Events/ApplicationEvent.h"
#include "Eagle/Events/KeyEvent.h"
#include "Eagle/Events/MouseEvent.h"

#include "Eagle/Renderer/RenderManager.h"
#include "Eagle/Renderer/RendererContext.h"
#include "Platform/Vulkan/VulkanContext.h"
#include "Utils/WindowsImage.h"

#include "GLFW/glfw3.h"
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

namespace Eagle
{
	static bool s_GLFWInitialized = false;

	// See `WindowsWindow::CreateModalLoopHandler()`
	static constexpr const wchar_t* s_ModalLoopWindowClass = L"EagleModalLoop";
	static constexpr UINT_PTR s_ModalLoopTimerID = 1;
	static constexpr UINT s_ModalLoopFrameMessage = WM_APP + 1;
	static WindowsWindow* s_ModalLoopWindow = nullptr; // Hook procedures don't get any user data

	static void GLFWErrorCallback(int error, const char* description)
	{
		EG_CORE_ERROR("GLFW Error ({0}): {1}", error, description);
	}

	// How much bigger the window is than its client area (the OS frame), for a window that's not maximized
	static RECT GetFrameInsets(HWND hwnd)
	{
		RECT insets{ 0, 0, 0, 0 };
		if (CustomWindowFrame::IsEnabled(hwnd))
			return insets; // The client area takes the whole window

		const DWORD style = DWORD(GetWindowLongPtrW(hwnd, GWL_STYLE)) & ~DWORD(WS_MAXIMIZE | WS_MINIMIZE);
		const DWORD exStyle = DWORD(GetWindowLongPtrW(hwnd, GWL_EXSTYLE));
		RECT rect{ 0, 0, 0, 0 };
		if (AdjustWindowRectExForDpi(&rect, style, FALSE, exStyle, GetDpiForWindow(hwnd)))
			insets = RECT{ -rect.left, -rect.top, rect.right, rect.bottom };
		return insets;
	}

	// `WINDOWPLACEMENT` uses workspace coordinates. They're relative to the monitor's work area instead of the monitor itself
	static POINT GetWorkspaceOffset(HMONITOR monitor)
	{
		MONITORINFO info{};
		info.cbSize = sizeof(info);
		if (!monitor || !GetMonitorInfoW(monitor, &info))
			return POINT{ 0, 0 };
		return POINT{ info.rcWork.left - info.rcMonitor.left, info.rcWork.top - info.rcMonitor.top };
	}

	static void ApplyWindowPlacement(HWND hwnd, const WindowPlacement& placement, UINT showCommand)
	{
		const RECT insets = GetFrameInsets(hwnd);
		RECT rect;
		rect.left = placement.Pos.x - insets.left;
		rect.top = placement.Pos.y - insets.top;
		rect.right = placement.Pos.x + placement.Size.x + insets.right;
		rect.bottom = placement.Pos.y + placement.Size.y + insets.bottom;

		const POINT offset = GetWorkspaceOffset(MonitorFromRect(&rect, MONITOR_DEFAULTTONEAREST));
		OffsetRect(&rect, -offset.x, -offset.y);

		// If the rect is off-screen (for example, its monitor was disconnected), the OS moves it onto a monitor
		WINDOWPLACEMENT windowPlacement{};
		windowPlacement.length = sizeof(windowPlacement);
		windowPlacement.showCmd = showCommand;
		windowPlacement.ptMinPosition = POINT{ -1, -1 };
		windowPlacement.ptMaxPosition = POINT{ -1, -1 };
		windowPlacement.rcNormalPosition = rect;
		SetWindowPlacement(hwnd, &windowPlacement);
	}

	WindowsWindow::WindowsWindow(const WindowProperties& props) : Window(props)
	{
		EG_CORE_INFO("Creating window {0}", m_Props.Title);
		m_WindowData.Props = &m_Props;
		m_WindowData.DPIScale = &m_DPIScale;

		SetConsoleOutputCP(CP_UTF8);

		if (!s_GLFWInitialized)
		{
			SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
			int success = glfwInit();
			EG_CORE_ASSERT(success, "Could not initialize GLFW!");
#ifndef EG_RELEASE
			glfwSetErrorCallback(GLFWErrorCallback);
#endif
			s_GLFWInitialized = true;
		}

		if (RenderManager::GetAPI() == RendererAPIType::Vulkan)
			glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

		GLFWmonitor* monitor = glfwGetPrimaryMonitor();
		float xscale, yscale;
		glfwGetMonitorContentScale(monitor, &xscale, &yscale);

		if (xscale > 1.0f || yscale > 1.0f)
		{
			m_DPIScale = glm::max(xscale, yscale);
			glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
		}

		// The window is created hidden. It's shown once the app is ready, so that the user never sees it empty.
		glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
		if (m_Props.bCustomTitleBar)
			glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);

		// Note: for now, a fullscreen window is created windowed, it becomes fullscreen when it's shown.
		m_Window = glfwCreateWindow(m_Props.Width, m_Props.Height, m_Props.Title.c_str(), nullptr, nullptr);

		// Restore the defaults for windows created later
		glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
		glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);

		if (m_Props.bCustomTitleBar)
		{
			WindowData* windowData = &m_WindowData;
			CustomWindowFrame::Install(GetNativeWindow(), [windowData](const glm::ivec2& screenPos)
			{
				return windowData->TitleBarHitTest ? windowData->TitleBarHitTest(screenPos) : WindowHitTest::Client;
			});

			// The OS doesn't pick a position for borderless windows, so center it in the work area of the monitor
			int areaX = 0, areaY = 0, areaWidth = 0, areaHeight = 0;
			glfwGetMonitorWorkarea(monitor, &areaX, &areaY, &areaWidth, &areaHeight);
			if (areaWidth > 0 && areaHeight > 0)
			{
				int width = 0, height = 0;
				glfwGetWindowSize(m_Window, &width, &height);
				width = std::min(width, areaWidth);
				height = std::min(height, areaHeight);
				glfwSetWindowSize(m_Window, width, height);
				glfwSetWindowPos(m_Window, areaX + (areaWidth - width) / 2, areaY + (areaHeight - height) / 2);
				m_Props.Width = uint32_t(width);
				m_Props.Height = uint32_t(height);
			}
		}

		auto& rendererContext = Application::Get().GetRenderContext();
		auto vulkanContext = Cast<VulkanContext>(rendererContext);
		m_Swapchain = MakeRef<VulkanSwapchain>(VulkanContext::GetInstance(), m_Window);
		vulkanContext->InitDevices(m_Swapchain->GetSurface(), true);
		m_Swapchain->Init(VulkanContext::GetDevice(), m_Props.VSync);

		if (!m_Props.IconPath.empty())
			SetWindowIcon(m_Props.IconPath);

		glfwSetWindowUserPointer(m_Window, &m_WindowData);

		// Set GLFW callbacks
		SetupGLFWCallbacks();

		CreateModalLoopHandler();
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
		if (!m_bShown)
		{
			GetPendingPlacement().Size = glm::ivec2(width, height);
			return;
		}
		glfwSetWindowSize(m_Window, width, height);
	}

	void WindowsWindow::SetWindowMaximized(bool bMaximize)
	{
		if (!m_bShown)
		{
			GetPendingPlacement().bMaximized = bMaximize;
			return;
		}
		auto func = bMaximize ? glfwMaximizeWindow : glfwRestoreWindow;
		func(m_Window);
	}

	void WindowsWindow::SetWindowPos(int x, int y)
	{
		if (!m_bShown)
		{
			GetPendingPlacement().Pos = glm::ivec2(x, y);
			return;
		}
		glfwSetWindowPos(m_Window, x, y);
	}

	WindowPlacement& WindowsWindow::GetPendingPlacement()
	{
		if (!m_PendingPlacement)
			m_PendingPlacement = GetWindowPlacement();
		return *m_PendingPlacement;
	}

	WindowPlacement WindowsWindow::GetWindowPlacement() const
	{
		if (m_PendingPlacement)
			return *m_PendingPlacement;

		WindowPlacement placement;
		HWND hwnd = (HWND)GetNativeWindow();
		WINDOWPLACEMENT windowPlacement{};
		windowPlacement.length = sizeof(windowPlacement);
		if (!::GetWindowPlacement(hwnd, &windowPlacement))
		{
			placement.Pos = glm::ivec2(GetWindowPos());
			placement.Size = glm::ivec2(GetWindowSize());
			placement.bMaximized = IsMaximized();
			return placement;
		}

		RECT rect = windowPlacement.rcNormalPosition;
		const POINT offset = GetWorkspaceOffset(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST));
		OffsetRect(&rect, offset.x, offset.y);

		const RECT insets = GetFrameInsets(hwnd);
		placement.Pos = glm::ivec2(rect.left + insets.left, rect.top + insets.top);
		placement.Size = glm::ivec2(std::max(0L, (rect.right - rect.left) - insets.left - insets.right), std::max(0L, (rect.bottom - rect.top) - insets.top - insets.bottom));
		placement.bMaximized = IsIconic(hwnd) ? (windowPlacement.flags & WPF_RESTORETOMAXIMIZED) != 0 : IsZoomed(hwnd) != FALSE;
		return placement;
	}

	void WindowsWindow::SetWindowPlacement(const WindowPlacement& placement)
	{
		WindowPlacement newPlacement = placement;
		if (newPlacement.Size.x <= 0 || newPlacement.Size.y <= 0)
			newPlacement.Size = GetWindowPlacement().Size;

		// A hidden window applies it when it's shown, a fullscreen one when it leaves fullscreen
		if (!m_bShown || m_Props.Fullscreen)
		{
			m_PendingPlacement = newPlacement;
			return;
		}

		ApplyWindowPlacement((HWND)GetNativeWindow(), newPlacement, newPlacement.bMaximized ? SW_SHOWMAXIMIZED : SW_SHOWNORMAL);
	}

	void WindowsWindow::Show(bool bCloaked)
	{
		if (m_bShown)
			return;

		SetCloaked(bCloaked);
		m_bShown = true;

		HWND hwnd = (HWND)GetNativeWindow();
		const bool bIsFullscreen = glfwGetWindowMonitor(m_Window) != nullptr; // It might have been fullscreen before it was hidden
		if (m_Props.Fullscreen)
		{
			if (bIsFullscreen)
			{
				// The pending placement is kept, it's applied when the window leaves fullscreen
				glfwShowWindow(m_Window);
			}
			else
			{
				// The windowed placement is applied first, the window returns to it when it leaves fullscreen
				if (m_PendingPlacement)
					ApplyWindowPlacement(hwnd, *m_PendingPlacement, SW_HIDE);
				m_PendingPlacement.reset();
				ApplyFullscreen(true); // Shows the window
			}
		}
		else
		{
			if (bIsFullscreen)
				ApplyFullscreen(false); // It was made windowed while it was hidden. It stays hidden

			if (m_PendingPlacement)
			{
				const WindowPlacement placement = *m_PendingPlacement;
				m_PendingPlacement.reset();
				ApplyWindowPlacement(hwnd, placement, placement.bMaximized ? SW_SHOWMAXIMIZED : SW_SHOWNORMAL);
			}
			else
			{
				glfwShowWindow(m_Window);
			}
		}
		glfwFocusWindow(m_Window);
	}

	void WindowsWindow::Uncloak()
	{
		SetCloaked(false);
	}

	void WindowsWindow::Hide()
	{
		if (!m_bShown)
			return;

		glfwHideWindow(m_Window);
		m_bShown = false;
	}

	void WindowsWindow::SetCloaked(bool bCloaked)
	{
		if (m_bCloaked == bCloaked)
			return;

		const BOOL value = bCloaked ? TRUE : FALSE;
		if (SUCCEEDED(DwmSetWindowAttribute((HWND)GetNativeWindow(), DWMWA_CLOAK, &value, sizeof(value))))
			m_bCloaked = bCloaked;
	}

	void WindowsWindow::SetWindowTitle(const std::string& title)
	{
		m_Props.Title = title;
		glfwSetWindowTitle(m_Window, title.c_str());
	}

	void WindowsWindow::SetWindowIcon(const Path& iconPath)
	{
		m_Props.IconPath = iconPath;

		// The icons are created at the exact sizes the OS uses, from a high quality downscale of the image
		WindowsImage::WICContext wic;
		WindowsImage::ComPtr<IWICBitmapSource> image = WindowsImage::LoadImageFile(wic.GetFactory(), iconPath);
		if (!image)
			return;

		HWND hwnd = (HWND)GetNativeWindow();
		const UINT dpi = GetDpiForWindow(hwnd);
		HICON bigIcon = WindowsImage::CreateIcon(wic.GetFactory(), image.Get(), GetSystemMetricsForDpi(SM_CXICON, dpi));
		HICON smallIcon = WindowsImage::CreateIcon(wic.GetFactory(), image.Get(), GetSystemMetricsForDpi(SM_CXSMICON, dpi));
		if (!bigIcon || !smallIcon)
		{
			EG_CORE_ERROR("Failed to create the window icon from: {}", iconPath);
			if (bigIcon)
				DestroyIcon(bigIcon);
			if (smallIcon)
				DestroyIcon(smallIcon);
			return;
		}

		SendMessageW(hwnd, WM_SETICON, ICON_BIG, (LPARAM)bigIcon);
		SendMessageW(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)smallIcon);

		if (m_BigIcon)
			DestroyIcon((HICON)m_BigIcon);
		if (m_SmallIcon)
			DestroyIcon((HICON)m_SmallIcon);

		m_BigIcon = bigIcon;
		m_SmallIcon = smallIcon;
	}

	void WindowsWindow::SetFullscreen(bool bFullscreen)
	{
		if (!m_bShown)
		{
			m_Props.Fullscreen = bFullscreen;
			return;
		}

		if (m_Props.Fullscreen == bFullscreen)
			return;

		ApplyFullscreen(bFullscreen);

		if (!bFullscreen && m_PendingPlacement)
		{
			const WindowPlacement placement = *m_PendingPlacement;
			m_PendingPlacement.reset();
			ApplyWindowPlacement((HWND)GetNativeWindow(), placement, placement.bMaximized ? SW_SHOWMAXIMIZED : SW_SHOWNORMAL);
		}
	}

	void WindowsWindow::ApplyFullscreen(bool bFullscreen)
	{
		// A fullscreen window has no frame, no resizable edges, no caption
		if (bFullscreen && m_Props.bCustomTitleBar)
			CustomWindowFrame::SetEnabled(GetNativeWindow(), false);

		m_Props.Fullscreen = bFullscreen;
		GLFWmonitor* monitor = bFullscreen ? glfwGetPrimaryMonitor() : nullptr;
		const GLFWvidmode* mode = bFullscreen ? glfwGetVideoMode(monitor) : nullptr;

		glm::ivec2 windowSize = mode ? glm::ivec2(mode->width, mode->height) : glm::ivec2(GetWindowSize());
		const glm::ivec2 windowPos = GetWindowPos();
		glfwSetWindowMonitor(m_Window, monitor, windowPos.x, windowPos.y, windowSize.x, windowSize.y, GLFW_DONT_CARE);
		glfwGetWindowSize(m_Window, &windowSize.x, &windowSize.y);
		m_Props.Width = uint32_t(windowSize.x);
		m_Props.Height = uint32_t(windowSize.y);

		if (!bFullscreen && m_Props.bCustomTitleBar)
			CustomWindowFrame::SetEnabled(GetNativeWindow(), true);
	}

	void WindowsWindow::Minimize()
	{
		glfwIconifyWindow(m_Window);
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
		DestroyModalLoopHandler();

		m_Swapchain.reset();

		glfwDestroyWindow(m_Window);
		glfwTerminate();

		if (m_BigIcon)
			DestroyIcon((HICON)m_BigIcon);
		if (m_SmallIcon)
			DestroyIcon((HICON)m_SmallIcon);

		m_BigIcon = m_SmallIcon = nullptr;
		m_Window = nullptr;
		s_GLFWInitialized = false;
	}

	void WindowsWindow::ProcessEvents()
	{
		EG_CORE_ASSERT(!m_bInModalLoopCallback, "Events can't be processed from within the modal loop callback");
		glfwPollEvents();
	}

	// How the app keeps running while the user moves or resizes a window:
	// `glfwPollEvents()` doesn't return until the mouse is released, since the OS runs its own modal loop meanwhile. That loop still dispatches
	// posted & timer messages of the thread's windows, so the frames are run from the window procedure of a hidden window (`ModalLoopWindowProc()`):
	//  - After each move/resize step. A thread hook sees `WM_WINDOWPOSCHANGED` of any of the thread's windows (ImGui's platform windows included),
	//    and posts a message to the hidden window. The modal loop dispatches posted messages before it handles the next mouse input,
	//    so each step is followed by a frame laid out for the new size. Frames block until the renderer is ready for the next one,
	//    so the steps follow the display's refresh rate (the mouse moves that happen meanwhile are merged into one step by the OS).
	//  - From a timer, when nothing moves (for example, the mouse is held still), so that the content stays animated.
	//    Timer messages are only generated when there's nothing else to process, so they don't compete with the steps.
	// The modal loop is tracked from the `WM_ENTERSIZEMOVE`/`WM_EXITSIZEMOVE` (and menu loop) messages, also seen by the hook
	void WindowsWindow::CreateModalLoopHandler()
	{
		EG_CORE_ASSERT(s_ModalLoopWindow == nullptr, "Only one window can handle the modal loops");
		const HINSTANCE instance = GetModuleHandleW(nullptr);

		WNDCLASSEXW windowClass{};
		windowClass.cbSize = sizeof(windowClass);
		windowClass.lpfnWndProc = &WindowsWindow::ModalLoopWindowProc;
		windowClass.hInstance = instance;
		windowClass.lpszClassName = s_ModalLoopWindowClass;

		// The class stays registered if the window is recreated
		if (!RegisterClassExW(&windowClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
		{
			EG_CORE_ERROR("Failed to register the modal loop window class (error {}). Windows won't update while they're moved or resized", GetLastError());
			return;
		}

		// A message-only window: it's never shown and it doesn't receive broadcasts, it only receives the messages for it
		const HWND hwnd = CreateWindowExW(0, s_ModalLoopWindowClass, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, instance, nullptr);
		if (!hwnd)
		{
			EG_CORE_ERROR("Failed to create the modal loop window (error {}). Windows won't update while they're moved or resized", GetLastError());
			return;
		}

		// A thread hook (not a global one): it only sees the messages sent to this thread's windows. Called after the window procedure processed them
		const HHOOK hook = SetWindowsHookExW(WH_CALLWNDPROCRET, &WindowsWindow::ModalLoopHookProc, nullptr, GetCurrentThreadId());
		if (!hook)
		{
			EG_CORE_ERROR("Failed to install the modal loop hook (error {}). Windows won't update while they're moved or resized", GetLastError());
			DestroyWindow(hwnd);
			return;
		}

		SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)this);
		m_ModalLoopWindow = hwnd;
		m_ModalLoopHook = hook;
		s_ModalLoopWindow = this;
	}

	void WindowsWindow::DestroyModalLoopHandler()
	{
		if (m_ModalLoopHook)
		{
			UnhookWindowsHookEx((HHOOK)m_ModalLoopHook);
			m_ModalLoopHook = nullptr;
		}
		if (m_ModalLoopWindow)
		{
			DestroyWindow((HWND)m_ModalLoopWindow); // Its timer is destroyed with it
			m_ModalLoopWindow = nullptr;
		}
		if (s_ModalLoopWindow == this)
			s_ModalLoopWindow = nullptr;

		m_ModalLoopCallback = {};
		m_ModalLoopDepth = 0;
		m_bModalLoopFramePosted = false;
	}

	void WindowsWindow::OnWindowMessageProcessed(const CWPRETSTRUCT& message)
	{
		switch (message.message)
		{
			case WM_ENTERSIZEMOVE:
			case WM_ENTERMENULOOP:
			{
				if (m_ModalLoopDepth++ == 0)
					SetTimer((HWND)m_ModalLoopWindow, s_ModalLoopTimerID, USER_TIMER_MINIMUM, nullptr);
				break;
			}
			case WM_EXITSIZEMOVE:
			case WM_EXITMENULOOP:
			{
				if (m_ModalLoopDepth > 0 && --m_ModalLoopDepth == 0)
					KillTimer((HWND)m_ModalLoopWindow, s_ModalLoopTimerID);
				break;
			}
			case WM_WINDOWPOSCHANGED:
			{
				// Changes made by the frame itself (ImGui moving its platform windows) don't need another frame
				if (m_ModalLoopDepth == 0 || m_bInModalLoopCallback || m_bModalLoopFramePosted)
					break;

				const WINDOWPOS& pos = *(const WINDOWPOS*)message.lParam;
				if ((pos.flags & SWP_NOSIZE) == 0 || (pos.flags & SWP_NOMOVE) == 0)
					m_bModalLoopFramePosted = PostMessageW((HWND)m_ModalLoopWindow, s_ModalLoopFrameMessage, 0, 0) != FALSE;
				break;
			}
		}
	}

	void WindowsWindow::RunModalLoopFrame()
	{
		// The OS has the final say. For example, if a window is destroyed while it's being moved, the loop might end without `WM_EXITSIZEMOVE`.
		// Also, a posted frame message might only be dispatched after the loop ended, by `glfwPollEvents()` itself
		GUITHREADINFO info{};
		info.cbSize = sizeof(info);
		const bool bInModalLoop = GetGUIThreadInfo(GetCurrentThreadId(), &info) && (info.flags & (GUI_INMOVESIZE | GUI_INMENUMODE)) != 0;
		if (!bInModalLoop)
		{
			if (m_ModalLoopDepth > 0)
			{
				m_ModalLoopDepth = 0;
				KillTimer((HWND)m_ModalLoopWindow, s_ModalLoopTimerID);
			}
			return;
		}

		if (m_bInModalLoopCallback || !m_ModalLoopCallback)
			return;

		m_bInModalLoopCallback = true;
		m_ModalLoopCallback();
		m_bInModalLoopCallback = false;

		// Restart the timer's countdown, so that it only fires once no frame was run for a while.
		// Otherwise it could fire right after a frame, and its frame would delay the next move/resize step until the renderer is ready again
		if (m_ModalLoopDepth > 0)
			SetTimer((HWND)m_ModalLoopWindow, s_ModalLoopTimerID, USER_TIMER_MINIMUM, nullptr);
	}

	LRESULT CALLBACK WindowsWindow::ModalLoopWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		const bool bTimer = msg == WM_TIMER && wParam == s_ModalLoopTimerID;
		if (bTimer || msg == s_ModalLoopFrameMessage)
		{
			if (WindowsWindow* window = (WindowsWindow*)GetWindowLongPtrW(hwnd, GWLP_USERDATA))
			{
				if (!bTimer)
					window->m_bModalLoopFramePosted = false;
				window->RunModalLoopFrame();
			}
			return 0;
		}
		return DefWindowProcW(hwnd, msg, wParam, lParam);
	}

	LRESULT CALLBACK WindowsWindow::ModalLoopHookProc(int code, WPARAM wParam, LPARAM lParam)
	{
		if (code == HC_ACTION && s_ModalLoopWindow)
			s_ModalLoopWindow->OnWindowMessageProcessed(*(const CWPRETSTRUCT*)lParam);
		return CallNextHookEx(nullptr, code, wParam, lParam);
	}

	void* WindowsWindow::GetNativeWindow() const
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
			if (data.EventCallback)
				data.EventCallback(event);
		});

		glfwSetWindowCloseCallback(m_Window, [](GLFWwindow* window)
		{
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);

			WindowCloseEvent event(false);
			if (data.EventCallback)
				data.EventCallback(event);
		});

		glfwSetWindowFocusCallback(m_Window, [](GLFWwindow* window, int focus)
		{
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
			WindowFocusedEvent event(focus == GLFW_TRUE);
			if (data.EventCallback)
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
				if (data.EventCallback)
					data.EventCallback(event);
				break;
			}
			case GLFW_RELEASE:
			{
				KeyReleasedEvent event((Key)key);
				if (data.EventCallback)
					data.EventCallback(event);
				break;
			}
			case GLFW_REPEAT:
			{
				KeyPressedEvent event((Key)key, 1);
				if (data.EventCallback)
					data.EventCallback(event);
				break;
			}
			}
		});

		glfwSetCharCallback(m_Window, [](GLFWwindow* window, unsigned int keyCode)
		{
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
			KeyTypedEvent event((Key)keyCode);
			if (data.EventCallback)
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
					if (data.EventCallback)
						data.EventCallback(event);
					break;
				}
				case GLFW_RELEASE:
				{
					MouseButtonReleasedEvent event((Mouse)button);
					if (data.EventCallback)
						data.EventCallback(event);
					break;
				}
			}
		});

		glfwSetScrollCallback(m_Window, [](GLFWwindow* window, double xOffset, double yOffset)
		{
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);

			MouseScrolledEvent event((float)xOffset, (float)yOffset);
			if (data.EventCallback)
				data.EventCallback(event);
		});

		glfwSetCursorPosCallback(m_Window, [](GLFWwindow* window, double xPos, double yPos)
		{
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);

			MouseMovedEvent event((float)xPos, (float)yPos);
			if (data.EventCallback)
				data.EventCallback(event);
		});

		glfwSetWindowContentScaleCallback(m_Window, [](GLFWwindow* window, float xscale, float yscale)
		{
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);

			*data.DPIScale = glm::max(xscale, yscale);

			WindowContentScaleEvent event(xscale, yscale);
			if (data.EventCallback)
				data.EventCallback(event);
		});
	}
}
