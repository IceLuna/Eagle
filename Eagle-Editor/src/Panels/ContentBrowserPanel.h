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
		void DrawContent(const std::vector<Path>& directories, const std::vector<Path>& files, bool bHintFullPath = false);
		void DrawPathHistory();

		void GetSearchingContent(const std::string& search, std::vector<Path>& outFiles);

		void DrawPopupMenu(const Path& path, int timesCalledForASinglePath = 0);

		void GoBack();
		void GoForward();

		void OnDirectoryOpened(const Path& previousPath);

		bool IsDraggableFileFormat(Utils::FileFormat format) const { return format == Utils::FileFormat::TEXTURE || format == Utils::FileFormat::TEXTURE_CUBE || format == Utils::FileFormat::MESH || format == Utils::FileFormat::SOUND; }
		
		const char* GetDragCellTag(Utils::FileFormat format)
		{
			switch (format)
			{
				case Utils::FileFormat::TEXTURE:
					return "TEXTURE_CELL";
				case Utils::FileFormat::TEXTURE_CUBE:
					return "TEXTURE_CUBE_CELL";
				case Utils::FileFormat::MESH:
					return "MESH_CELL";
				case Utils::FileFormat::SOUND:
					return "SOUND_CELL";
				default:
					return "UNKNOWN";
			}
		}

		void SelectFile(const Path& path);
		Ref<Texture2D>& GetFileIconTexture(const Utils::FileFormat& fileFormat);

	private:
		static constexpr int searchBufferSize = 512;
		static char searchBuffer[searchBufferSize];
		Ref<Texture> textureToView;
		Path m_CurrentDirectory;
		Path m_SelectedFile;
		Path m_PathOfSceneToOpen;
		EditorLayer& m_EditorLayer;
		std::vector<Path> m_Directories;
		std::vector<Path> m_Files;
		std::vector<Path> m_SearchFiles;
		std::vector<Path> m_BackHistory;
		std::vector<Path> m_ForwardHistory;
		bool m_ShowSaveScenePopup = false;
		bool m_ShowTextureView = false;
		bool m_ContentBrowserHovered = false;
	};
}
