#pragma once

#include "Eagle/Core/Core.h"

#include <imgui.h>

namespace Eagle
{
	class Texture2D;

	// A top-level tab of the editor.
	// The title bar of a frame shows the menus of its active tab
	class MajorTab
	{
	public:
		virtual ~MajorTab() = default;

		// Name of the ImGui window of the tab
		virtual const std::string& GetTabWindowName() const = 0;

		// Shown in the title bar of a floating frame (and in its taskbar button) while the tab is active there
		virtual std::string GetTabTitle() const = 0;

		// Menus of the title bar while the tab is active. It's called within a menu bar, so it should submit `ImGui::BeginMenu()` items
		virtual void DrawTabMenus() {}

		virtual void CloseTab() {}

		// If true, the tab always stays in the main frame: it can't be dragged out of it
		virtual bool IsPinnedToMainFrame() const { return false; }
	};

	// Frames are the editor's windows: they have the editor's title bar, and a tab well that holds major tabs (see `MajorTab`).
	// The main frame covers the main window, and new tabs are opened there.
	// When a tab is dragged out of a tab well, it gets its own floating frame: a separate OS window with the same title bar and tab well,
	// that other tabs can be docked into. Tabs can be dragged back into any tab well, and a floating frame closes once it has no tabs left.
	// Tab wells only accept major tabs, and the panels of a major tab can only be docked within it (the tab has its own dockspace)
	class EditorFrames
	{
	public:
		// @icon. Shown in the title bars
		static void Init(const Ref<Texture2D>& icon);

		static void Shutdown();

		// Draws the frames. Must be called every frame, before any major tab is submitted
		static void BeginFrame();

		// Must be called every frame, after all major tabs were submitted
		static void EndFrame();

		// Must be called right before `ImGui::Begin()` of a major tab's window, every frame. The tab is registered by the first call
		static void PrepareTabWindow(MajorTab* tab);

		static void RemoveTab(MajorTab* tab);

		static void FocusTab(MajorTab* tab);

		// Recommended flags of major tabs' windows. Their placement is managed by the frames, so it's not saved
		static constexpr ImGuiWindowFlags TabWindowFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;
	};
}
