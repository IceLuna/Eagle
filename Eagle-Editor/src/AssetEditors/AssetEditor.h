#pragma once

#include "../EditorResources.h"
#include "../Frames/EditorFrames.h"

#include "Eagle/Math/Transform.h"

namespace Eagle
{
	class Asset;
	class Scene;
	class SceneRenderer;
	class AssetTextureCube;

	// Asset editors are major tabs. They open next to the level editor, and can be dragged out into their own windows.
	// An editor that isn't a major tab is a regular panel of the level editor
	class AssetEditor : public MajorTab
	{
	public:
		AssetEditor(bool bNeedRenderer = false, bool bNeedSkybox = true);

		virtual ~AssetEditor();

		const std::string& GetTabWindowName() const override;
		std::string GetTabTitle() const override;
		void DrawTabMenus() override;
		void CloseTab() override;

		virtual bool IsMajorTab() const { return true; }
		virtual bool UsesGuizmo() const { return false; }

		// pOpen - In case X button is clicked, this flag will be set to false.
		// pOpen - if nullptr set, windows will not have X button 
		virtual void OnImGuiRender(bool* pOpen) {}

		virtual void SetInFocus();

		virtual void OnEvent(Event& e);

		virtual const Ref<Asset> GetAsset() const = 0;

		// `DrawEditorWindow` must be called before it in the same frame
		void DrawViewport(bool bForceAnimUpdate = false);

		// Called right before ImGui::End() of viewport to allow custom widgets
		virtual void OnViewportEnd() {}

		bool DrawGuizmo(Transform& transform, bool bEnabled, bool bWorld = true);
		int GetGuizmoType() const { return m_GuizmoType; }

		void DrawOGuizmo();

		void SetSimulationEnabled(bool bEnabled);

		const Ref<Scene>& GetCurrentScene() const { return m_CurrentScene; }

	protected:
		// --- Editor window ---
		// An asset editor that consists of several panels (viewport, details, etc) has a single host window. It's the editor's major tab,
		// so it's docked into a tab well of the editor's frames (see `EditorFrames`), next to the level editor and other asset editors.
		// Its panels live in the host window's own dockspace, and they can only be docked there.
		// So the user can still rearrange them within the editor, but they can't get stuck inside unrelated panels, and unrelated panels can't be docked into the editor.

		// Submits the host window and its dockspace. Must be called every frame, before any `BeginPanel` (and before `DrawViewport`).
		bool DrawEditorWindow(bool* pOpen);

		// Editors that submit their own window (instead of `DrawEditorWindow`) must call it right before their `ImGui::Begin()`,
		// and set the name of their window with `SetTabWindowName()`
		void PrepareTabWindow();
		void SetTabWindowName(const std::string& windowName) { m_EditorWindowName = windowName; }

		// Same as `ImGui::Begin()`, but for a panel of this editor.
		// @bNoPadding. The content can take the whole pane (for example, a viewport)
		bool BeginPanel(const std::string& panelName, ImGuiWindowFlags flags = 0, bool bNoPadding = false);

		// Called when the editor's dockspace doesn't have a layout yet (the editor is opened for the first time).
		// @dockspaceID. The root node. Dock panes with `ImGui::DockBuilderDockWindow()`.
		// By default, the viewport panel is on the left, and the details panel is on the right
		virtual void BuildDefaultLayout(ImGuiID dockspaceID);

		static void HideTabBar(ImGuiID dockNodeID);

		// Makes `dockNodeID` (a leaf node of the dockspace) the central node of the dockspace. 0 - no central node.
		// The central node keeps its space even when it's empty, while any other node is removed when it becomes empty
		// (for example, its panel is undocked), and its neighbours take over the space. The central node also takes the remaining space
		// when the editor is resized, while other nodes keep their size.
		// `DrawEditorWindow` keeps the viewport panel's node as the central node, so this is only needed for editors without a viewport panel
		static void SetCentralNode(ImGuiID dockspaceID, ImGuiID dockNodeID);

		// Valid after the first `DrawEditorWindow` call
		const std::string& GetDetailsPanelName() const { return m_DetailsPanelName; }
		const std::string& GetViewportPanelName() const { return m_ViewportPanelName; }
		const ImGuiWindowClass& GetPanelWindowClass() const { return m_PanelWindowClass; }

		static std::string GetPanelName(const Ref<Asset>& asset, std::string_view panel);

		void HandleCameraFocus();
		glm::ivec2 GetMousePosWithinViewport() const;

		static std::string GetAssetWindowName(const Ref<Asset>& asset, std::string_view ending = "");
		static ImVec2 GetDefaultWindowSize() { return ImVec2(920.f, 760.f); }

		// It's here to avoid duplicating help msg all over the code base
		static bool DrawAlwaysIncludeInGameBuilds(const Ref<Asset>& asset);
		static bool DrawAlwaysIncludeInGameBuildsCheckbox(const Ref<Asset>& asset);

	protected:
		ImVec2 m_FirstUseSizeRatio = ImVec2(0.7f, 0.75f);
		float m_FirstUseViewportRatio = 0.65f;

	private:
		void AddSkybox();
		bool OnKeyPressed(KeyPressedEvent& e);
		void InitEditorWindowNames() const;
		void UpdateCentralNode();
		void DrawViewportToolbar();

	private:
		Ref<Scene> m_Scene;
		Ref<Scene> m_SimulationScene;
		Ref<Scene> m_CurrentScene;

		// Initialized on the first use, since they depend on the asset
		mutable std::string m_EditorWindowName; // Host window
		mutable std::string m_DetailsPanelName;
		mutable std::string m_ViewportPanelName;
		mutable ImGuiWindowClass m_PanelWindowClass;
		ImGuiID m_DockSpaceID = 0;
		bool m_bResetLayout = false;

	protected:
		Ref<SceneRenderer> m_Renderer;
		Ref<AssetTextureCube> m_Skybox;
		glm::vec2 m_ViewportBounds[2] = { glm::vec2(), glm::vec2() };
		int m_GuizmoType = 7; // TRANSLATE;

		bool bViewportVisible = false;
		bool bViewportFocused = false;
		bool bViewportHovered = false;
		bool bViewportToolbarHovered = false; // The mouse is over the viewport's toolbar (or one of its popups is open), so the viewport ignores clicks
		bool bUsingImGuizmoOrHovered = false;
	};
}
