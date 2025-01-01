#pragma once

#include "EntityPropertiesPanel.h"
#include "AssetImporterPanel.h"
#include "Eagle/UI/Editors/AnimationGraphEditor.h"

#include "Eagle/Renderer/VidWrappers/Texture.h"
#include "Eagle/Utils/Utils.h"
#include "Eagle/Asset/Asset.h"

#include "../AssetEditors/AssetEditor.h"

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
		void HandleAddPanel();
		void HandleAssetEditors();
		void RefreshContentInfo();
		bool HandleImport();

		void GetSearchingContent(const std::string& search, std::vector<Path>& outFiles);

		void DrawPopupMenu(const Path& path, int timesCalledForASinglePath = 0);

		void GoBack();
		void GoForward();

		void OnCopyAsset(const Path& path);
		void OnCutAsset(const Path& path);
		void OnRenameAsset(const Ref<Asset>& asset);
		void OnDeleteAsset(const Ref<Asset>& asset);
		void OnPasteAsset();
		void OnSaveAsset(const Ref<Asset>& asset);

		void OnDirectoryOpened(const Path& previousPath);

		void SelectFile(const Path& path);
		Ref<Texture2D>& GetFileIconTexture(AssetType fileFormat);

	private:
		template<typename EditorType, typename AssetType, class... Args>
		void AddAssetEditor(const Ref<Asset>& asset, Args&&... args)
		{
			auto it = m_AssetEditors.find(asset);
			if (it == m_AssetEditors.end())
				m_AssetEditors[asset] = MakeRef<EditorType>(Cast<AssetType>(asset), std::forward<Args>(args)...);
			else
				it->second->SetInFocus();
		}

	private:
		static constexpr int searchBufferSize = 512;
		static char searchBuffer[searchBufferSize];

		Ref<AssetScene> m_SceneToOpen;
		
		std::unordered_map<Ref<Asset>, Ref<AssetEditor>> m_AssetEditors;

		Ref<Texture2D> m_TextureIcon;
		Ref<Texture2D> m_MeshIcon;
		Ref<Texture2D> m_AudioIcon;
		Ref<Texture2D> m_SoundGroupIcon;
		Ref<Texture2D> m_FontIcon;
		Ref<Texture2D> m_PhysicsMaterialIcon;
		Ref<Texture2D> m_EntityIcon;
		Ref<Texture2D> m_SceneIcon;
		Ref<Texture2D> m_FolderIcon;
		Ref<Texture2D> m_AsteriskIcon;
		Ref<Texture2D> m_UnknownIcon;

		Path m_ProjectPath;
		Path m_ContentPath;
		Path m_CurrentDirectory;
		Path m_CurrentDirectoryRelative;
		Path m_SelectedFile;
		EditorLayer& m_EditorLayer;
		std::vector<Path> m_Directories;
		std::vector<Path> m_Files;
		std::vector<Path> m_SearchFiles;
		std::vector<Path> m_BackHistory;
		std::vector<Path> m_ForwardHistory;

		TextureImporterPanel m_TextureImporter;
		MeshImporterPanel m_MeshImporter;
		AnimationGraphImporterPanel m_AnimationGraphImporter;
		bool m_DrawTextureImporter = false;
		bool m_DrawMeshImporter = false;
		bool m_DrawAnimationGraphImporter = false;

		float m_ColumnWidth = 1.f;

		Ref<Asset> m_AssetToDelete;
		Ref<Asset> m_AssetToRename;
		Path m_CopiedPath;
		bool m_bCopy = false; // If true, it's copy, else - cut

		bool m_ShowSaveScenePopup = false;

		bool m_bShowInputName = false;
		bool m_ShowDeleteConfirmation = false;
		std::string m_DeleteConfirmationMessage;

		enum class InputNameState
		{
			None, NewFolder, AssetRename
		} m_InputState = InputNameState::None;

		bool m_ContentBrowserHovered = false;
		bool m_DrawAddPanel = false;
		bool m_RefreshBrowser = true;
	};
}
