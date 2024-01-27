#include "egpch.h"
#include "ContentBrowserPanel.h"
#include "../EditorLayer.h"

#include "Eagle/Core/Project.h"
#include "Eagle/Asset/AssetManager.h"
#include "Eagle/Asset/AssetImporter.h"
#include "Eagle/Utils/PlatformUtils.h"
#include "Eagle/UI/UI.h"

#include "Eagle/Debug/CPUTimings.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

namespace Eagle
{
	char ContentBrowserPanel::searchBuffer[searchBufferSize];

	static bool IsReloadableAsset(AssetType type)
	{
		switch (type)
		{
			case AssetType::Texture2D:
			case AssetType::TextureCube:
			case AssetType::Mesh:
			case AssetType::Audio:
			case AssetType::Font: return true;
			default: return false;
		}
	}

	ContentBrowserPanel::ContentBrowserPanel(EditorLayer& editorLayer)
		: m_ProjectPath(Project::GetProjectPath())
		, m_ContentPath(Project::GetContentPath())
		, m_CurrentDirectory(m_ContentPath)
		, m_CurrentDirectoryRelative(std::filesystem::relative(m_CurrentDirectory, m_ProjectPath))
		, m_EditorLayer(editorLayer)
	{
		m_MeshIcon = Texture2D::Create(Application::GetCorePath() / "assets/textures/Editor/meshicon.png");
		m_TextureIcon = Texture2D::Create(Application::GetCorePath() / "assets/textures/Editor/textureicon.png");
		m_SceneIcon = Texture2D::Create(Application::GetCorePath() / "assets/textures/Editor/sceneicon.png");
		m_SoundIcon = Texture2D::Create(Application::GetCorePath() / "assets/textures/Editor/soundicon.png");
		m_UnknownIcon = Texture2D::Create(Application::GetCorePath() / "assets/textures/Editor/unknownicon.png");
		m_FolderIcon = Texture2D::Create(Application::GetCorePath() / "assets/textures/Editor/foldericon.png");
		m_FontIcon = Texture2D::Create(Application::GetCorePath() / "assets/textures/Editor/fonticon.png");
		m_AsteriskIcon = Texture2D::Create(Application::GetCorePath() / "assets/textures/Editor/asterisk.png");
	}

