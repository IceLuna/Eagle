#pragma once

#include "Eagle.h"

namespace Eagle
{
	class ProjectLayer : public Layer
	{
	public:
		ProjectLayer() : Layer("Project Layer") {}

		void OnAttach() override;
		void OnDetach() override;
		void OnUpdate(Timestep ts) override;
		void OnEvent(Event& e) override;
		void OnImGuiRender() override;

	private:
		void OpenEditor();
		UI::ButtonType DrawCreateProjectPopup(const std::string_view title, const std::string_view hint);
		void DrawCreateOpenButtons();
		void DrawRecentProjects();

		void AddRecentProject(const Path& path);

	private:
		bool bWasVSync = false;

		ImGuiWindowClass m_NoTabBar;

		Ref<Texture2D> m_OpenProjectIcon;
		Ref<Texture2D> m_CreateProjectIcon;
		Ref<Texture2D> m_UserProjectIcon;
		std::vector<Path> m_RecentProjects; // Absolute path to recently opened projects
		Path m_SelectedPath;

		std::string m_NewProjectName;
		Path m_NewProjectPath;
		bool m_DrawCreateProjectPopup = false;
	};
}
