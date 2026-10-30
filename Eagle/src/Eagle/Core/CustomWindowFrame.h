#pragma once

#include <cstdint>
#include <functional>
#include <glm/glm.hpp>

namespace Eagle
{
	// What's under the cursor in a window that draws its own title bar
	enum class WindowHitTest : uint8_t
	{
		// Regular content. Everything interactive in the title bar (menus, buttons, tabs, etc.) is the client area too
		Client,

		// Draggable part of the title bar. Dragging it moves the window (and lets the OS snap it), double-clicking it maximizes/restores the window,
		// and right-clicking it opens the system menu
		Caption,

		// The maximize/restore button. On Windows 11, hovering it shows the snap layouts flyout.
		// Its clicks are handled by `CustomWindowFrame` (they toggle the maximized state), see `CustomWindowFrame::GetMaximizeButtonState()`
		MaximizeButton
	};

	// @screenPos. Position of the cursor in screen coordinates (the same space as `Window::GetWindowPos()` and ImGui platform windows)
	using WindowHitTestCallback = std::function<WindowHitTest(const glm::ivec2& screenPos)>;

	// Replaces the OS decorations of a native window (its title bar and borders) with the ones that the app draws, while keeping the native behaviour:
	//		resizing by the edges, moving & snapping by dragging the title bar, double-click to maximize,
	//		the system menu (right-click on the title bar, Alt+Space), minimize/maximize animations, the shadow, and snap layouts of the maximize button (Windows 11).
	// The app reports what's under the cursor with a `WindowHitTestCallback`.
	// The window must be created without OS decorations (`GLFW_DECORATED` set to false), so that GLFW doesn't account for a frame
	// when it positions and resizes the window (the client area takes the whole window).
	class CustomWindowFrame
	{
	public:
		struct ButtonState
		{
			bool bHovered = false;
			bool bPressed = false;
		};

		// @nativeWindow. Native handle of the window (HWND on Windows)
		// @hitTest. Called whenever the OS needs to know what's under the cursor. It's called on the thread that owns the window, while it processes events.
		//			 The edges of the window that resize it are handled before it's called
		// @minSize. The window can't be resized below it (in pixels). Zero - no limit
		// @iconSource. Optional native window whose icons are used by this window as well
		// Returns false if custom frames aren't supported (or the handle is invalid). If the frame is already installed, its parameters are updated
		static bool Install(void* nativeWindow, const WindowHitTestCallback& hitTest, const glm::ivec2& minSize = glm::ivec2(0), void* iconSource = nullptr);
		static void Uninstall(void* nativeWindow);
		static bool IsInstalled(void* nativeWindow);

		static void SetHitTestCallback(void* nativeWindow, const WindowHitTestCallback& hitTest);

		// A disabled frame behaves like a plain borderless window: no resizable edges, no caption, etc. For example, it's disabled while the window is fullscreen
		static void SetEnabled(void* nativeWindow, bool bEnabled);
		static bool IsEnabled(void* nativeWindow);

		// Thickness of the edges of the window that resize it, in pixels. They're inside the window and overlap its content,
		// so the app should keep them free of interactive UI.
		// Zero if the window is maximized or minimized, if the frame is disabled, or if it's not installed
		static int GetResizeBorderThickness(void* nativeWindow);

		// The maximize button is clicked through the OS (see `WindowHitTest::MaximizeButton`), so the app doesn't receive the mouse events while it's over it.
		// Its state is reported here, so that the app can draw it
		static ButtonState GetMaximizeButtonState(void* nativeWindow);

		static void Minimize(void* nativeWindow);
		static void Maximize(void* nativeWindow);
		static void Restore(void* nativeWindow);
		static bool IsMaximized(void* nativeWindow);
	};
}
