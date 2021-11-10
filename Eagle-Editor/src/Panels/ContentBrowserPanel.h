#pragma once

#include <filesystem>
#include "Eagle/Renderer/Texture.h"
#include "Eagle/Utils/Utils.h"

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

		bool IsDraggableFileFormat(Utils::FileFormat format) const { return format == Utils::FileFormat::TEXTURE || format == Utils::FileFormat::MESH || format == Utils::FileFormat::SOUND; }
		
		const char* GetDragCellTag(Utils::FileFormat format)
		{
			switch (format)
			{
				case Utils::FileFormat::TEXTURE:
					return "TEXTURE_CELL";
				case Utils::FileFormat::MESH:
					return "MESH_CELL";
				case Utils::FileFormat::SOUND:
					return "SOUND_CELL";
				default:
					return "UNKNOWN";
			}
		}

		void SelectFile(const std::filesystem::path& path);
		uint32_t GetFileIconRendererID(const Utils::FileFormat& fileFormat);

	private:
		static constexpr int searchBufferSize = 512;
		static char searchBuffer[searchBufferSize];
		Ref<Texture> textureToView;
		std::filesystem::path m_CurrentDirectory;
		std::filesystem::path m_SelectedFile;
		std::filesystem::path m_PathOfSceneToOpen;
		EditorLayer& m_EditorLayer;
		std::vector<std::filesystem::path> m_Directories;
		std::vector<std::filesystem::path> m_Files;
		std::vector<std::filesystem::path> m_SearchFiles;
		std::vector<std::filesystem::path> m_BackHistory;
		std::vector<std::filesystem::path> m_ForwardHistory;
		bool m_ShowSaveScenePopup = false;
		bool m_ShowTextureView = false;
		bool m_ContentBrowserHovered = false;
	};
}
