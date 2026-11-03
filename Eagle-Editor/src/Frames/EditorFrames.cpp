#include "EditorFrames.h"
#include "TitleBar.h"

#include "Eagle/Core/Application.h"
#include "Eagle/Core/CustomWindowFrame.h"

#include <imgui_internal.h>

namespace Eagle
{
	namespace
	{
		struct Frame
		{
			uint32_t Index = 0; // Unique among the existing frames. Indices of closed frames are reused
			ImGuiID WindowID = 0;
			ImGuiID DockspaceID = 0;
			ImVec2 InitialPos;
			ImVec2 InitialSize;
			int EmptyFrames = 0; // Number of consecutive frames without tabs
			bool bAppearing = true;
			bool bRemove = false;
		};

		struct TabState
		{
			MajorTab* Tab = nullptr;
			ImGuiID WindowID = 0;
			ImGuiID PendingDockID = 0; // The tab is docked into this tab well by the next `PrepareTabWindow()`
			int FloatingFrames = 0;    // Number of consecutive frames the tab has been floating outside of tab wells
			bool bAppeared = false;    // `PrepareTabWindow()` has been called
			bool bSelectWhenDocked = false; // Make it the active tab of its tab well once it's docked there
		};

		// The windows submitted within a `TabContentScope`, by their begin order within the frame (`ImGuiWindow::BeginOrderWithinContext`)
		struct TabContentRange
		{
			MajorTab* Tab = nullptr;
			int Begin = 0; // The first window
			int End = 0;   // After the last one
		};

		std::vector<TabState> s_Tabs;
		MajorTab* s_LastFocusedTab = nullptr;
		std::vector<TabContentRange> s_OpenTabContentScopes; // `End` isn't set yet
		std::vector<TabContentRange> s_TabContentRanges; // Of the current frame
		std::unordered_map<ImGuiID, MajorTab*> s_WindowTabs; // The windows that were submitted within a `TabContentScope` last frame, and their tabs
		std::vector<Frame> s_Frames;
		Ref<Texture2D> s_Icon;
		ImGuiWindowClass s_TabWindowClass;
		ImGuiWindowClass s_FrameWindowClass;

		constexpr const char* s_MainFrameWindowName = "##EagleMainFrame";

		// A tab well holds major tabs only (see `s_TabWindowClass`), and only as tabs (it can't be split)
		constexpr ImGuiDockNodeFlags s_TabWellFlags = ImGuiDockNodeFlags_NoDockingSplit | ImGuiDockNodeFlags_NoWindowMenuButton | ImGuiDockNodeFlags_NoCloseButton;

		// The platform window of a frame moves & resizes it (see `CustomWindowFrame`). Nothing can be docked into a frame, except into its tab well
		constexpr ImGuiWindowFlags s_FrameWindowFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
			| ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings
			| ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

		// Number of frames a tab has to float before it gets its own frame. A tab that was dropped onto a tab well floats for a frame,
		// since ImGui docks it at the beginning of the next frame
		constexpr int s_FramesToWrapFloatingTab = 2;

		ImGuiID GetMainDockspaceID()
		{
			return ImHashStr("EagleTabWell_Main");
		}

		ImGuiID GetFrameDockspaceID(uint32_t index)
		{
			return ImHashStr(("EagleTabWell_Frame" + std::to_string(index)).c_str());
		}

		// The title is a part of the name, so that it's the title of the platform window as well. The ID only depends on the index
		std::string GetFrameWindowName(const std::string& title, uint32_t index)
		{
			return title + "###EagleFrame_" + std::to_string(index);
		}

		TabState* FindTab(const MajorTab* tab)
		{
			for (TabState& state : s_Tabs)
			{
				if (state.Tab == tab)
					return &state;
			}

			return nullptr;
		}

		bool IsTabWell(ImGuiID dockNodeID)
		{
			if (dockNodeID == GetMainDockspaceID())
				return true;

			for (const Frame& frame : s_Frames)
			{
				if (frame.DockspaceID == dockNodeID && !frame.bRemove)
					return true;
			}

			return false;
		}

		// Which tab each window that was submitted this frame belongs to (see `TabContentScope`)
		void UpdateWindowTabs()
		{
			const ImGuiContext& g = *GImGui;
			s_WindowTabs.clear();
			for (const ImGuiWindow* window : g.Windows)
			{
				if (window->LastFrameActive != g.FrameCount)
					continue; // Not submitted this frame, its begin order is from an earlier one

				// The innermost scope, in case they're nested
				const TabContentRange* owner = nullptr;
				for (const TabContentRange& range : s_TabContentRanges)
				{
					const int order = window->BeginOrderWithinContext;
					if (order >= range.Begin && order < range.End && (!owner || range.End - range.Begin < owner->End - owner->Begin))
						owner = &range;
				}
				if (owner)
					s_WindowTabs[window->ID] = owner->Tab;
			}
			s_TabContentRanges.clear();
		}

