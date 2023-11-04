#pragma once

#include "Eagle.h"
#include "Panels/SceneHierarchyPanel.h"
#include "Panels/ContentBrowserPanel.h"
#include "Panels/ConsolePanel.h"
#include "EditorSerializer.h"
#include <filesystem>

namespace Eagle
{
	class Sound2D;

	enum class EditorState
	{
		Edit, Play, Pause, SimulatePhysics
	};

	class EditorLayer : public Layer
	{
	public:
		EditorLayer();

		void OnAttach() override;
		void OnDetach() override;
		void OnUpdate(Timestep ts) override;
		void OnEvent(Event& e) override;
		void OnImGuiRender() override;

		void OpenScene(const Path& filepath);
		void SaveScene();

		EditorState GetEditorState() const { return m_EditorState; }
		bool IsViewportFocused() const { return m_ViewportFocused; }
		bool IsViewportHovered() const { return m_ViewportHovered; }

	private:
		bool OnKeyPressed(KeyPressedEvent& e);
		
		void ReloadScriptsIfNecessary();
		void HandleResize();
		void HandleEntitySelection();

		void NewScene();
		void SaveSceneAs();

		void UpdateEditorTitle(const std::filesystem::path& scenePath);

		void OnDeserialized(const glm::vec2& windowSize, const glm::vec2& windowPos, const SceneRendererSettings& settings, bool bWindowMaximized, bool bVSync);
		void SetCurrentScene(const Ref<Scene>& scene);

		void UpdateGuizmo();
		void DrawMenuBar();
		void DrawSceneSettings();
		void DrawSettings();
		void DrawEditorPreferences();
		void DrawStats();
		void DrawViewport();
		void DrawSimulatePanel();

		void Submit(const std::function<void()>& func);

		void PlayScene();
		void StopPlayingScene();
		void HandleOnSimulationButton();

		void BeginDocking();
		void EndDocking();

		const Ref<Image>& GetRequiredGBufferImage(const Ref<SceneRenderer>& renderer, const GBuffer& gbuffer);

	private:
		enum class GBufferVisualizingType
		{
			Final, Albedo, Emissive, SSAO, GTAO, Motion
		};
		void SetVisualizingBufferType(GBufferVisualizingType value);

		SceneHierarchyPanel m_SceneHierarchyPanel;
		ContentBrowserPanel m_ContentBrowserPanel;
		ConsolePanel m_ConsolePanel;

		std::vector<std::function<void()>> m_DeferredCalls;

		Ref<Scene> m_EditorScene;
		Ref<Scene> m_SimulationScene;
		Ref<Scene> m_CurrentScene;
		const Ref<Image>* m_ViewportImage = nullptr; // A pointer just not to copy Ref
		GBufferVisualizingType m_VisualizingGBufferType = GBufferVisualizingType::Final;
		int m_SelectedBufferIndex = 0;

		Ref<Texture2D> m_PlayButtonIcon;
		Ref<Texture2D> m_StopButtonIcon;

		Ref<Sound2D> m_PlaySound;

		std::filesystem::path m_OpenedScenePath;
		std::string m_WindowTitle;

		glm::vec3 m_SnappingValues = glm::vec3(0.1f, 10.f, 0.1f);
		glm::vec2 m_CurrentViewportSize = {1.f, 1.f};
		glm::vec2 m_NewViewportSize = {1.f, 1.f};
		glm::vec2 m_ViewportBounds[2] = {glm::vec2(), glm::vec2()};
		EditorSerializer m_EditorSerializer;
		Window& m_Window;
		Timestep m_Ts;

		GUID m_OpenedSceneCallbackID;

		int m_GuizmoType = 7; // TRANSLATE;
		ImGuiLayer::Style m_EditorStyle = ImGuiLayer::Style::Default;
		EditorState m_EditorState = EditorState::Edit;
		
		ImGuiWindowClass m_SimulatePanelSettings;

		struct BeforeSimulationData
		{
			SceneRendererSettings RendererSettings{};
			Ref<TextureCube> Cubemap;
			SkySettings Sky{};
			float CubemapIntensity = 1.f;
			bool bSkyAsBackground = false;
			bool bSkyboxEnabled = false;
		} m_BeforeSimulationData;

		bool m_ViewportHovered = false;
		bool m_ViewportFocused = false;
		bool m_ViewportHidden = false;
		bool m_bFullScreen = false;

		friend class EditorSerializer;
	};
}
