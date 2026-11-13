#pragma once

#include "../Frames/EditorFrames.h"

namespace Eagle
{
	class ProjectSettingsTab : public MajorTab
	{
	public:
		enum class Section
		{
			General, CollisionGroups, Navigation, UIScale, SplashScreen
		};

		ProjectSettingsTab() = default;
		~ProjectSettingsTab() override;

		ProjectSettingsTab(const ProjectSettingsTab&) = delete;
		ProjectSettingsTab& operator=(const ProjectSettingsTab&) = delete;

		const std::string& GetTabWindowName() const override { return m_WindowName; }
		std::string GetTabTitle() const override { return "Project Settings"; }
		void DrawTabMenus() override;
		void CloseTab() override { m_bCloseRequested = true; }

		// Returns false once it's closed
		bool OnImGuiRender();

	private:
		void DrawSectionList();
		void DrawSelectedSection();

		// They return whether a setting was changed
		bool DrawGeneralSection();
		bool DrawCollisionGroupsSection();
		bool DrawNavigationSection();
		bool DrawUIScaleSection();
		bool DrawSplashScreenSection();

	private:
		const std::string m_WindowName = "Project Settings###ProjectSettingsTab";
		Section m_SelectedSection = Section::General;
		std::unordered_map<uint32_t, std::string> m_PendingAgentTypeNames; // Agent type ID -> the name being typed
		bool m_bCloseRequested = false;
	};
}
