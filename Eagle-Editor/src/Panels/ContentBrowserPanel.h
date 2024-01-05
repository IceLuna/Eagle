#pragma once

#include <filesystem>
#include "Eagle/Renderer/VidWrappers/Texture.h"
#include "Eagle/Utils/Utils.h"
#include "Eagle/Asset/Asset.h"

namespace Eagle
{
	class EditorLayer;
	class Event;
	class Asset;
	class Sound2D;

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
		
		const char* GetDragCellTag(AssetType format)
		{
			switch (format)
			{
				case AssetType::Texture2D:
					return "TEXTURE_CELL";
				case AssetType::TextureCube:
					return "TEXTURE_CUBE_CELL";
				case AssetType::Mesh:
					return "MESH_CELL";
				case AssetType::Audio:
					return "SOUND_CELL";
				case AssetType::Font:
					return "FONT_CELL";
				case AssetType::Material:
					return "MATERIAL_CELL";
				case AssetType::PhysicsMaterial:
					return "PHYSICS_MATERIAL_CELL";
				case AssetType::SoundGroup:
					return "SOUND_GROUP_CELL";
				default:
					return "UNKNOWN";
			}
		}

		void SelectFile(const Path& path);
		Ref<Texture2D>& GetFileIconTexture(AssetType fileFormat);

	private:
		static constexpr int searchBufferSize = 512;
		static char searchBuffer[searchBufferSize];
		
		Ref<Asset> m_TextureToView;
		Ref<Asset> m_MaterialToView;
		Ref<Asset> m_PhysicsMaterialToView;
		Ref<Asset> m_AudioToView;
		Ref<Asset> m_SoundGroupToView;

		Ref<Texture2D> m_MeshIcon;
		Ref<Texture2D> m_TextureIcon;
		Ref<Texture2D> m_SceneIcon;
		Ref<Texture2D> m_SoundIcon;
		Ref<Texture2D> m_UnknownIcon;
		Ref<Texture2D> m_FolderIcon;
		Ref<Texture2D> m_FontIcon;
		Path m_CurrentDirectory;
		Path m_SelectedFile;
		Path m_PathOfSceneToOpen;
		EditorLayer& m_EditorLayer;
		std::vector<Path> m_Directories;
		std::vector<Path> m_Files;
		std::vector<Path> m_SearchFiles;
		std::vector<Path> m_BackHistory;
		std::vector<Path> m_ForwardHistory;
		Ref<Sound2D> m_SoundToPlay;
		bool m_ShowSaveScenePopup = false;
		bool m_ShowTextureView = false;
		bool m_ShowMaterialEditor = false;
		bool m_ShowPhysicsMaterialEditor = false;
		bool m_ShowAudioEditor = false;
		bool m_ShowSoundGroupEditor = false;
		bool m_ContentBrowserHovered = false;
	};
}
