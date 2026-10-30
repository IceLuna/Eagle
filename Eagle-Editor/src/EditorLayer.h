#pragma once

#include "Eagle.h"
#include "Panels/SceneHierarchyPanel.h"
#include "Panels/ContentBrowserPanel.h"
#include "Panels/ConsolePanel.h"
#include "Panels/AssetUsagePanel.h"
#include "Frames/EditorFrames.h"
#include "Widgets/ViewportToolbar.h"
#include "EditorSerializer.h"
#include <filesystem>

namespace Eagle
{
	class Sound2D;
	class SceneRenderer;

	enum class EditorState
	{
		Edit, Play, Pause, SimulatePhysics
	};

	enum class DirtyAssetsReason
	{
		None, ProjectClose, ProjectBuild
	};

	// The level editor. It's the main tab of the editor. It's always in the main window, and asset editors are opened next to it
	class EditorLayer : public Layer, public MajorTab
	{
	public:
		EditorLayer();

		void OnAttach() override;
		void OnDetach() override;
		void OnUpdate(Timestep ts) override;
		void OnEvent(Event& e) override;
		void OnImGuiRender() override;

		const std::string& GetTabWindowName() const override { return m_LevelTabWindowName; }
		std::string GetTabTitle() const override { return m_LevelTabTitle; }
		void DrawTabMenus() override { DrawMenus(); }
		bool IsPinnedToMainFrame() const override { return true; }

		void OpenScene(const Ref<AssetScene>& sceneAsset);
		bool SaveScene();

		const Ref<AssetScene>& GetOpenedSceneAsset() const { return m_OpenedSceneAsset; }
		EditorState GetEditorState() const { return m_EditorState; }
		bool IsViewportFocused() const { return m_ViewportFocused; }
		bool IsViewportHovered() const { return m_ViewportHovered; }

		SnapSettings& GetSnapSettings() { return m_SnapSettings; }
		const SnapSettings& GetSnapSettings() const { return m_SnapSettings; }
		bool IsDrawingAllTextUIBounds() const { return bDrawAllTextUIBounds; }

		void ShowShortcutsWindow() { m_bShowShortcuts = true; }
		void ShowAboutWindow() { m_bShowAbout = true; }

		// Oh boy... I guess it's better than passing around `EditorLayer`
		static EditorLayer* Get();

	private:
		bool OnKeyPressed(KeyPressedEvent& e);
		bool OnWindowClose(WindowCloseEvent& e);
		
		void CheckAppAssembly();
		void LoadAppAssembly();
		void ReloadScriptsIfNecessary();
		void HandleResize();
		bool HandleEntitySelection(MouseButtonPressedEvent& e);
		void ToggleWindowFullscreenState();

		void HandleAssetDragDrop();
		void SpawnEntityAtDepth(Entity entity, glm::vec2 uv, float depth);

		glm::ivec2 GetMousePosWithinViewport() const;

		void NewScene();
		bool SaveSceneAs();

		void UpdateEditorTitle(const Ref<AssetScene>& scene);

		void OnDeserialized(const EditorCamera& camera, const WindowPlacement& windowPlacement, const SceneRendererSettings& settings, bool bVSync, bool bRenderOnlyWhenFocused,
			bool bDrawNavMesh, bool bDrawMeshAABBs, bool bDrawAxisGuizmo, bool bForceDrawGuizmo, Key stopSimulationKey, bool bUpdateAnimationsInEditor, int guizmoMode);
		void SetCurrentScene(const Ref<Scene>& scene);

		void UpdateSceneEditorCamera(const Ref<Scene>& scene, bool bUpdateTransform = false);

		void UpdateGuizmo();
		void UpdateSelectionOutline();
		void UpdateTextUIBounds();
		void DrawLevelEditorTab();
		void DrawMenus();
		void DrawDebugWindows();
		void DrawViewModeMenuItems();
		void DrawShowFlagsMenuItems();
		void DrawSceneSettings();
		void DrawRendererSettings();
		void DrawProjectSettings();
		void DrawGameSplashScreenSettings(bool& bChanged);
		void PreviewSplashScreen();
		void DrawEditorPreferences();
		void DrawStats();
		void DrawViewport();
		void DrawSimulatePanel();
		UI::ButtonType DrawDirtyAssetsPopup(std::string_view yesButtonText, std::string_view noButtonText);
		void HandleDirtyAssetsPopup();
		
		void SaveDirtyAssets();
		void PrepareDirtyAssets(DirtyAssetsReason reason);

		// Closes the editor
		void OpenProjectSelector();

		void Submit(const std::function<void()>& func);

		void PlayScene();
		void StopPlayingScene();
		void HandleOnSimulationButton();
		void HandleCloseRequest(bool bCloseEngine);
		void ProcessCloseRequest();
		bool CanCloseProject() const;

		Ref<Image> GetRequiredGBufferImage(const Ref<SceneRenderer>& renderer, const GBuffer& gbuffer);

