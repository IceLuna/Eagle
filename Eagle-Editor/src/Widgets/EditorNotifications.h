#pragma once

#include <imgui.h>

namespace Eagle
{
	// Short messages in the bottom right corner of an editor window (an OS window) that fade out after a few seconds.
	// For example, "Undo: Move Entity". They don't take input, so they never get in the way.
	// The newest one is at the bottom, older ones are pushed up
	class EditorNotifications
	{
	public:
		// @viewportID. The ImGui viewport of the window to show it in (for example, the one of a floating frame). 0 - the main window.
		// If the viewport is gone by the time it's drawn, it's shown in the main window
		static void Show(std::string text, ImGuiID viewportID = 0);

		// Must be called every frame, after the editor's windows were submitted
		static void Draw();
	};
}
