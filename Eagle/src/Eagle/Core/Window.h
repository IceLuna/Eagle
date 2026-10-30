#pragma once

#include "Core.h"
#include "CustomWindowFrame.h"
#include "Eagle/Events/Event.h"
#include "Eagle/Renderer/RendererContext.h"

#include <functional>

struct GLFWwindow;

namespace Eagle
{
	struct WindowProperties
	{
		std::string Title = "Untitled";
		uint32_t Width = 1280;
		uint32_t Height = 720;
		bool VSync = true;
		bool Fullscreen = false;
		Path IconPath; // Optional

		// The OS title bar and borders are hidden, and the app draws its own title bar. The window still behaves like a regular one. See `CustomWindowFrame`.
		// The app must report the draggable parts of its title bar with `SetTitleBarHitTestCallback()`
		bool bCustomTitleBar = false;

		WindowProperties() = default;
		WindowProperties(const std::string& title, uint32_t width, uint32_t height, bool bVSync, bool bFullscreen)
			: Title(title), Width(width), Height(height), VSync(bVSync), Fullscreen(bFullscreen) {}
	};

	// The size & position the window has when it's not maximized and not fullscreen
	struct WindowPlacement
	{
		glm::ivec2 Pos = glm::ivec2(0);
		glm::ivec2 Size = glm::ivec2(0);
		bool bMaximized = false;
	};

	class VulkanSwapchain;

	// Window interface
	// A window must be created hidden, so that it's shown once the app is ready and has something to show.
	// While it's hidden, changes of its placement & fullscreen state should be stored, and applied when it's shown
	class Window
	{
	public:
		using EventCallbackFn = std::function<void(Event&)>;
		using ModalLoopCallbackFn = std::function<void()>;

		Window(const WindowProperties& props) : m_Props(props) {}
		virtual ~Window() = default;

		virtual void ProcessEvents() = 0;

		uint32_t GetWidth() const { return m_Props.Width; }
		uint32_t GetHeight() const { return m_Props.Height; }
		bool IsVSync() const { return m_Props.VSync; }

		virtual void* GetNativeWindow() const = 0;
		GLFWwindow* GetGLFWWindow() const { return m_Window; }

		//Window attributes
		virtual void SetEventCallback(const EventCallbackFn& callback) = 0;
		virtual void SetVSync(bool enable) = 0;
		virtual void SetFocus(bool focus) = 0;
		virtual void SetWindowSize(int width, int height) = 0;
		virtual void SetWindowMaximized(bool bMaximize) = 0;
		virtual void SetWindowTitle(const std::string& title) = 0;
		virtual void SetWindowPos(int x, int y) = 0;
		virtual void SetWindowIcon(const Path& iconPath) = 0;
		virtual void SetFullscreen(bool bFullscreen) = 0;
		virtual void Minimize() = 0;

		// @bCloaked. The window is shown (it's activated, gets a taskbar button, etc), but the OS doesn't draw it until `Uncloak()` is called.
		// It's required so that the first thing that's visible is a frame rendered at the final size, not an empty/broken window state
		virtual void Show(bool bCloaked = false) = 0;
		virtual void Uncloak() = 0;
		virtual void Hide() = 0;
		virtual bool IsVisible() const = 0;

		// These are used to save & restore the window's state.
		// Unlike the size & position, the placement is the same while the window is maximized
		virtual WindowPlacement GetWindowPlacement() const = 0;
		virtual void SetWindowPlacement(const WindowPlacement& placement) = 0;

		// While the user moves or resizes one of the app's windows (including ImGui's platform windows), the OS runs its own modal loop,
		// so `ProcessEvents()` doesn't return until the mouse is released. The callback is called from within that loop after each move/resize step,
		// and periodically while nothing moves, so that the app keeps rendering (otherwise, the window's content would only update once the mouse is released)
		virtual void SetModalLoopCallback(const ModalLoopCallbackFn& callback) = 0;

		// Only used if the window has a custom title bar.
		// Called while the window processes its events, whenever the OS needs to know what's under the cursor.
		// If it's not set, the whole window is the client area, so it can't be dragged
		virtual void SetTitleBarHitTestCallback(const WindowHitTestCallback& callback) = 0;
		bool HasCustomTitleBar() const { return m_Props.bCustomTitleBar; }

		virtual glm::vec2 GetWindowSize() const = 0;
		virtual bool IsMaximized() const = 0;
		virtual glm::vec2 GetWindowPos() const = 0;
		const std::string& GetWindowTitle() const { return m_Props.Title; }
		virtual Ref<VulkanSwapchain>& GetSwapchain() = 0;
		bool IsFullscreen() const { return m_Props.Fullscreen; }
		float GetDPIScale() const { return m_DPIScale; }

		static Ref<Window> Create(const WindowProperties& props);

	protected:
		WindowProperties m_Props;
		GLFWwindow* m_Window;
		float m_DPIScale = 1.f;
	};
}