		// The tab of the focused window. It's either one of the windows of the tab (see `TabContentScope`), or leads to one:
		// docked panels lead to the window that hosts their dockspace, popups and child windows lead to the window that opened them
		MajorTab* FindFocusedTab()
		{
			constexpr int maxDepth = 64; // Just in case, the chain is short
			const ImGuiWindow* window = GImGui->NavWindow;
			for (int depth = 0; window && depth < maxDepth; ++depth)
			{
				if (auto it = s_WindowTabs.find(window->ID); it != s_WindowTabs.end())
					return it->second;

				for (const TabState& state : s_Tabs)
				{
					if (state.WindowID == window->ID)
						return state.Tab;
				}

				if (window->DockNode && window->DockNode->HostWindow && window->DockNode->HostWindow != window)
					window = window->DockNode->HostWindow;
				else if (window->ParentWindowInBeginStack)
					window = window->ParentWindowInBeginStack;
				else
					window = window->ParentWindow;
			}
			return nullptr;
		}

		MajorTab* GetActiveTab(ImGuiID tabWellID)
		{
			const ImGuiDockNode* node = ImGui::DockBuilderGetNode(tabWellID);
			if (!node || !node->VisibleWindow)
				return nullptr;

			for (const TabState& state : s_Tabs)
			{
				if (state.WindowID == node->VisibleWindow->ID)
					return state.Tab;
			}

			return nullptr;
		}

		bool OwnsPlatformWindow(const ImGuiWindow* window)
		{
			const ImGuiViewportP* viewport = window->Viewport;
			return viewport && viewport->PlatformHandleRaw && (viewport == ImGui::GetMainViewport() || viewport->Window == window);
		}

		uint32_t GetFreeFrameIndex()
		{
			uint32_t index = 1;
			while (std::any_of(s_Frames.begin(), s_Frames.end(), [index](const Frame& frame) { return frame.Index == index; }))
				++index;
			return index;
		}

		ImVec2 GetMinFrameSize()
		{
			const float fontSize = ImGui::GetFontSize();
			return ImVec2(fontSize * 28.f, fontSize * 16.f);
		}

		void BeginFrameWindow(const char* name)
		{
			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
			ImGui::PushStyleColor(ImGuiCol_WindowBg, TitleBar::GetBackgroundColor()); // The edges around the tab well are a part of the frame
			ImGui::Begin(name, nullptr, s_FrameWindowFlags);
			ImGui::PopStyleColor();
			ImGui::PopStyleVar(3);
		}

		// The tab well and the title bar of the current frame window
		void DrawFrameContents(ImGuiID tabWellID, bool bTitleBar, const std::string& title, const std::function<void()>& onClose)
		{
			ImGuiWindow* window = ImGui::GetCurrentWindow();
			const float titleBarHeight = bTitleBar ? TitleBar::GetHeight() : 0.f;

			// The edges of the platform window resize it, so the tab well is inset by them
			const float border = OwnsPlatformWindow(window) ? TitleBar::GetResizeBorder(window->Viewport) : 0.f;
			const ImVec2 tabWellPos(window->Pos.x + border, window->Pos.y + titleBarHeight);
			const ImVec2 tabWellSize(ImMax(1.f, window->Size.x - border * 2.f), ImMax(1.f, window->Size.y - titleBarHeight - border));

			// The tab well is submitted first, so that the title bar shows the menus of the tab that's active in this frame
			ImGui::SetCursorScreenPos(tabWellPos);
			ImGui::DockSpace(tabWellID, tabWellSize, s_TabWellFlags, &s_TabWindowClass);

			if (bTitleBar)
			{
				MajorTab* activeTab = GetActiveTab(tabWellID);

				TitleBarDesc desc;
				desc.Title = title;
				desc.Icon = s_Icon;
				desc.OnClose = onClose;
				if (activeTab)
				{
					desc.DrawMenus = [activeTab]()
					{
						EditorFrames::TabContentScope scope(activeTab);
						activeTab->DrawTabMenus();
					};
				}

				ImGui::SetCursorScreenPos(window->Pos);
				TitleBar::Draw(desc);
			}
		}

