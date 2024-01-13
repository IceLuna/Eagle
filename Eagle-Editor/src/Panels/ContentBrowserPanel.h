#pragma once

#include "EntityPropertiesPanel.h"

#include "Eagle/Renderer/VidWrappers/Texture.h"
#include "Eagle/Utils/Utils.h"
#include "Eagle/Asset/Asset.h"

#include <filesystem>

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
				case AssetType::Entity:
					return "ENTITY_CELL";
				default:
					return "UNKNOWN";
			}
		}

		void SelectFile(const Path& path);
		Ref<Texture2D>& GetFileIconTexture(AssetType fileFormat);

	private:
		static constexpr int searchBufferSize = 512;
		static char searchBuffer[searchBufferSize];

		EntityPropertiesPanel m_EntityProperties;
		
		Ref<AssetTexture2D> m_Texture2DToView;
		Ref<AssetTextureCube> m_TextureCubeToView;
		Ref<AssetMaterial> m_MaterialToView;
		Ref<AssetPhysicsMaterial> m_PhysicsMaterialToView;
		Ref<AssetAudio> m_AudioToView;
		Ref<AssetSoundGroup> m_SoundGroupToView;
		Ref<AssetEntity> m_EntityToView;
		Ref<AssetScene> m_SceneToOpen;

		Ref<Texture2D> m_MeshIcon;
		Ref<Texture2D> m_TextureIcon;
		Ref<Texture2D> m_SceneIcon;
		Ref<Texture2D> m_SoundIcon;
		Ref<Texture2D> m_UnknownIcon;
		Ref<Texture2D> m_FolderIcon;
		Ref<Texture2D> m_FontIcon;
		Ref<Texture2D> m_AsteriskIcon;
		Path m_ProjectPath;
		Path m_ContentPath;
		Path m_CurrentDirectory;
		Path m_SelectedFile;
		EditorLayer& m_EditorLayer;
		std::vector<Path> m_Directories;
		std::vector<Path> m_Files;
		std::vector<Path> m_SearchFiles;
		std::vector<Path> m_BackHistory;
		std::vector<Path> m_ForwardHistory;

		bool m_ShowSaveScenePopup = false;
		bool m_ShowTexture2DView = false;
		bool m_ShowTextureCubeView = false;
		bool m_ShowMaterialEditor = false;
		bool m_ShowPhysicsMaterialEditor = false;
		bool m_ShowAudioEditor = false;
		bool m_ShowSoundGroupEditor = false;
		bool m_ShowEntityEditor = false;

		bool m_ContentBrowserHovered = false;
	};
}
