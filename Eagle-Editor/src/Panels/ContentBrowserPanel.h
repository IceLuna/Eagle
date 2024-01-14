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

		void OnCopyAsset(const Path& path);
		void OnCutAsset(const Path& path);
		void OnRenameAsset(const Ref<Asset>& asset);
		void OnDeleteAsset(const Path& path);
		void OnPasteAsset();
		void OnSaveAsset(const Ref<Asset>& asset);

		void OnDirectoryOpened(const Path& previousPath);

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

		Path m_AssetToDelete;
		Ref<Asset> m_AssetToRename;
		Path m_CopiedPath;
		bool m_bCopy = false; // If true, it's copy, else - cut

		bool m_ShowSaveScenePopup = false;
		bool m_ShowTexture2DView = false;
		bool m_ShowTextureCubeView = false;
		bool m_ShowMaterialEditor = false;
		bool m_ShowPhysicsMaterialEditor = false;
		bool m_ShowAudioEditor = false;
		bool m_ShowSoundGroupEditor = false;
		bool m_ShowEntityEditor = false;

		bool m_bShowInputName = false;
		bool m_ShowDeleteConfirmation = false;
		std::string m_DeleteConfirmationMessage;

		enum class InputNameState
		{
			None, NewFolder, AssetRename
		} m_InputState = InputNameState::None;

		bool m_ContentBrowserHovered = false;
	};
}
