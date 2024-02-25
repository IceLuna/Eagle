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
	constexpr static float s_ItemSize = 96.f;
	char ContentBrowserPanel::searchBuffer[searchBufferSize];

	static const char* s_ImportTooltip = "Import a texture, mesh, audio, or font";
	static bool IsReloadableAsset(AssetType type)
	{
		switch (type)
		{
			case AssetType::Texture2D:
			case AssetType::TextureCube:
			case AssetType::StaticMesh:
			case AssetType::SkeletalMesh:
			case AssetType::Audio:
			case AssetType::Font: return true;
			default: return false;
		}
	}

	static bool GetAssetBorderColor(AssetType type, ImVec4& borderColor)
	{
		switch (type)
		{
		case AssetType::Texture2D:
			borderColor = ImVec4(0.85f, 0.075f, 0.075f, 1.f);
			return true;
		case AssetType::TextureCube:
			borderColor = ImVec4(0.85f, 0.075f, 0.075f, 1.f);
			return true;
		case AssetType::StaticMesh:
			borderColor = ImVec4(0.5f, 0.5f, 0.85f, 1.f);
			return true;
		case AssetType::SkeletalMesh:
			borderColor = ImVec4(0.75f, 0.15f, 0.75f, 1.f);
			return true;
		case AssetType::Audio:
			borderColor = ImVec4(0.65f, 0.65f, 0.15f, 1.f);
			return true;
		case AssetType::SoundGroup:
			borderColor = ImVec4(0.95f, 0.95f, 0.15f, 1.f);
			return true;
		case AssetType::Font:
			borderColor = ImVec4(0.5f, 0.5f, 0.5f, 1.f); // TODO:
			return true;
		case AssetType::Material:
			borderColor = ImVec4(0.01f, 0.85f, 0.01f, 1.f);
			return true;
		case AssetType::PhysicsMaterial:
			borderColor = ImVec4(0.5f, 0.5f, 0.5f, 1.f); // TODO:
			return true;
		case AssetType::Entity:
			borderColor = ImVec4(0.25f, 0.25f, 1.f, 1.f);
			return true;
		case AssetType::Scene:
			borderColor = ImVec4(0.95f, 0.95f, 0.15f, 1.f); // TODO: it's the same as sound group. Figure it out
			return true;
		}
		return false;
	}

	ContentBrowserPanel::ContentBrowserPanel(EditorLayer& editorLayer)
		: m_ProjectPath(Project::GetProjectPath())
		, m_ContentPath(Project::GetContentPath())
		, m_CurrentDirectory(m_ContentPath)
		, m_CurrentDirectoryRelative(std::filesystem::relative(m_CurrentDirectory, m_ProjectPath))
		, m_EditorLayer(editorLayer)
	{
		m_TextureIcon = Texture2D::Create(Application::GetCorePath() / "assets/textures/Editor/textureicon.png");
		m_MeshIcon = Texture2D::Create(Application::GetCorePath() / "assets/textures/Editor/meshicon.png");
		m_AudioIcon = Texture2D::Create(Application::GetCorePath() / "assets/textures/Editor/audioicon.png");
		m_SoundGroupIcon = Texture2D::Create(Application::GetCorePath() / "assets/textures/Editor/soundgroupicon.png");
		m_FontIcon = Texture2D::Create(Application::GetCorePath() / "assets/textures/Editor/fonticon.png");
		m_PhysicsMaterialIcon = Texture2D::Create(Application::GetCorePath() / "assets/textures/Editor/physicsmaterialicon.png");
		m_EntityIcon = Texture2D::Create(Application::GetCorePath() / "assets/textures/Editor/entityicon.png");
		m_SceneIcon = Texture2D::Create(Application::GetCorePath() / "assets/textures/Editor/sceneicon.png");

		m_FolderIcon = Texture2D::Create(Application::GetCorePath() / "assets/textures/Editor/foldericon.png");
		m_AsteriskIcon = Texture2D::Create(Application::GetCorePath() / "assets/textures/Editor/asterisk.png");
		m_UnknownIcon = Texture2D::Create(Application::GetCorePath() / "assets/textures/Editor/unknownicon.png");
	}

	void ContentBrowserPanel::OnImGuiRender()
	{
		EG_CPU_TIMING_SCOPED("Content Browser");

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
						{
							Application::Get().GetImGuiLayer()->AddMessage("Rename failed. File already exists");
							EG_CORE_ERROR("Rename failed. File already exists: {}", newFilepath);
						}
						else
							AssetManager::Rename(m_AssetToRename, newFilepath);
						m_AssetToRename.reset();
					}
				}
				input = "";
				m_bShowInputName = false;
				m_InputState = InputNameState::None;
				m_RefreshBrowser = true;
			}
		}
		if (m_ShowDeleteConfirmation)
		{
			if (auto button = UI::ShowMessage("Eagle Editor", m_DeleteConfirmationMessage, UI::ButtonType::OK | UI::ButtonType::Cancel); button != UI::ButtonType::None)
			{
				if (button == UI::ButtonType::OK)
				{
					AssetManager::Delete(m_AssetToDelete);
					m_AssetToDelete.reset();
					m_RefreshBrowser = true;
				}
				m_ShowDeleteConfirmation = false;
			}
		}

		ImVec2 size = ImGui::GetContentRegionAvail();
		m_ColumnWidth = s_ItemSize + GImGui->Style.FramePadding.x * 2.f + 1.f;
		const int columns = int(size[0] / m_ColumnWidth);
		m_ContentBrowserHovered = ImGui::IsWindowHovered();

		//Drawing Path-History buttons on top.
		ImGui::Separator();
		{
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.f, 0.55f, 0.f, 1.f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.f, 0.7f, 0.f, 1.f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.f, 0.35f, 0.f, 1.f));

				if (ImGui::Button("Add asset"))
					m_DrawAddPanel = true;

				ImGui::PopStyleColor(3);

				ImGui::SameLine();
			}
			DrawPathHistory();
		}
		ImGui::Separator();

		if (columns > 1)
		{
			ImGui::Columns(columns, nullptr, false);
			ImGui::SetColumnWidth(0, m_ColumnWidth);
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

		if (m_ContentBrowserHovered || m_RefreshBrowser)
		{
			m_RefreshBrowser = false;
			RefreshContentInfo();
		}

		ImGui::Columns(1);
		ImGui::PopID();

		m_ShowSaveScenePopup = m_ShowSaveScenePopup && m_EditorLayer.GetEditorState() == EditorState::Edit;

		if (m_ShowSaveScenePopup)
		{
			UI::ButtonType result = UI::ShowMessage("Eagle Editor", "Do you want to save the current scene?", UI::ButtonType::YesNoCancel);
			if (result != UI::ButtonType::None)
			{
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

				m_SceneToOpen.reset();
			}
		}

		HandleAssetEditors();
		HandleAddPanel();

		if (m_DrawTextureImporter)
			m_RefreshBrowser |= m_TextureImporter.OnImGuiRender(m_CurrentDirectoryRelative, &m_DrawTextureImporter);

		if (m_DrawMeshImporter)
			m_RefreshBrowser |= m_MeshImporter.OnImGuiRender(m_CurrentDirectoryRelative, &m_DrawMeshImporter);

		ImGui::End();
	}

	void ContentBrowserPanel::RefreshContentInfo()
	{
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

	void ContentBrowserPanel::HandleAssetEditors()
	{
		if (m_ShowTexture2DView)
		{
			if (m_Texture2DToView)
				UI::Editor::OpenTextureEditor(m_Texture2DToView, &m_ShowTexture2DView);
		}
		else
			m_Texture2DToView.reset();

		if (m_ShowTextureCubeView)
		{
			if (m_TextureCubeToView)
				UI::Editor::OpenTextureEditor(m_TextureCubeToView, &m_ShowTextureCubeView);
		}
		else
			m_TextureCubeToView.reset();

		if (m_ShowMaterialEditor)
		{
			if (m_MaterialToView)
				UI::Editor::OpenMaterialEditor(m_MaterialToView, &m_ShowMaterialEditor);
		}
		else
			m_MaterialToView.reset();

		if (m_ShowPhysicsMaterialEditor)
		{
			if (m_PhysicsMaterialToView)
				UI::Editor::OpenPhysicsMaterialEditor(m_PhysicsMaterialToView, &m_ShowPhysicsMaterialEditor);
		}
		else
			m_PhysicsMaterialToView.reset();

		if (m_ShowAudioEditor)
		{
			if (m_AudioToView)
				UI::Editor::OpenAudioEditor(m_AudioToView, &m_ShowAudioEditor);
		}
		else
			m_AudioToView.reset();

		if (m_ShowSoundGroupEditor)
		{
			if (m_SoundGroupToView)
				UI::Editor::OpenSoundGroupEditor(m_SoundGroupToView, &m_ShowSoundGroupEditor);
		}
		else
			m_SoundGroupToView.reset();

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
						ImGui::SameLine();
						UI::HelpMarker("The scene needs to be saved to store reloaded assets");

						if (bDisableReload)
							UI::PopItemDisabled();

						UI::Tooltip("On the opened scene, all entities created from this assets will be reloaded to match this asset");
					}

				}
				ImGui::End(); // Entity Editor
			}
		}
		else
			m_EntityToView.reset();
	}

	void ContentBrowserPanel::HandleAddPanel()
	{
		if (m_DrawAddPanel)
		{
			ImGui::OpenPopup("Add asset");

			// Always center this window when appearing
			ImVec2 center = ImGui::GetMainViewport()->GetCenter();
			ImVec2 size = ImVec2(720.f, 5.66f * s_ItemSize);
			ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
			ImGui::SetNextWindowSize(size, ImGuiCond_FirstUseEver);
		}

		if (ImGui::BeginPopupModal("Add asset", &m_DrawAddPanel))
		{
			bool bCreatedAsset = false;

			if (UI::ImageButtonWithTextHorizontal(m_UnknownIcon, "Import...", { s_ItemSize, s_ItemSize }, s_ItemSize))
			{
				Path path = FileDialog::OpenFile(FileDialog::IMPORT_FILTER);
				if (!path.empty())
				{
					const AssetType assetType = AssetImporter::GetAssetTypeByExtension(path);
					if (assetType == AssetType::Texture2D || assetType == AssetType::TextureCube)
					{
						m_TextureImporter = TextureImporterPanel(path);
						m_DrawAddPanel = false;
						m_DrawTextureImporter = true;
					}
					else if (assetType == AssetType::StaticMesh || assetType == AssetType::SkeletalMesh)
					{
						m_MeshImporter = MeshImporterPanel(path);
						m_DrawAddPanel = false;
						m_DrawMeshImporter = true;
					}
					else
					{
						AssetImporter::Import(path, m_CurrentDirectoryRelative, assetType, {});
						bCreatedAsset = true;
					}
				}
			}
			UI::Tooltip(s_ImportTooltip);

			ImGui::Separator();

			if (UI::ImageButtonWithTextHorizontal(m_EntityIcon, "Entity", { s_ItemSize, s_ItemSize }, s_ItemSize))
			{
				AssetImporter::CreateEntity(m_CurrentDirectoryRelative);
				bCreatedAsset = true;
			}

			if (UI::ImageButtonWithTextHorizontal(m_MeshIcon, "Material", { s_ItemSize, s_ItemSize }, s_ItemSize))
			{
				AssetImporter::CreateMaterial(m_CurrentDirectoryRelative);
				bCreatedAsset = true;
			}

			if (UI::ImageButtonWithTextHorizontal(m_PhysicsMaterialIcon, "Physics Material", { s_ItemSize, s_ItemSize }, s_ItemSize))
			{
				AssetImporter::CreatePhysicsMaterial(m_CurrentDirectoryRelative);
				bCreatedAsset = true;
			}

			if (UI::ImageButtonWithTextHorizontal(m_SoundGroupIcon, "Sound Group", { s_ItemSize, s_ItemSize }, s_ItemSize))
			{
				AssetImporter::CreateSoundGroup(m_CurrentDirectoryRelative);
				bCreatedAsset = true;
			}

			if (bCreatedAsset)
			{
				// Close popup and refresh content browser
				m_RefreshBrowser = true;
				m_DrawAddPanel = false;
			}

			ImGui::EndPopup();
		}
	}

	void ContentBrowserPanel::OnEvent(Event& e)
	{
		if (!m_ContentBrowserHovered)
			return;

		bool bHandled = false;
		if (e.GetEventType() == EventType::MouseButtonPressed)
		{
			MouseButtonEvent& mbEvent = (MouseButtonEvent&)e;
			Mouse button = mbEvent.GetMouseCode();
			if (button == Mouse::Button3)
			{
				GoBack();
				bHandled = true;
			}
			else if (button == Mouse::Button4)
			{
				GoForward();
				bHandled = true;
			}
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
						{
							OnSaveAsset(asset);
							bHandled = true;
						}
						break;
					case Key::X:
						if (control)
						{
							OnCutAsset(m_SelectedFile);
							bHandled = true;
						}
						break;
					case Key::C:
						if (control)
						{
							OnCopyAsset(m_SelectedFile);
							bHandled = true;
						}
						break;
					case Key::F2:
						OnRenameAsset(asset);
						bHandled = true;
						break;
					case Key::Delete:
						OnDeleteAsset(asset);
						bHandled = true;
						break;
					}
				}
			}
			
			if (pressedKey == Key::V && control)
			{
				OnPasteAsset();
				bHandled = true;
			}
		}
	
		e.Handled |= bHandled;
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

			{
				const bool bFillBg = m_SelectedFile == path;
				if (bFillBg)
					UI::PushButtonSelectedStyleColors();

				UI::ImageButtonWithText(m_FolderIcon, filename, { s_ItemSize, s_ItemSize }, bFillBg);

				if (bFillBg)
					UI::PopButtonSelectedStyleColors();
			}
			
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

			bHoveredAnyItem |= ImGui::IsItemHovered();
			UI::Tooltip(bHintFullPath ? pathString : filename);
			ImGui::NextColumn();
			ImGui::SetColumnWidth(-1, m_ColumnWidth);
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

			{
				const bool bFillBg = m_SelectedFile == path;
				if (bFillBg)
					UI::PushButtonSelectedStyleColors();

				ImVec4 borderColor;
				const bool bBorderColor = GetAssetBorderColor(assetType, borderColor);
				if (bBorderColor)
					ImGui::PushStyleColor(ImGuiCol_Border, borderColor);

				UI::ImageButtonWithText(Cast<Texture2D>(texture), filename, { s_ItemSize, s_ItemSize }, bFillBg, 2.0f);

				if (bBorderColor)
					ImGui::PopStyleColor();

				if (bFillBg)
					UI::PopButtonSelectedStyleColors();
			}

			if (asset->IsDirty())
			{
				// Setting asterisk's color to be the inverse of window background color
				ImVec4 invWindowBg = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
				invWindowBg.x = 1.f - invWindowBg.x;
				invWindowBg.y = 1.f - invWindowBg.y;
				invWindowBg.z = 1.f - invWindowBg.z;
				const uint32_t color = IM_COL32(uint32_t(invWindowBg.x * 255.f), uint32_t(invWindowBg.y * 255.f), uint32_t(invWindowBg.z * 255.f), 255u);

				constexpr float asteriskDrawOffset = 36.f;
				UI::AddImage(m_AsteriskIcon, ImVec2(p.x + s_ItemSize - asteriskDrawOffset, p.y + s_ItemSize - asteriskDrawOffset), ImVec2(p.x + s_ItemSize, p.y + s_ItemSize), ImVec2(0, 0), ImVec2(1, 1), color);
			}
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

			if (bClicked)
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

			bHoveredAnyItem |= ImGui::IsItemHovered();
			UI::Tooltip(bHintFullPath ? pathString : filename);
			ImGui::NextColumn();
			ImGui::SetColumnWidth(-1, m_ColumnWidth);
		}
		ImGui::PopID();

		if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && !bHoveredAnyItem)
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
							OnDeleteAsset(asset);
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

	void ContentBrowserPanel::OnDeleteAsset(const Ref<Asset>& asset)
	{
		m_ShowDeleteConfirmation = true;
		m_AssetToDelete = asset;
		m_DeleteConfirmationMessage = "Are you sure you want to delete " + m_AssetToDelete->GetPath().stem().u8string()
			+ "?\nDeleting it won't remove it from the current scene,\nbut the next time it's opened, it will be replaced with an empty asset";
	}

	void ContentBrowserPanel::OnPasteAsset()
	{
		Ref<Asset> assetToCopy;
		AssetManager::Get(m_CopiedPath, &assetToCopy);
		if (assetToCopy)
		{
			const std::string newName = m_CopiedPath.stem().u8string() + (m_bCopy ? "_Copy" : "") + Asset::GetExtension();
			const Path newFilepath = m_CurrentDirectoryRelative / newName;
			if (std::filesystem::exists(newFilepath))
			{
				Application::Get().GetImGuiLayer()->AddMessage("Paste failed. File already exists");
				EG_CORE_ERROR("Paste failed. File already exists: {}", newFilepath);
			}
			else
			{
				if (m_bCopy)
				{
					if (!AssetManager::Duplicate(assetToCopy, newFilepath))
						Application::Get().GetImGuiLayer()->AddMessage("Copy failed. See logs for more details");
				}
				else
				{
					if (!AssetManager::Rename(assetToCopy, newFilepath))
						Application::Get().GetImGuiLayer()->AddMessage("Cut failed. See logs for more details");
				}
			}
		}
		else
		{
			Application::Get().GetImGuiLayer()->AddMessage("Failed to paste an asset. Didn't find an asset");
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
			case AssetType::StaticMesh:
				return m_MeshIcon;
			case AssetType::Audio:
				return m_AudioIcon;
			case AssetType::SoundGroup:
				return m_SoundGroupIcon;
			case AssetType::Font:
				return m_FontIcon;
			case AssetType::PhysicsMaterial:
				return m_PhysicsMaterialIcon;
			case AssetType::Entity:
				return m_EntityIcon;
			case AssetType::Scene:
				return m_SceneIcon;
			default:
				return m_UnknownIcon;
		}
	}
}
