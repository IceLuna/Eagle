#pragma once

#include <filesystem>

namespace Eagle
{
	class EditorLayer;

	class ContentBrowserPanel
	{
	public:
		ContentBrowserPanel(EditorLayer& editorLayer);

		void OnImGuiRender();
	private:
		std::filesystem::path m_CurrentDirectory;
		EditorLayer& m_EditorLayer;
		std::vector<std::filesystem::path> m_Directories;
		std::vector<std::filesystem::path> m_Files;
	};
}
