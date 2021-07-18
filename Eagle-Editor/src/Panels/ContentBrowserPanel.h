#pragma once

#include <filesystem>
#include "Eagle/Renderer/Texture.h"

namespace Eagle
{
	class EditorLayer;
	class Event;

	class ContentBrowserPanel
	{
	public:
		ContentBrowserPanel(EditorLayer& editorLayer);

		void OnImGuiRender();

		void OnEvent(Event& e);

	private:
		void DrawContent(const std::vector<std::filesystem::path>& directories, const std::vector<std::filesystem::path>& files, bool bHintFullPath = false);
		void DrawPathHistory();

		void GetSearchingContent(const std::string& search, std::vector<std::filesystem::path>& outFiles);

		void DrawPopupMenu(const std::filesystem::path& path, int timesCalledForASinglePath = 0);

		void GoBack();
		void GoForward();

		void OnDirectoryOpened(const std::filesystem::path& previousPath);

	private:
		Ref<Texture> textureToView;
		std::filesystem::path m_CurrentDirectory;
		std::filesystem::path m_SelectedFile;
		std::filesystem::path pathOfSceneToOpen;
		EditorLayer& m_EditorLayer;
		std::vector<std::filesystem::path> m_Directories;
		std::vector<std::filesystem::path> m_Files;
		std::vector<std::filesystem::path> m_SearchFiles;
		std::vector<std::filesystem::path> m_BackHistory;
		std::vector<std::filesystem::path> m_ForwardHistory;
		bool bShowSaveScenePopup = false;
		bool bShowTextureView = false;
		bool bContentBrowserHovered = false;
	};
}