	private:
		enum class GBufferVisualizingType
		{
			Final, Albedo, Emissive, ScreenSpaceShadows, AO, Motion
		};
		void SetVisualizingBufferType(GBufferVisualizingType value);
		bool IsVisualizingBufferTypeAvailable(GBufferVisualizingType value) const;
		static const char* GetVisualizingBufferTypeName(GBufferVisualizingType value);

		Ref<ImGuiLayer> m_ImGuiLayer = nullptr;

		SceneHierarchyPanel m_SceneHierarchyPanel;
		ContentBrowserPanel m_ContentBrowserPanel;
		ConsolePanel m_ConsolePanel;
		AssetUsagePanel m_AssetUsagePanel;

		std::vector<std::function<void()>> m_DeferredCalls;

		Ref<Scene> m_EditorScene;
		Ref<Scene> m_SimulationScene;
		Ref<Scene> m_CurrentScene;
		Ref<Image> m_ViewportImage;
		GBufferVisualizingType m_VisualizingGBufferType = GBufferVisualizingType::Final;

		Ref<Texture2D> m_PlayButtonIcon;
		Ref<Texture2D> m_StopButtonIcon;
		Ref<Texture2D> m_EditorIcon;
		double m_SplashScreenPreviewEndTime = 0.0; // Not 0 while the splash screen is previewed (Project Settings)

		Ref<Sound2D> m_PlaySound;

		Ref<AssetScene> m_OpenedSceneAsset;
		std::string m_WindowTitle;
		std::string m_LevelTabWindowName; // The name changes with the opened scene, the ID stays the same
		std::string m_LevelTabTitle;

		EditorCamera m_Camera;
		SnapSettings m_SnapSettings;
		glm::vec2 m_CurrentViewportSize = {1.f, 1.f};
		glm::vec2 m_NewViewportSize = {1.f, 1.f};
		glm::vec2 m_ViewportBounds[2] = {glm::vec2(), glm::vec2()};
		Window& m_Window;
		Timestep m_Ts;

		GUID m_OpenedSceneCallbackID;

		int m_GuizmoType = 7; // TRANSLATE;
		int m_GuizmoMode = 1; // ImGuizmo::WORLD;
		ImGuiLayer::Style m_EditorStyle = ImGuiLayer::Style::Default;
		EditorState m_EditorState = EditorState::Edit;
		bool bRenderOnlyWhenFocused = true;
		bool bUpdateAnimationsInEditor = true;
		bool bDrawNavMesh = true;
		bool bDrawMeshAABBs = false;
		bool bDrawAxisGuizmo = true;
		bool bForceDrawGuizmo = false;
		bool bDrawSelectionOutline = true;
		bool bDrawAllTextUIBounds = false;
		Key m_StopSimulationKey = Key::Escape;
		OutlineSettings m_SelectionOutline{};

		struct AppliedSelectionOutline
		{
			std::weak_ptr<Scene> TargetScene;
			std::vector<GUID> EntityGUIDs;
			OutlineSettings Settings{};
		} m_AppliedSelectionOutline;
		
		ImGuiWindowClass m_SimulatePanelSettings;

		struct BeforeSimulationData
		{
			SceneRendererSettings RendererSettings{};
			Ref<AssetTextureCube> Cubemap;
			SkySettings Sky{};
			float CubemapIntensity = 1.f;
			bool bSkyAsBackground = false;
			bool bSkyboxEnabled = false;
			bool bRenderSkybox = false;
		} m_BeforeSimulationData;

		glm::vec2 m_WindowSizeBeforeFS;
		glm::vec2 m_WindowPosBeforeFS;

		bool m_WindowFocused = true;
		bool m_ViewportHovered = false;
		bool m_ViewportFocused = false;
		bool m_ViewportHidden = false;
		bool m_bFullScreen = false;
		bool m_ShowSaveScenePopupForNewScene = false;
		bool m_bDrawEditorMisc = true;
		bool m_bFirstContentBrowserRender = true;
		bool m_bUsingImGuizmoOrHovered = false;
		bool m_bSkipGuizmoThisFrame = false;
		bool m_bViewportToolbarHovered = false; // The mouse is over the viewport's toolbar (or one of its popups is open), so the viewport ignores clicks
		
		bool m_bShowShortcuts = false;
		bool m_bShowAbout = false;
		bool m_bShowGPUTimings = false;
		bool m_bShowCPUTimings = false;
		bool m_bShowGPUMemoryUsage = false;

		bool m_CloseEngineRequested = false;
		bool m_ShowDirtyAssetMessage = false;
		std::vector<Ref<Asset>> m_DirtyAssets;
		std::vector<bool> m_DirtyAssetsChecked;
		DirtyAssetsReason m_DirtyAssetsReason = DirtyAssetsReason::None;

		friend class EditorSerializer;
	};
}