		void DrawMainFrame()
		{
			const ImGuiViewport* viewport = ImGui::GetMainViewport();
			const Window& appWindow = Application::Get().GetWindow();

			ImGui::SetNextWindowPos(viewport->Pos);
			ImGui::SetNextWindowSize(viewport->Size);
			ImGui::SetNextWindowViewport(viewport->ID);
			BeginFrameWindow(s_MainFrameWindowName);

			// A fullscreen window doesn't have a title bar. And it's not needed if the OS draws one
			const bool bTitleBar = appWindow.HasCustomTitleBar() && !appWindow.IsFullscreen();
			DrawFrameContents(GetMainDockspaceID(), bTitleBar, appWindow.GetWindowTitle(), &TitleBar::RequestAppClose);

			ImGui::End();
		}

		void CloseFrameTabs(const Frame& frame)
		{
			// Gathered first: closing a tab can modify the list of tabs
			std::vector<MajorTab*> tabs;
			for (const TabState& state : s_Tabs)
			{
				const ImGuiWindow* window = ImGui::FindWindowByID(state.WindowID);
				if (window && window->DockId == frame.DockspaceID)
					tabs.push_back(state.Tab);
			}
			for (MajorTab* tab : tabs)
				tab->CloseTab();
		}

		void DrawFloatingFrame(Frame& frame)
		{
			const MajorTab* activeTab = GetActiveTab(frame.DockspaceID);
			const std::string title = activeTab ? activeTab->GetTabTitle() : std::string("Eagle Editor");

			if (frame.bAppearing)
			{
				ImGui::SetNextWindowPos(frame.InitialPos);
				ImGui::SetNextWindowSize(frame.InitialSize);
			}
			ImGui::SetNextWindowClass(&s_FrameWindowClass);
			BeginFrameWindow(GetFrameWindowName(title, frame.Index).c_str());
			frame.bAppearing = false;

			// Closed by the OS (for example, Alt+F4)
			ImGuiWindow* window = ImGui::GetCurrentWindow();
			bool bClose = OwnsPlatformWindow(window) && window->Viewport->PlatformRequestClose;

			DrawFrameContents(frame.DockspaceID, true, title, [&bClose]() { bClose = true; });
			ImGui::End();

			if (bClose)
				CloseFrameTabs(frame);
		}

		void WrapFloatingTabs()
		{
			const ImGuiContext& g = *GImGui;

			// Tabs that float together (they were docked into each other) get the same frame
			struct FrameRequest
			{
				ImGuiID FloatingID = 0;
				ImVec2 Pos;
				ImVec2 Size;
				std::vector<TabState*> Tabs;
			};
			std::vector<FrameRequest> requests;

			for (TabState& state : s_Tabs)
			{
				ImGuiWindow* window = ImGui::FindWindowByID(state.WindowID);
				if (!window || !window->Active || state.PendingDockID)
				{
					state.FloatingFrames = 0;
					continue;
				}

				ImGuiDockNode* root = window->DockNode ? ImGui::DockNodeGetRootNode(window->DockNode) : nullptr;
				const bool bInTabWell = root && IsTabWell(root->ID);
				const bool bDragged = g.MovingWindow && g.MovingWindow->RootWindowDockTree == window->RootWindowDockTree;
				if (bInTabWell || bDragged)
				{
					state.FloatingFrames = 0;
					continue;
				}
				if (++state.FloatingFrames < s_FramesToWrapFloatingTab)
					continue;
				state.FloatingFrames = 0;

				if (state.Tab->IsPinnedToMainFrame())
				{
					state.PendingDockID = GetMainDockspaceID();
					continue;
				}

				const ImGuiID floatingID = root ? root->ID : window->ID;
				auto it = std::find_if(requests.begin(), requests.end(), [floatingID](const FrameRequest& request) { return request.FloatingID == floatingID; });
				if (it == requests.end())
				{
					FrameRequest& request = requests.emplace_back();
					request.FloatingID = floatingID;
					if (root && root->HostWindow)
					{
						request.Pos = root->HostWindow->Pos;
						request.Size = root->HostWindow->Size;
					}
					else if (root)
					{
						request.Pos = root->Pos;
						request.Size = root->Size;
					}
					else
					{
						request.Pos = window->Pos;
						request.Size = window->Size;
					}
					it = requests.end() - 1;
				}
				it->Tabs.push_back(&state);
			}

			// The frame takes the place of the floating window, and its tabs are docked into it by the next `PrepareTabWindow()`
			for (const FrameRequest& request : requests)
			{
				Frame frame;
				frame.Index = GetFreeFrameIndex();
				frame.WindowID = ImHashStr(GetFrameWindowName("", frame.Index).c_str());
				frame.DockspaceID = GetFrameDockspaceID(frame.Index);
				frame.InitialPos = request.Pos;
				frame.InitialSize = ImMax(request.Size, GetMinFrameSize());
				for (TabState* state : request.Tabs)
					state->PendingDockID = frame.DockspaceID;
				s_Frames.push_back(frame);
			}
		}

