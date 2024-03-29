#pragma once

#include "Eagle.h"
#include "Panels/SceneHierarchyPanel.h"
#include "Panels/ContentBrowserPanel.h"
#include "EditorSerializer.h"

#include <filesystem>

namespace Eagle
{
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

		void OpenScene(const std::filesystem::path& filepath);
		void SaveScene();

		EditorState GetEditorState() const { return m_EditorState; }

	private:
		bool OnKeyPressed(KeyPressedEvent& e);

		void NewScene();
		void OpenScene();
		void SaveSceneAs();

		void UpdateEditorTitle(const std::filesystem::path& scenePath);

		bool CanRenderSkybox() const;
		void OnDeserialized(const glm::vec2& windowSize, const glm::vec2& windowPos, bool bWindowMaximized);
		void SetCurrentScene(const Ref<Scene>& scene);

		void UpdateGuizmo();

	private:
		std::array<Ref<Texture>, 6> m_CubemapFaces;
		SceneHierarchyPanel m_SceneHierarchyPanel;
		ContentBrowserPanel m_ContentBrowserPanel;

		Ref<Scene> m_EditorScene;
		Ref<Scene> m_SimulationScene;
		Ref<Scene> m_CurrentScene;

		std::filesystem::path m_OpenedScenePath;
		std::string m_WindowTitle;

		glm::vec3 m_SnappingValues = glm::vec3(0.1f, 10.f, 0.1f);
		glm::vec2 m_CurrentViewportSize = {1.f, 1.f};
		glm::vec2 m_NewViewportSize = {1.f, 1.f};
		glm::vec2 m_ViewportBounds[2] = {glm::vec2(), glm::vec2()};
		EditorSerializer m_EditorSerializer;
		Window& m_Window;
		Timestep m_Ts;

		int m_GuizmoType = -1;
		int m_EditorStyleIdx = 0;
		EditorState m_EditorState = EditorState::Edit;

		bool m_VSync = false;
		bool m_ViewportHovered = false;
		bool m_ViewportFocused = false;
		bool m_ViewportHidden = false;
		bool m_EnableSkybox = false;

		friend class EditorSerializer;
	};
}
