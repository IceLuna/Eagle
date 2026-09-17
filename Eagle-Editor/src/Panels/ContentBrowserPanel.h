#pragma once

#include "EntityPropertiesPanel.h"
#include "AssetImporterPanel.h"
#include "Eagle/UI/Editors/AnimationGraphEditor.h"

#include "Eagle/Renderer/VidWrappers/Texture.h"
#include "Eagle/Utils/Utils.h"
#include "Eagle/Utils/FileWatcher.h"
#include "Eagle/Asset/Asset.h"

#include "../AssetEditors/AssetEditor.h"

#include <filesystem>

namespace Eagle
{
	// Cached, frame-invariant data of a single content browser cell.
	// It's built once in `RefreshContentInfo`/`GetSearchingContent` so that the draw loop doesn't have to
	// touch the filesystem, query the AssetManager or do UTF conversions every single frame.
	struct ContentEntry
	{
		Path Filepath;               // Relative to the project path
		std::string Filename;        // Displayed name. Stem for assets, folder name for directories
		std::string FullPathString;  // Used for tooltips and as the popup ID
		Ref<Asset> AssetRef;         // Null for directories
		AssetType Type = AssetType::None;
		ImVec4 BorderColor = ImVec4(0.f, 0.f, 0.f, 0.f);
		bool bHasBorderColor = false;
		bool bDirectory = false;
	};

	class EditorLayer;
	class Event;
	class Asset;
	class Sound2D;

	class ContentBrowserPanel
	{
	public:
		ContentBrowserPanel();
		~ContentBrowserPanel();

		void OnImGuiRender();

		void OnEvent(Event& e);

		void OpenAssetEditor(const Ref<Asset>& asset);

		const Path& GetCurrentRelativeDirectory() const { return m_CurrentDirectoryRelative; }
		void RefreshBrowserContent() { m_RefreshBrowser = true; }

		static ContentBrowserPanel& Get(); // Not a good idea

		const char* GetWindowName() const { return "Content Browser"; }

	private:
		void DrawContent(const std::vector<ContentEntry>& entries, int32_t columns, bool bHintFullPath = false);
		void DrawEntry(const ContentEntry& entry, const ImVec2& thumbnailSize, bool bHintFullPath, bool& bHoveredAnyItem);
		void DrawPathHistory();
		void HandleAddPanel();
		void HandleAssetEditors();
		void RefreshContentInfo();
		void UpdateDirectoryWatcher();
		bool HandleImport();
		void HandleDragDropOnFolder(const Path& destinationFolder);
		void CloseInputField();

		void GetSearchingContent(const std::string& search, std::vector<ContentEntry>& outEntries);

		void DrawItemPopupMenu(const Path& path, const std::string& pathString, bool bDirectory);
		void DrawContentBrowserPopupMenu();

		void GoBack();
		void GoForward();

		void OnCopyAsset(const Path& path);
		void OnCutAsset(const Path& path);
		void OnRenameAsset(const Ref<Asset>& asset);
		void OnDeleteAsset(const Ref<Asset>& asset);
		void OnSaveAsset(const Ref<Asset>& asset);
		void OnDeleteFolder(const Path& path);
		void DuplicateAsset(const Ref<Asset>& asset);

		void OnDirectoryOpened(const Path& previousPath);

		void NavigateToFile(const Path& path);
		void SetSelected(const Path& path, bool bScrollToIt);

		bool OnKeyPressed(KeyPressedEvent& e);
		bool OnMousePressedEvent(MouseButtonPressedEvent& e);

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
		Ref<AssetScene> m_SceneToOpen;
		
		std::unordered_map<Ref<Asset>, Ref<AssetEditor>> m_AssetEditors;

		Ref<Texture2D> m_FolderIcon;
		Ref<Texture2D> m_AsteriskIcon;

		Path m_ProjectPath;
		Path m_ContentPath;
		Path m_CurrentDirectory;
		Path m_CurrentDirectoryRelative;
		Path m_SelectedFile;
		std::vector<ContentEntry> m_Entries; // Directories first, assets afterwards
		std::vector<ContentEntry> m_FileEntries; // To reuse the allocation between refreshes
		std::vector<ContentEntry> m_SearchEntries; // Assets only
		std::vector<Path> m_BackHistory;
		std::vector<Path> m_ForwardHistory;

		TextureImporterPanel m_TextureImporter;
		MeshImporterPanel m_MeshImporter;
		AnimationGraphImporterPanel m_AnimationGraphImporter;
		AnimationBlendSpaceImporterPanel m_AnimationBlendSpaceImporter;

		// TODO: Improve it. We can a unique ptr to allocated importers and check if they're valid
		bool m_DrawTextureImporter = false;
		bool m_DrawMeshImporter = false;
		bool m_DrawAnimationGraphImporter = false;
		bool m_DrawAnimationBlendSpaceImporter = false;

		// Notifies us when the currently displayed directory is modified by something outside of the editor,
		// so that the (expensive) rescan only happens when it's actually required.
		// Can be null if the directory can't be watched, in which case we fall back to polling
		Ref<FileWatcher> m_DirectoryWatcher;
		Path m_WatchedDirectory; // The directory `m_DirectoryWatcher` was created for

		float m_ColumnWidth = 1.f;
		float m_PendingRefreshTimer = 0.f;     // > 0 while waiting for reported changes to settle down
		float m_TimeSinceContentRefresh = 0.f; // Only used when there's no watcher

		Path m_FolderToDelete;
		Ref<Asset> m_AssetToDelete;
		Ref<Asset> m_AssetToRename;
		Path m_CopiedPath;
		bool m_bCopy = false; // If true, it's copy, else - cut

		bool m_ShowSaveScenePopup = false;

		std::string m_Search;
		std::string m_PopupInput;
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
		bool m_ScrollToSelected = false;
	};
}