	void ContentBrowserPanel::OnImGuiRender()
	{
		EG_CPU_TIMING_SCOPED("Content Browser");

		static bool bRenderingFirstTime = true;

		ImGui::Begin("Content Browser");
		ImGui::PushID("Content Browser");
		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
		ImGui::InputTextWithHint("##search", "Search...", searchBuffer, searchBufferSize);

		if (ImGui::BeginPopupContextWindow("ContentBrowserPopup", ImGuiPopupFlags_MouseButtonRight))
		{
			if (ImGui::MenuItem("Create Entity"))
				AssetImporter::CreateEntity(m_CurrentDirectoryRelative);
			if (ImGui::MenuItem("Create Material"))
				AssetImporter::CreateMaterial(m_CurrentDirectoryRelative);
			if (ImGui::MenuItem("Create Physics Material"))
				AssetImporter::CreatePhysicsMaterial(m_CurrentDirectoryRelative);
			if (ImGui::MenuItem("Create Sound Group"))
				AssetImporter::CreateSoundGroup(m_CurrentDirectoryRelative);

			if (ImGui::MenuItem("Create folder"))
			{
				m_bShowInputName = true;
				m_InputState = InputNameState::NewFolder;
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Import an asset"))
			{
				Path path = FileDialog::OpenFile(FileDialog::IMPORT_FILTER);
				if (!path.empty())
					AssetImporter::Import(path, m_CurrentDirectoryRelative, {});
			}

			ImGui::Separator();

			const bool bDisablePaste = m_CopiedPath.empty();
			if (bDisablePaste)
				UI::PushItemDisabled();

			if (ImGui::MenuItem("Paste"))
				OnPasteAsset();

			if (bDisablePaste)
				UI::PopItemDisabled();

			ImGui::EndPopup();
		}
		if (m_bShowInputName)
		{
			static std::string input;
			const char* hint = m_InputState == InputNameState::NewFolder ? "Folder name" :
				m_InputState == InputNameState::AssetRename ? "New asset name" : "";
			UI::ButtonType pressedButton = UI::InputPopup("Eagle Editor", hint, input);
			if (pressedButton != UI::ButtonType::None)
			{
				if (pressedButton == UI::ButtonType::OK)
				{
					if (m_InputState == InputNameState::NewFolder)
					{
						const size_t size = input.size() + 1;
						const char* buf_end = NULL;
						ImWchar* wData = new ImWchar[size];
						ImTextStrFromUtf8(wData, int(size), input.c_str(), NULL, &buf_end);
						Path newPath = m_CurrentDirectory / Path((const char16_t*)wData);
						delete[] wData;
						std::filesystem::create_directory(newPath);
					}
					else if (m_InputState == InputNameState::AssetRename)
					{
						const Path newFilepath = m_CurrentDirectoryRelative / (input + Asset::GetExtension());
						if (std::filesystem::exists(newFilepath))
							EG_CORE_ERROR("Rename failed. File already exists: {}", newFilepath);
						else
							AssetManager::Rename(m_AssetToRename, newFilepath);
					}
				}
				input = "";
				m_bShowInputName = false;
				m_InputState = InputNameState::None;
			}
		}
		if (m_ShowDeleteConfirmation)
		{
			if (auto button = UI::ShowMessage("Eagle Editor", m_DeleteConfirmationMessage, UI::ButtonType::OK | UI::ButtonType::Cancel); button != UI::ButtonType::None)
			{
				if (button == UI::ButtonType::OK)
				{
					std::error_code error;
					std::filesystem::remove(m_AssetToDelete, error);
					if (error)
						EG_CORE_ERROR("Failed to delete {}. Error: {}", m_AssetToDelete, error.message());
					else
						EG_CORE_TRACE("Deleted asset at: {}", m_AssetToDelete);
				}
				m_ShowDeleteConfirmation = false;
			}
		}

		ImVec2 size = ImGui::GetContentRegionAvail();
		constexpr int columnWidth = 72;
		int columns = (int)size[0] / columnWidth;
		m_ContentBrowserHovered = ImGui::IsWindowHovered();

		//Drawing Path-History buttons on top.
		ImGui::Separator();
		DrawPathHistory();
		ImGui::Separator();

		if (columns > 1)
		{
			ImGui::Columns(columns, nullptr, false);
			ImGui::SetColumnWidth(0, columnWidth);
		}

		const char* buf_end = NULL;
		ImWchar wData[searchBufferSize];
		ImTextStrFromUtf8(wData, searchBufferSize, searchBuffer, NULL, &buf_end);
		std::u16string tempu16((const char16_t*)wData);
		Path temp = tempu16;
		std::string search = temp.u8string();
		if (search.length())
		{
			static std::vector<Path> directoriesTempEmpty; // empty dirs not to display dirs
			if (m_ContentBrowserHovered)
			{
				m_SearchFiles.clear();
				GetSearchingContent(search, m_SearchFiles);
			}
			DrawContent(directoriesTempEmpty, m_SearchFiles, true);
		}
		else
		{
			DrawContent(m_Directories, m_Files);
		}

		if (m_ContentBrowserHovered || bRenderingFirstTime)
		{
			bRenderingFirstTime = false;
			m_Directories.clear();
			m_Files.clear();

			// If dir is not there anymore, reset to content dir and clear history
			if (!std::filesystem::exists(m_CurrentDirectory))
			{
				m_CurrentDirectory = m_ContentPath;
				m_CurrentDirectoryRelative = std::filesystem::relative(m_CurrentDirectory, m_ProjectPath);
				m_BackHistory.clear();
				m_ForwardHistory.clear();
			}
			
			const Path& projectPath = Project::GetProjectPath();
			for (auto& dir : std::filesystem::directory_iterator(m_CurrentDirectory))
			{
				const auto& path = dir.path();

				if (dir.is_directory())
					m_Directories.push_back(std::filesystem::relative(path, projectPath));
				else
					m_Files.push_back(std::filesystem::relative(path, projectPath));
			}
		}

		ImGui::Columns(1);
		ImGui::PopID();

		m_ShowSaveScenePopup = m_ShowSaveScenePopup && m_EditorLayer.GetEditorState() == EditorState::Edit;

		if (m_ShowSaveScenePopup)
		{
			UI::ButtonType result = UI::ShowMessage("Eagle Editor", "Do you want to save the current scene?", UI::ButtonType::YesNoCancel);
			if (result == UI::ButtonType::Yes)
			{
				if (m_EditorLayer.SaveScene()) // Open a new scene only if the old scene was successfully saved
					m_EditorLayer.OpenScene(m_SceneToOpen);
				m_ShowSaveScenePopup = false;
			}
			else if (result == UI::ButtonType::No)
			{
				m_EditorLayer.OpenScene(m_SceneToOpen);
				m_ShowSaveScenePopup = false;
			}
			else if (result == UI::ButtonType::Cancel)
				m_ShowSaveScenePopup = false;
		}

		if (m_ShowTexture2DView)
		{
			if (m_Texture2DToView)
				UI::Editor::OpenTextureEditor(m_Texture2DToView, &m_ShowTexture2DView);
		}
		if (m_ShowTextureCubeView)
		{
			if (m_TextureCubeToView)
				UI::Editor::OpenTextureEditor(m_TextureCubeToView, &m_ShowTextureCubeView);
		}
		if (m_ShowMaterialEditor)
		{
			if (m_MaterialToView)
				UI::Editor::OpenMaterialEditor(m_MaterialToView, &m_ShowMaterialEditor);
		}
		if (m_ShowPhysicsMaterialEditor)
		{
			if (m_PhysicsMaterialToView)
				UI::Editor::OpenPhysicsMaterialEditor(m_PhysicsMaterialToView, &m_ShowPhysicsMaterialEditor);
		}
		if (m_ShowAudioEditor)
		{
			if (m_AudioToView)
				UI::Editor::OpenAudioEditor(m_AudioToView, &m_ShowAudioEditor);
		}
		if (m_ShowSoundGroupEditor)
		{
			if (m_SoundGroupToView)
				UI::Editor::OpenSoundGroupEditor(m_SoundGroupToView, &m_ShowSoundGroupEditor);
		}
		if (m_ShowEntityEditor)
		{
			if (m_EntityToView)
			{
				constexpr bool bRuntime = false;
				constexpr bool bVolumetricsEnabled = true;
				constexpr bool bDrawTransform = false;

				if (ImGui::Begin("Entity Editor", &m_ShowEntityEditor))
				{
					const bool bEntityChanged = m_EntityProperties.OnImGuiRender(*m_EntityToView->GetEntity().get(), bRuntime, bVolumetricsEnabled, bDrawTransform);
					if (bEntityChanged)
						m_EntityToView->SetDirty(true);

					ImGui::Separator();
					ImGui::Separator();

					{
						if (ImGui::Button("Save asset"))
							Asset::Save(m_EntityToView);
						
						const bool bDisableReload = m_EditorLayer.GetEditorState() != EditorState::Edit;
						if (bDisableReload)
							UI::PushItemDisabled();

						ImGui::SameLine();

						if (ImGui::Button("Reload entities"))
						{
							auto& scene = Scene::GetCurrentScene();
							scene->ReloadEntitiesCreatedFromAsset(m_EntityToView);
						}

						if (bDisableReload)
							UI::PopItemDisabled();

						UI::Tooltip("On the opened scene, all entities created from this assets will be reloaded to match this asset");
					}

				}
				ImGui::End(); // Entity Editor
			}
		}

		ImGui::End();
	}

	void ContentBrowserPanel::OnEvent(Event& e)
	{
		if (!m_ContentBrowserHovered)
			return;

		if (e.GetEventType() == EventType::MouseButtonPressed)
		{
			MouseButtonEvent& mbEvent = (MouseButtonEvent&)e;
			Mouse button = mbEvent.GetMouseCode();
			if (button == Mouse::Button3)
				GoBack();
			else if (button == Mouse::Button4)
				GoForward();
		}

		if (e.GetEventType() == EventType::KeyPressed)
		{
			KeyPressedEvent& keyEvent = (KeyPressedEvent&)e;
			const Key pressedKey = keyEvent.GetKey();
			const bool control = Input::IsKeyPressed(Key::LeftControl) || Input::IsKeyPressed(Key::RightControl);

			if (!std::filesystem::is_directory(m_SelectedFile))
			{
				Ref<Asset> asset;
				if (AssetManager::Get(m_SelectedFile, &asset))
				{
					//Shortcuts
					if (keyEvent.GetRepeatCount() > 0)
						return;


					switch (pressedKey)
					{
					case Key::S:
						if (control)
							OnSaveAsset(asset);
						break;
					case Key::X:
						if (control)
							OnCutAsset(m_SelectedFile);
						break;
					case Key::C:
						if (control)
							OnCopyAsset(m_SelectedFile);
						break;
					case Key::W:
						if (control)
							OnRenameAsset(asset);
						break;
					case Key::Delete:
						OnDeleteAsset(m_SelectedFile);
						break;
					}
				}
			}
			
			if (pressedKey == Key::V)
				if (control)
					OnPasteAsset();
		}
	}

	void ContentBrowserPanel::DrawContent(const std::vector<Path>& directories, const std::vector<Path>& files, bool bHintFullPath /* = false */)
	{
		bool bHoveredAnyItem = false;

		ImGui::PushID("DIRECTORIES_FILL");
		for (auto& dir : directories)
		{
			const auto& path = dir;
			std::string pathString = path.u8string();
			std::string filename = path.filename().u8string();

			UI::Image(m_FolderIcon, { 64, 64 });
			
			DrawPopupMenu(path);

			bHoveredAnyItem |= ImGui::IsItemHovered();
			if (ImGui::IsItemClicked())
			{
				if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
				{
					auto prevDir = m_CurrentDirectory;
					m_CurrentDirectory = path;
					m_CurrentDirectoryRelative = std::filesystem::relative(m_CurrentDirectory, m_ProjectPath);
					m_SelectedFile.clear();
					OnDirectoryOpened(prevDir);
				}
				else
				{
					m_SelectedFile = path;
				}
			}
			bool bChangeColor = m_SelectedFile == path;
			if (bChangeColor)
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.8f, 0.1f, 0.15f, 1.f });
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.9f, 0.2f, 0.2f, 1.f });
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.8f, 0.1f, 0.15f, 1.f });
			}

			if (ImGui::Button(filename.c_str(), { 64, 22 }))
			{
				auto prevDir = m_CurrentDirectory;
				m_CurrentDirectory = path;
				m_CurrentDirectoryRelative = std::filesystem::relative(m_CurrentDirectory, m_ProjectPath);
				OnDirectoryOpened(prevDir);
				m_SelectedFile.clear();
			}
			DrawPopupMenu(path, 1);
			if (bChangeColor)
				ImGui::PopStyleColor(3);

			bHoveredAnyItem |= ImGui::IsItemHovered();
			UI::Tooltip(bHintFullPath ? pathString : filename);
			ImGui::NextColumn();
		}
		ImGui::PopID();

		ImGui::PushID("FILES_FILL");
		for (auto& file : files)
		{
			const auto& path = file;
			std::string pathString = path.u8string();
			std::string filename = path.stem().u8string();

			Ref<Asset> asset;
			if (AssetManager::Get(path, &asset) == false)
				continue; // Ignore non assets

			Ref<Texture> texture;
			const AssetType assetType = asset->GetAssetType();

			if (assetType == AssetType::Texture2D)
				texture = Cast<AssetTexture2D>(asset)->GetTexture();
			else if (assetType == AssetType::TextureCube)
				texture = Cast<AssetTextureCube>(asset)->GetTexture()->GetTexture2D();
			else
				texture = GetFileIconTexture(assetType);

			bool bClicked = false;
			ImVec2 p = ImGui::GetCursorScreenPos();
			UI::Image(Cast<Texture2D>(texture), { 64, 64 }, { 0, 0 }, { 1, 1 }, m_SelectedFile == path ? ImVec4{ 0.75, 0.75, 0.75, 1.0 } : ImVec4{ 1, 1, 1, 1 });
			if (asset->IsDirty())
				UI::AddImage(m_AsteriskIcon, ImVec2(p.x + 40.f, p.y + 40.f), ImVec2(p.x + 64.f, p.y + 64.f));
			DrawPopupMenu(path);

			//Handling Drag Event.
			{
				if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
				{
					const char* cellTag = GetAssetDragDropCellTag(assetType);
					std::wstring wide = path.wstring();
					const wchar_t* tt = wide.c_str();
					ImGui::SetDragDropPayload(cellTag, tt, (wide.size() + 1) * sizeof(wchar_t));
					ImGui::Text(filename.c_str());

					ImGui::EndDragDropSource();
				}
			}

			bHoveredAnyItem |= ImGui::IsItemHovered();
			bClicked |= ImGui::IsItemClicked();
			if (bClicked)
			{
				if (!ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
					m_SelectedFile = path;
			}
			bClicked = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && bClicked;

			//If drawing currently selected file, change its color.
			bool bChangeColor = m_SelectedFile == path;
			if (bChangeColor)
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.8f, 0.1f, 0.15f, 1.f });
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.9f, 0.2f, 0.2f, 1.f });
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.8f, 0.1f, 0.15f, 1.f });
			}

			//Button OR File-Double-Click event.
			if (ImGui::Button(filename.c_str(), { 64, 22 }) || bClicked)
			{
				if (assetType == AssetType::Scene)
				{
					m_ShowSaveScenePopup = true;
					m_SceneToOpen = Cast<AssetScene>(asset);
				}
				else if (assetType == AssetType::Texture2D)
				{
					m_Texture2DToView = Cast<AssetTexture2D>(asset);
					m_ShowTexture2DView = true;
				}
				else if (assetType == AssetType::TextureCube)
				{
					m_TextureCubeToView = Cast<AssetTextureCube>(asset);
					m_ShowTextureCubeView = true;
				}
				else if (assetType == AssetType::Material)
				{
					m_MaterialToView = Cast<AssetMaterial>(asset);
					m_ShowMaterialEditor = true;
				}
				else if (assetType == AssetType::PhysicsMaterial)
				{
					m_PhysicsMaterialToView = Cast<AssetPhysicsMaterial>(asset);
					m_ShowPhysicsMaterialEditor = true;
				}
				else if (assetType == AssetType::Audio)
				{
					m_AudioToView = Cast<AssetAudio>(asset);
					m_ShowAudioEditor = true;
				}
				else if (assetType == AssetType::SoundGroup)
				{
					m_SoundGroupToView = Cast<AssetSoundGroup>(asset);
					m_ShowSoundGroupEditor = true;
				}
				else if (assetType == AssetType::Entity)
				{
					m_EntityProperties = {};
					m_EntityToView = Cast<AssetEntity>(asset);
					m_ShowEntityEditor = true;
				}
			}
			DrawPopupMenu(path, 1);
			if (bChangeColor)
				ImGui::PopStyleColor(3);

			bHoveredAnyItem |= ImGui::IsItemHovered();
			UI::Tooltip(bHintFullPath ? pathString : filename);
			ImGui::NextColumn();
		}
		ImGui::PopID();

		if (ImGui::IsMouseDown(0) && !bHoveredAnyItem)
		{
			m_SelectedFile.clear();
		}
	}

	void ContentBrowserPanel::DrawPathHistory()
	{
		bool bEmptyBackHistory = m_BackHistory.empty();
		bool bEmptyForwardHistory = m_ForwardHistory.empty();

		ImGui::PushItemFlag(ImGuiItemFlags_Disabled, bEmptyBackHistory);
		if (bEmptyBackHistory)
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{ 0.5f, 0.5f, 0.5f, 1.f });

		if (ImGui::Button("<-"))
			GoBack();
		ImGui::PopItemFlag();
		if (bEmptyBackHistory)
			ImGui::PopStyleColor(1);

		ImGui::SameLine();

		ImGui::PushItemFlag(ImGuiItemFlags_Disabled, bEmptyForwardHistory);
		if (bEmptyForwardHistory)
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{ 0.5f, 0.5f, 0.5f, 1.f });

		if (ImGui::Button("->"))
			GoForward();
		ImGui::PopItemFlag();
		if (bEmptyForwardHistory)
			ImGui::PopStyleColor(1);

		ImGui::SameLine();

		Path temp = m_CurrentDirectoryRelative;
		static std::vector<Path> paths; paths.clear();
		paths.push_back(temp.filename()); //Current dir
		while (!temp.empty()) //Saving all dir names separatly in vector
		{
			auto parent = temp.parent_path();
			if (parent.empty())
				break;
			paths.push_back(parent.filename());
			temp = temp.parent_path();
		}

		temp = m_ProjectPath;

		for (auto it = paths.rbegin(); it != paths.rend(); ++it) //Drawing buttons
		{
			temp /= (*it);
			std::string filename = (*it).u8string();
			if (ImGui::Button(filename.c_str()))
			{
				m_SelectedFile.clear();
				auto prevPath = m_CurrentDirectory;
				m_CurrentDirectory = temp;
				m_CurrentDirectoryRelative = std::filesystem::relative(m_CurrentDirectory, m_ProjectPath);
				OnDirectoryOpened(prevPath);
			}

			auto tempIT = it;
			++tempIT;
			if (tempIT != paths.rend())
			{
				ImGui::SameLine();
				ImGui::Text("%s", ">>");
				ImGui::SameLine();
			}
		}
	}

	void ContentBrowserPanel::GetSearchingContent(const std::string& search, std::vector<Path>& outFiles)
	{
		const Path& projectPath = Project::GetProjectPath();
		for (auto& dirEntry : std::filesystem::recursive_directory_iterator(m_CurrentDirectory))
		{
			if (dirEntry.is_directory())
				continue;

			const Path path = dirEntry.path();
			const std::string filename = path.filename().u8string();

			std::size_t pos = Utils::FindSubstringI(filename, search);
			if (pos != std::string::npos)
			{
				outFiles.push_back(std::filesystem::relative(path, projectPath));
			}
		}
	}

	void ContentBrowserPanel::DrawPopupMenu(const Path& path, int timesCalledForASinglePath)
	{
		static bool bDoneOnce = false;
		std::string pathString = path.u8string();
		pathString += std::to_string(timesCalledForASinglePath); // TODO: can improve?
		if (ImGui::BeginPopupContextItem(pathString.c_str()))
		{
			if (!bDoneOnce)
			{
				m_SelectedFile = path;
				bDoneOnce = true;
			}

			if (ImGui::MenuItem("Show In Explorer"))
				Utils::ShowInExplorer(path);

			if (!std::filesystem::is_directory(path))
			{
				if (ImGui::MenuItem("Show In Folder View"))
					SelectFile(path);

				ImGui::Separator();

				// Reload an asset
				{
					Ref<Asset> asset;
					if (AssetManager::Get(path, &asset))
					{
						if (IsReloadableAsset(asset->GetAssetType()) && ImGui::MenuItem("Reload the asset"))
							Asset::Reload(asset, true);

						if (ImGui::MenuItem("Save the asset"))
							OnSaveAsset(asset);

						ImGui::Separator();

						if (ImGui::MenuItem("Copy"))
							OnCopyAsset(path);
						if (ImGui::MenuItem("Cut"))
							OnCutAsset(path);
						if (ImGui::MenuItem("Rename"))
							OnRenameAsset(asset);
						if (ImGui::MenuItem("Delete"))
							OnDeleteAsset(path);
					}
				}
			}

			ImGui::EndPopup();
		}
		else
			bDoneOnce = false;
	}

	void ContentBrowserPanel::GoBack()
	{
		if (m_BackHistory.size())
		{
			auto& backPath = m_BackHistory.back();
			m_ForwardHistory.push_back(m_CurrentDirectory);
			m_CurrentDirectory = backPath;
			m_CurrentDirectoryRelative = std::filesystem::relative(m_CurrentDirectory, m_ProjectPath);
			m_BackHistory.pop_back();
		}
	}

	void ContentBrowserPanel::GoForward()
	{
		if (m_ForwardHistory.size())
		{
			auto& forwardPath = m_ForwardHistory.back();
			m_BackHistory.push_back(m_CurrentDirectory);
			m_CurrentDirectory = forwardPath;
			m_CurrentDirectoryRelative = std::filesystem::relative(m_CurrentDirectory, m_ProjectPath);
			m_ForwardHistory.pop_back();
		}
	}

	void ContentBrowserPanel::OnCopyAsset(const Path& path)
	{
		m_CopiedPath = path;
		m_bCopy = true;
	}

	void ContentBrowserPanel::OnCutAsset(const Path& path)
	{
		m_CopiedPath = path;
		m_bCopy = false;
	}

	void ContentBrowserPanel::OnRenameAsset(const Ref<Asset>& asset)
	{
		m_bShowInputName = true;
		m_InputState = InputNameState::AssetRename;
		m_AssetToRename = asset;
	}

	void ContentBrowserPanel::OnDeleteAsset(const Path& path)
	{
		m_ShowDeleteConfirmation = true;
		m_AssetToDelete = path;
		m_DeleteConfirmationMessage = "Are you sure you want to delete " + path.stem().u8string()
			+ "?\nDeleting it won't remove it from the current scene,\nbut the next time it's opened, it will be replaced with an empty asset";
	}

	void ContentBrowserPanel::OnPasteAsset()
	{
		// TODO: Added message boxes to errors
		Ref<Asset> assetToCopy;
		AssetManager::Get(m_CopiedPath, &assetToCopy);
		if (assetToCopy)
		{
			const std::string newName = m_CopiedPath.stem().u8string() + (m_bCopy ? "_Copy" : "") + Asset::GetExtension();
			const Path newFilepath = m_CurrentDirectoryRelative / newName;
			if (std::filesystem::exists(newFilepath))
			{
				EG_CORE_ERROR("Paste failed. File already exists: {}", newFilepath);
			}
			else
			{
				if (m_bCopy)
					AssetManager::Duplicate(assetToCopy, newFilepath);
				else
					AssetManager::Rename(assetToCopy, newFilepath);
			}
		}
		else
		{
			EG_CORE_ERROR("Failed to paste an asset. Didn't find an asset at: {}", m_CopiedPath);
		}
		m_CopiedPath.clear();
	}

	void ContentBrowserPanel::OnSaveAsset(const Ref<Asset>& asset)
	{
		if (asset->GetAssetType() == AssetType::Scene)
		{
			if (m_EditorLayer.GetOpenedSceneAsset() == asset)
				m_EditorLayer.SaveScene();
		}
		else
			Asset::Save(asset);
	}

	// By user clicking in UI
	void ContentBrowserPanel::OnDirectoryOpened(const Path& previousPath)
	{
		m_BackHistory.push_back(previousPath);
		m_ForwardHistory.clear();
	}

	void ContentBrowserPanel::SelectFile(const Path& path)
	{
		searchBuffer[0] = '\0';
		m_SelectedFile = path;
		m_CurrentDirectory = path.parent_path();
		m_CurrentDirectoryRelative = std::filesystem::relative(m_CurrentDirectory, m_ProjectPath);
	}
	
	Ref<Texture2D>& ContentBrowserPanel::GetFileIconTexture(AssetType fileFormat)
	{
		switch (fileFormat)
		{
			case AssetType::Texture2D:
			case AssetType::TextureCube:
				return m_TextureIcon;
			case AssetType::Mesh:
				return m_MeshIcon;
			case AssetType::Scene:
				return m_SceneIcon;
			case AssetType::Audio:
				return m_SoundIcon;
			case AssetType::Font:
				return m_FontIcon;
			default:
				return m_UnknownIcon;
		}
	}
}