		void UpdateEmptyFrames()
		{
			for (Frame& frame : s_Frames)
			{
				const bool bHasTabs = std::any_of(s_Tabs.begin(), s_Tabs.end(), [&frame](const TabState& state)
				{
					if (state.PendingDockID == frame.DockspaceID)
						return true;

					// A tab that's no longer submitted (for example, it was just closed) doesn't keep the frame alive
					const ImGuiWindow* window = ImGui::FindWindowByID(state.WindowID);
					return window && window->Active && window->DockId == frame.DockspaceID;
				});

				frame.EmptyFrames = bHasTabs ? 0 : frame.EmptyFrames + 1;
				if (frame.EmptyFrames >= 2)
					frame.bRemove = true; // Removed at the beginning of the next frame, before anything is submitted
			}
		}

		// A window docked with `SetNextWindowDockID()` is added as an inactive tab. Focusing it doesn't select it either, since the focus usually goes
		// to the tab's panels when they appear (they're separate windows in the tab's own dockspace). So the tab is selected explicitly
		void SelectDockedTabs()
		{
			for (TabState& state : s_Tabs)
			{
				if (!state.bSelectWhenDocked)
					continue;

				ImGuiWindow* window = ImGui::FindWindowByID(state.WindowID);
				if (!window || !window->Active)
				{
					state.bSelectWhenDocked = false;
					continue;
				}
				if (window->DockNode && window->DockNode->TabBar)
				{
					window->DockNode->TabBar->NextSelectedTabId = window->TabId;
					state.bSelectWhenDocked = false;
				}
			}
		}

		void InstallPlatformWindowFrames()
		{
			// Platform windows of floating frames are created by ImGui at the end of the frame, so they get their custom frame on the next one
			void* mainNativeWindow = Application::Get().GetWindow().GetNativeWindow();
			const ImVec2 minSize = GetMinFrameSize();
			for (const Frame& frame : s_Frames)
			{
				const ImGuiWindow* window = ImGui::FindWindowByID(frame.WindowID);
				if (!window || !window->Active || frame.bRemove || !OwnsPlatformWindow(window) || !window->Viewport->PlatformWindowCreated)
					continue;

				void* nativeWindow = window->Viewport->PlatformHandleRaw;
				if (!CustomWindowFrame::IsInstalled(nativeWindow))
					CustomWindowFrame::Install(nativeWindow, TitleBar::MakeHitTestCallback(window->Viewport->ID), glm::ivec2(int(minSize.x), int(minSize.y)), mainNativeWindow);
			}
		}

		void RemoveFrames(bool bAll)
		{
			for (Frame& frame : s_Frames)
			{
				if (bAll || frame.bRemove)
				{
					if (ImGui::DockBuilderGetNode(frame.DockspaceID))
						ImGui::DockBuilderRemoveNode(frame.DockspaceID);
					frame.bRemove = true;
				}
			}
			s_Frames.erase(std::remove_if(s_Frames.begin(), s_Frames.end(), [](const Frame& frame) { return frame.bRemove; }), s_Frames.end());
		}
	}

	void EditorFrames::Init(const Ref<Texture2D>& icon)
	{
		s_Icon = icon;

		// Major tabs can only be docked into tab wells, and nothing else can be docked into tab wells
		s_TabWindowClass = ImGuiWindowClass();
		s_TabWindowClass.ClassId = ImHashStr("EagleMajorTab");
		s_TabWindowClass.DockingAllowUnclassed = false;

		// A floating frame always has its own platform window with a taskbar button, even if it's within the main window
		s_FrameWindowClass = ImGuiWindowClass();
		s_FrameWindowClass.ViewportFlagsOverrideSet = ImGuiViewportFlags_NoAutoMerge | ImGuiViewportFlags_NoDecoration;
		s_FrameWindowClass.ViewportFlagsOverrideClear = ImGuiViewportFlags_NoTaskBarIcon;
	}

	void EditorFrames::Shutdown()
	{
		// Their dock nodes are removed, so that they don't end up in the ImGui settings
		if (ImGui::GetCurrentContext())
			RemoveFrames(true);
		s_Frames.clear();
		s_Tabs.clear();
		s_Icon.reset();
	}

