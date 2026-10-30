#pragma once

#include "Eagle/Core/Core.h"
#include "Eagle/Core/CustomWindowFrame.h"

#include <imgui.h>
#include <functional>
#include <string_view>

namespace Eagle
{
	class Texture2D;

	struct TitleBarDesc
	{
		std::string_view Title;

		// Drawn on the left. Optional
		Ref<Texture2D> Icon;

		// Called within a menu bar, so it should submit `ImGui::BeginMenu()` items. Optional
		std::function<void()> DrawMenus;

		// Called when the close button is clicked. If it's not set, there's no close button
		std::function<void()> OnClose;
	};

	// The title bar of the editor's windows: an icon, menus, a title and minimize/maximize/close buttons.
	// It replaces the OS title bar of windows with a custom frame (see `CustomWindowFrame`), and it's drawn at the top of an ImGui window
	class TitleBar
	{
	public:
		static float GetHeight();
		static ImU32 GetBackgroundColor();

		// Must be called once per frame, before any title bar is drawn
		static void NewFrame();

		// Draws a title bar at the top of the current window
		static void Draw(const TitleBarDesc& desc);

		// What's under the cursor in the platform window of the viewport, based on the title bars drawn in the last frame
		static WindowHitTest HitTest(ImGuiID viewportID, const glm::ivec2& screenPos);
		static WindowHitTestCallback MakeHitTestCallback(ImGuiID viewportID);

		// Thickness of the edges of the viewport's platform window that resize it. 0 if it's maximized, fullscreen, or doesn't have a custom frame.
		static float GetResizeBorder(const ImGuiViewport* viewport);

		// Same as closing the main window by the OS (for example, Alt+F4).
		// It sends `WindowCloseEvent` on the next frame, so that the layers can handle it
		static void RequestAppClose();
	};
}
