#pragma once

#include <filesystem>
#include "Eagle/Renderer/Texture.h"

namespace Eagle
{
	class EditorLayer;

	class ContentBrowserPanel
	{
	public:
		ContentBrowserPanel(EditorLayer& editorLayer);

		void OnImGuiRender();

	private:
		void DrawContent(const std::vector<std::filesystem::path>& directories, const std::vector<std::filesystem::path>& files, bool bHintFullPath = false);
		void DrawPathHistory();

		void GetSearchingContent(const std::string& search, std::vector<std::filesystem::path>& outFiles);

	private:
		Ref<Texture> textureToView;
		std::filesystem::path m_CurrentDirectory;
		std::filesystem::path m_SelectedFile;
		std::filesystem::path pathOfSceneToOpen;
		EditorLayer& m_EditorLayer;
		std::vector<std::filesystem::path> m_Directories;
		std::vector<std::filesystem::path> m_Files;
		std::vector<std::filesystem::path> m_SearchFiles;
		bool bShowSaveScenePopup = false;
		bool bShowTextureView = false;
	};
}
