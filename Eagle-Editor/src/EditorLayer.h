#pragma once

#include "Eagle.h"
#include "Panels/SceneHierarchyPanel.h"
#include <filesystem>

namespace Eagle
{
	class EditorLayer : public Layer
	{
	public:
		EditorLayer();

		void OnAttach() override;
		void OnDetach() override;
		void OnUpdate(Timestep ts) override;
		void OnEvent(Event& e) override;
		void OnImGuiRender() override;

	private:
		bool OnKeyPressed(KeyPressedEvent& e);

		void NewScene();
		void OpenScene();
		void SaveScene();
		void SaveSceneAs();

	private:
		SceneHierarchyPanel m_SceneHierarchyPanel;

		Ref<Scene> m_ActiveScene;

		Ref<Framebuffer> m_Framebuffer;

		std::filesystem::path m_OpenedScene;
		std::string m_WindowTitle;

		glm::vec3 m_SnappingValues = glm::vec3(0.1f, 10.f, 0.1f);
		glm::vec2 m_CurrentViewportSize = {1.f, 1.f};
		glm::vec2 m_NewViewportSize = {1.f, 1.f};
		glm::vec2 m_ViewportBounds[2] = {glm::vec2(), glm::vec2()};
		Timestep m_Ts;

		int m_GuizmoType = -1;
		
		bool m_InvertColor = false;
		bool m_ViewportHovered = false;
		bool m_ViewportFocused = false;
	};
}