	void EditorFrames::BeginFrame()
	{
		EG_CORE_ASSERT(s_OpenTabContentScopes.empty(), "A `TabContentScope` was left open");
		s_OpenTabContentScopes.clear();
		s_TabContentRanges.clear();

		TitleBar::NewFrame();
		RemoveFrames(false);

		DrawMainFrame();
		for (Frame& frame : s_Frames)
			DrawFloatingFrame(frame);
	}

	void EditorFrames::EndFrame()
	{
		UpdateWindowTabs();
		if (MajorTab* focusedTab = FindFocusedTab())
			s_LastFocusedTab = focusedTab;

		SelectDockedTabs();
		WrapFloatingTabs();
		UpdateEmptyFrames();
		InstallPlatformWindowFrames();
	}

	void EditorFrames::PrepareTabWindow(MajorTab* tab)
	{
		TabState* state = FindTab(tab);
		if (!state)
		{
			state = &s_Tabs.emplace_back();
			state->Tab = tab;
		}
		state->WindowID = ImHashStr(tab->GetTabWindowName().c_str());

		ImGuiID dockID = state->PendingDockID;
		if (dockID == 0)
		{
			const ImGuiWindow* window = ImGui::FindWindowByID(state->WindowID);
			if (!state->bAppeared)
				dockID = GetMainDockspaceID(); // New tabs open in the main frame
			else if (tab->IsPinnedToMainFrame() && (!window || window->DockId != GetMainDockspaceID()))
				dockID = GetMainDockspaceID();
		}

		ImGui::SetNextWindowClass(&s_TabWindowClass);
		if (dockID)
		{
			ImGui::SetNextWindowDockID(dockID, ImGuiCond_Always);
			state->bSelectWhenDocked = true;
		}

		state->PendingDockID = 0;
		state->bAppeared = true;
	}

	void EditorFrames::RemoveTab(MajorTab* tab)
	{
		if (s_LastFocusedTab == tab)
			s_LastFocusedTab = nullptr;

		for (auto it = s_WindowTabs.begin(); it != s_WindowTabs.end(); )
			it = it->second == tab ? s_WindowTabs.erase(it) : std::next(it);

		auto isTabRange = [tab](const TabContentRange& range) { return range.Tab == tab; };
		s_TabContentRanges.erase(std::remove_if(s_TabContentRanges.begin(), s_TabContentRanges.end(), isTabRange), s_TabContentRanges.end());
		s_Tabs.erase(std::remove_if(s_Tabs.begin(), s_Tabs.end(), [tab](const TabState& state) { return state.Tab == tab; }), s_Tabs.end());

		for (TabContentRange& range : s_OpenTabContentScopes)
		{
			if (range.Tab == tab)
				range.Tab = nullptr; // Its windows don't belong to any tab
		}
	}

	void EditorFrames::FocusTab(MajorTab* tab)
	{
		const TabState* state = FindTab(tab);
		ImGuiWindow* window = state ? ImGui::FindWindowByID(state->WindowID) : nullptr;
		if (!window)
			return;

		// Focusing a window selects its tab only if the window keeps the focus (it may be followed by focusing one of the tab's panels)
		if (window->DockNode && window->DockNode->TabBar)
			window->DockNode->TabBar->NextSelectedTabId = window->TabId;
		ImGui::FocusWindow(window);
	}

	EditorFrames::TabContentScope::TabContentScope(MajorTab* tab)
	{
		s_OpenTabContentScopes.push_back({ tab, GImGui->WindowsActiveCount, 0 });
	}

	EditorFrames::TabContentScope::~TabContentScope()
	{
		EG_CORE_ASSERT(!s_OpenTabContentScopes.empty(), "Unbalanced `TabContentScope`");
		if (s_OpenTabContentScopes.empty())
			return;

		TabContentRange range = s_OpenTabContentScopes.back();
		s_OpenTabContentScopes.pop_back();
		range.End = GImGui->WindowsActiveCount;
		if (range.Tab && range.End > range.Begin)
			s_TabContentRanges.push_back(range);
	}

	MajorTab* EditorFrames::GetFocusedTab()
	{
		if (MajorTab* focusedTab = FindFocusedTab())
			return focusedTab;

		return s_LastFocusedTab;
	}

	ImGuiViewport* EditorFrames::GetTabViewport(const MajorTab* tab)
	{
		const TabState* state = FindTab(tab);
		const ImGuiWindow* window = state ? ImGui::FindWindowByID(state->WindowID) : nullptr;
		return window ? window->Viewport : nullptr;
	}
}
