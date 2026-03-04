#include "egpch.h"
#include "ContentBrowserPanel.h"
#include "../EditorLayer.h"
#include "../EditorResources.h"

#include "Eagle/Core/Project.h"
#include "Eagle/Asset/AssetManager.h"
#include "Eagle/Asset/AssetImporter.h"
#include "Eagle/Utils/PlatformUtils.h"
#include "Eagle/Utils/ThumbnailCache.h"
#include "Eagle/UI/UI.h"

#include "Eagle/Debug/CPUTimings.h"

#include "../AssetEditors/Texture2DAssetEditor.h"
#include "../AssetEditors/TextureCubeAssetEditor.h"
#include "../AssetEditors/MaterialAssetEditor.h"
#include "../AssetEditors/PhysicsMaterialAssetEditor.h"
#include "../AssetEditors/AudioAssetEditor.h"
#include "../AssetEditors/SoundGroupAssetEditor.h"
#include "../AssetEditors/AnimationAssetEditor.h"
#include "../AssetEditors/AnimationGraphAssetEditor.h"
#include "../AssetEditors/EntityAssetEditor.h"
#include "../AssetEditors/StaticMeshAssetEditor.h"
#include "../AssetEditors/SkeletalMeshAssetEditor.h"
#include "../AssetEditors/ParticleSystemAssetEditor.h"
#include "../AssetEditors/FontAssetEditor.h"
#include "../AssetEditors/AnimationBlendSpaceAssetEditor.h"
#include "../AssetEditors/BehaviorGraphAssetEditor.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

namespace Eagle
{
	static ContentBrowserPanel* s_Instance = nullptr;

	static const char* s_ImportTooltip = "Import a texture, mesh, animation, audio, or font";
	static bool IsReloadableAsset(AssetType type)
	{
		switch (type)
		{
			case AssetType::Texture2D:
			case AssetType::TextureCube:
			case AssetType::StaticMesh:
			case AssetType::SkeletalMesh:
			case AssetType::Audio:
			case AssetType::Font:
			case AssetType::Animation:
				return true;
			default: return false;
		}
	}

	static bool GetAssetBorderColor(AssetType type, ImVec4& borderColor)
	{
		switch (type)
		{
		case AssetType::Texture2D:
			borderColor = ImVec4(0.75f, 0.25f, 0.25f, 1.f);
			return true;
		case AssetType::TextureCube:
			borderColor = ImVec4(0.75f, 0.25f, 0.25f, 1.f);
			return true;
		case AssetType::StaticMesh:
			borderColor = ImVec4(0.5f, 0.5f, 0.85f, 1.f);
			return true;
		case AssetType::SkeletalMesh:
			borderColor = ImVec4(0.945f, 0.64f, 0.945f, 1.f);
			return true;
		case AssetType::Audio:
			borderColor = ImVec4(0.0f, 0.68f, 1.0f, 1.f);
			return true;
		case AssetType::SoundGroup:
			borderColor = ImVec4(1.0, 0.68f, 0.0f, 1.f);
			return true;
		case AssetType::Font:
			borderColor = ImVec4(0.5f, 0.5f, 0.25f, 1.f);
			return true;
		case AssetType::Material:
			borderColor = ImVec4(0.01f, 0.85f, 0.01f, 1.f);
			return true;
		case AssetType::PhysicsMaterial:
			borderColor = ImVec4(0.78f, 0.75f, 0.5f, 1.f);
			return true;
		case AssetType::Entity:
			borderColor = ImVec4(0.25f, 0.5f, 1.f, 1.f);
			return true;
		case AssetType::Scene:
			borderColor = ImVec4(1.0f, 0.611f, 0.0f, 1.f);
			return true;
		case AssetType::Animation:
			borderColor = ImVec4(0.313f, 0.482f, 0.282f, 1.f);
			return true;
		case AssetType::AnimationGraph:
			borderColor = ImVec4(0.784f, 0.455f, 0.0f, 1.f);
			return true;
		case AssetType::ParticleSystem:
			borderColor = ImVec4(0.0f, 1.0f, 1.0f, 1.f);
			return true;
		case AssetType::AnimationBlendSpace:
			borderColor = ImVec4(1.0f, 0.658f, 0.435f, 1.f);
			return true;
		case AssetType::BehaviorGraph:
			borderColor = ImVec4(1.0f, 0.435f, 0.658f, 1.f);
			return true;
		}
		return false;
	}

	static Path OnPasteAsset(const Path& path, const Path& destinationFolder, bool bCopy)
	{
		if (path.empty())
			return {};

		Path newFilepath;
		bool bFailed = false;

		Ref<Asset> assetToCopy;
		AssetManager::Get(path, &assetToCopy);
		if (assetToCopy)
		{
			const std::string newName = path.stem().u8string() + (bCopy ? "_Copy" : "") + Asset::GetExtension();
			newFilepath = destinationFolder / newName;
			if (std::filesystem::exists(newFilepath))
			{
				Application::Get().GetImGuiLayer()->AddMessage("Paste failed. File already exists");
				EG_CORE_ERROR("Paste failed. File already exists: {}", newFilepath.u8string());
				bFailed = true;
			}
			else
			{
				if (bCopy)
				{
					if (!AssetManager::Duplicate(assetToCopy, newFilepath))
					{
						Application::Get().GetImGuiLayer()->AddMessage("Copy failed. See logs for more details");
						bFailed = true;
					}
				}
				else
				{
					if (!AssetManager::Rename(assetToCopy, newFilepath))
					{
						Application::Get().GetImGuiLayer()->AddMessage("Move failed. See logs for more details");
						bFailed = true;
					}
				}
			}
		}
		else
		{
			Application::Get().GetImGuiLayer()->AddMessage("Failed to paste an asset. Didn't find an asset");
			EG_CORE_ERROR("Failed to paste an asset. Didn't find an asset at: {}", path.u8string());
			bFailed = true;
		}

		return bFailed ? Path{} : newFilepath;
	}

	ContentBrowserPanel::ContentBrowserPanel(EditorLayer& editorLayer)
		: m_ProjectPath(Project::GetProjectPath())
		, m_ContentPath(Project::GetContentPath())
		, m_CurrentDirectory(m_ContentPath)
		, m_CurrentDirectoryRelative(std::filesystem::relative(m_CurrentDirectory, m_ProjectPath))
		, m_EditorLayer(editorLayer)
	{
		EG_CORE_ASSERT(!s_Instance);
		s_Instance = this;
		m_FolderIcon = Texture2D::Create(Application::GetCorePath() / "assets/textures/Editor/foldericon.png");
		m_AsteriskIcon = Texture2D::Create(Application::GetCorePath() / "assets/textures/Editor/asterisk.png");
	}

	ContentBrowserPanel::~ContentBrowserPanel()
	{
		s_Instance = nullptr;
	}

	void ContentBrowserPanel::OnImGuiRender()
	{
		EG_CPU_TIMING_SCOPED("Content Browser");

		ImGui::Begin(GetWindowName(), nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
		ImGui::PushID("Content Browser");
		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
		const bool bSearchInputChanged = UI::InputTextWithHint("##search", m_Search, "Search...");

		if (m_bShowInputName)
		{
			const char* hint = m_InputState == InputNameState::NewFolder ? "Folder name" :
				m_InputState == InputNameState::AssetRename ? "New asset name" : "";
			UI::ButtonType pressedButton = UI::InputPopup("Eagle Editor", hint, m_PopupInput);
			if (pressedButton != UI::ButtonType::None)
			{
				if (pressedButton == UI::ButtonType::OK)
				{
					if (m_InputState == InputNameState::NewFolder)
					{
						const size_t size = m_PopupInput.size() + 1;
						const char* buf_end = NULL;
						ImWchar* wData = new ImWchar[size];
						ImTextStrFromUtf8(wData, int(size), m_PopupInput.c_str(), NULL, &buf_end);
						Path newPath = m_CurrentDirectory / Path((const char16_t*)wData);
						delete[] wData;
						if (std::filesystem::create_directory(newPath))
							SetSelected(std::filesystem::relative(newPath, m_ProjectPath), true);
					}
					else if (m_InputState == InputNameState::AssetRename)
					{
						const Path newFilepath = m_CurrentDirectoryRelative / (m_PopupInput + Asset::GetExtension());
						if (std::filesystem::exists(newFilepath))
						{
							Application::Get().GetImGuiLayer()->AddMessage("Rename failed. File already exists");
							EG_CORE_ERROR("Rename failed. File already exists: {}", newFilepath.u8string());
						}
						else if (AssetManager::Rename(m_AssetToRename, newFilepath))
						{
							SetSelected(newFilepath, true);
						}
						m_AssetToRename.reset();
					}
					m_RefreshBrowser = true;
				}
				CloseInputField();
			}
		}
		if (m_ShowDeleteConfirmation)
		{
			if (auto button = UI::ShowMessage("Eagle Editor", m_DeleteConfirmationMessage, UI::ButtonType::OK | UI::ButtonType::Cancel); button != UI::ButtonType::None)
			{
				if (button == UI::ButtonType::OK)
				{
					if (m_AssetToDelete)
					{
						AssetManager::Delete(m_AssetToDelete);
						m_AssetToDelete.reset();
					}
					else if (!m_FolderToDelete.empty())
					{
						// Delete assets
						for (auto& dir : std::filesystem::directory_iterator(m_FolderToDelete))
						{
							const auto& path = dir.path();
							if (!dir.is_directory())
							{
								Ref<Asset> asset;
								if (AssetManager::Get(path, &asset))
								{
									AssetManager::Delete(asset);
								}
							}
						}
						std::filesystem::remove_all(m_FolderToDelete); // Delete folder & its content
						m_FolderToDelete.clear();
					}
					m_RefreshBrowser = true;
				}
				m_ShowDeleteConfirmation = false;
			}
		}

		ImVec2 size = ImGui::GetContentRegionAvail();
		m_ColumnWidth = ThumbnailCache::GetThumbnailSize().x + GImGui->Style.FramePadding.x * 2.f + 1.f;
		const int columns = glm::max(1, int(size[0] / m_ColumnWidth));
		m_ContentBrowserHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);

		// Drawing Path-History buttons on top.
		ImGui::Separator();
		{
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.f, 0.45f, 0.f, 1.f));
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

		if (!m_Search.empty())
		{
			static std::vector<Path> directoriesTempEmpty; // empty dirs not to display dirs
			if (bSearchInputChanged || m_RefreshBrowser)
			{
				m_SearchFiles.clear();
				GetSearchingContent(m_Search, m_SearchFiles);
			}
			DrawContent(directoriesTempEmpty, m_SearchFiles, columns, true);
		}
		else
		{
			if (m_ContentBrowserHovered || m_RefreshBrowser)
			{
				RefreshContentInfo();
			}
			DrawContent(m_Directories, m_Files, columns);
		}
		m_RefreshBrowser = false;

		ImGui::PopID();

		m_ShowSaveScenePopup = m_ShowSaveScenePopup && m_EditorLayer.GetEditorState() == EditorState::Edit;

		if (m_ShowSaveScenePopup)
		{
			if (const auto& openedScene = m_EditorLayer.GetOpenedSceneAsset(); !openedScene || openedScene->IsDirty())
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
			else
			{
				m_EditorLayer.OpenScene(m_SceneToOpen);
				m_ShowSaveScenePopup = false;
			}
		}

		HandleAssetEditors();
		HandleAddPanel();

		if (m_DrawTextureImporter)
			m_RefreshBrowser |= m_TextureImporter.OnImGuiRender(m_CurrentDirectoryRelative, &m_DrawTextureImporter);

		if (m_DrawMeshImporter)
			m_RefreshBrowser |= m_MeshImporter.OnImGuiRender(m_CurrentDirectoryRelative, &m_DrawMeshImporter);

		if (m_DrawAnimationGraphImporter)
			m_RefreshBrowser |= m_AnimationGraphImporter.OnImGuiRender(m_CurrentDirectoryRelative, &m_DrawAnimationGraphImporter);

		if (m_DrawAnimationBlendSpaceImporter)
			m_RefreshBrowser |= m_AnimationBlendSpaceImporter.OnImGuiRender(m_CurrentDirectoryRelative, &m_DrawAnimationBlendSpaceImporter);

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

	bool ContentBrowserPanel::HandleImport()
	{
		bool bCreatedAsset = false;

		std::vector<Path> paths = FileDialog::OpenFileMultiselect(FileDialog::IMPORT_FILTER);
		if (!paths.empty())
		{
			std::vector<Path> textures;
			std::vector<Path> meshes;
			std::vector<Path> others;
			for (auto& path : paths)
			{
				const AssetType assetType = AssetImporter::GetAssetTypeByExtension(path);
				if (assetType == AssetType::Texture2D || assetType == AssetType::TextureCube)
				{
					textures.emplace_back(std::move(path));
				}
				else if (assetType == AssetType::StaticMesh || assetType == AssetType::SkeletalMesh)
				{
					meshes.emplace_back(std::move(path));
				}
				else
				{
					others.emplace_back(std::move(path));
				}
			}

			if (!textures.empty())
			{
				m_TextureImporter = TextureImporterPanel(textures);
				m_DrawAddPanel = false;
				m_DrawTextureImporter = true;
			}
			if (!meshes.empty())
			{
				m_MeshImporter = MeshImporterPanel(meshes);
				m_DrawAddPanel = false;
				m_DrawMeshImporter = true;
			}

			if (!others.empty())
			{
				AssetImporter::Import(others, m_CurrentDirectoryRelative);
				bCreatedAsset = true;
			}
		}

		return bCreatedAsset;
	}

	void ContentBrowserPanel::HandleDragDropOnFolder(const Path& destinationFolder)
	{
		if (!ImGui::BeginDragDropTarget())
			return;

		magic_enum::enum_for_each<AssetType>([&destinationFolder, this](AssetType assetType)
		{
			if (assetType == AssetType::None)
				return;

			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(GetAssetDragDropCellTag(assetType)))
			{
				const wchar_t* payload_n = (const wchar_t*)payload->Data;
				Path filepath(payload_n);

				if (Path path = OnPasteAsset(filepath, destinationFolder, false); !path.empty())
					SetSelected(path, true);
			}
		});

		ImGui::EndDragDropTarget();
	}

	void ContentBrowserPanel::CloseInputField()
	{
		m_PopupInput.clear();
		m_bShowInputName = false;
		m_InputState = InputNameState::None;
		m_RefreshBrowser = true;
	}

	void ContentBrowserPanel::HandleAssetEditors()
	{
		for (auto it = m_AssetEditors.begin(); it != m_AssetEditors.end(); )
		{
			auto& editor = it->second;
			bool bOpened = true;
			editor->OnImGuiRender(&bOpened);

			if (bOpened == false)
				it = m_AssetEditors.erase(it);
			else
				++it;
		}
	}

	void ContentBrowserPanel::HandleAddPanel()
	{
		constexpr ImVec2 thumbnailSize = ImVec2(ThumbnailCache::GetThumbnailSize().x, ThumbnailCache::GetThumbnailSize().y);

		if (m_DrawAddPanel)
		{
			ImGui::OpenPopup("Add asset");

			// Always center this window when appearing
			ImVec2 center = ImGui::GetMainViewport()->GetCenter();
			ImVec2 size = ImVec2(720.f, 5.66f * thumbnailSize.y);
			ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
			ImGui::SetNextWindowSize(size, ImGuiCond_FirstUseEver);
		}

		if (ImGui::BeginPopupModal("Add asset", &m_DrawAddPanel))
		{
			bool bCreatedAsset = false;

			if (UI::ImageButtonWithTextHorizontal(EditorResources::GetAssetIconTexture(AssetType::None), "Import...", thumbnailSize, thumbnailSize.x))
			{
				bCreatedAsset |= HandleImport();
			}
			UI::Tooltip(s_ImportTooltip);

			ImGui::Separator();

			if (UI::ImageButtonWithTextHorizontal(EditorResources::GetAssetIconTexture(AssetType::Entity), "Entity", thumbnailSize, thumbnailSize.x))
			{
				AssetImporter::CreateEntity(m_CurrentDirectoryRelative);
				bCreatedAsset = true;
			}

			if (UI::ImageButtonWithTextHorizontal(EditorResources::GetAssetIconTexture(AssetType::ParticleSystem), "Particle System", thumbnailSize, thumbnailSize.x))
			{
				AssetImporter::CreateParticleSystem(m_CurrentDirectoryRelative);
				bCreatedAsset = true;
			}

			if (UI::ImageButtonWithTextHorizontal(EditorResources::GetAssetIconTexture(AssetType::Material), "Material", thumbnailSize, thumbnailSize.x))
			{
				AssetImporter::CreateMaterial(m_CurrentDirectoryRelative);
				bCreatedAsset = true;
			}

			if (UI::ImageButtonWithTextHorizontal(EditorResources::GetAssetIconTexture(AssetType::PhysicsMaterial), "Physics Material", thumbnailSize, thumbnailSize.x))
			{
				AssetImporter::CreatePhysicsMaterial(m_CurrentDirectoryRelative);
				bCreatedAsset = true;
			}

			if (UI::ImageButtonWithTextHorizontal(EditorResources::GetAssetIconTexture(AssetType::SoundGroup), "Sound Group", thumbnailSize, thumbnailSize.x))
			{
				AssetImporter::CreateSoundGroup(m_CurrentDirectoryRelative);
				bCreatedAsset = true;
			}

			if (UI::ImageButtonWithTextHorizontal(EditorResources::GetAssetIconTexture(AssetType::AnimationGraph), "Animation Graph", thumbnailSize, thumbnailSize.x))
			{
				m_AnimationGraphImporter = AnimationGraphImporterPanel(m_CurrentDirectoryRelative);
				m_DrawAddPanel = false;
				m_DrawAnimationGraphImporter = true;
			}

			if (UI::ImageButtonWithTextHorizontal(EditorResources::GetAssetIconTexture(AssetType::AnimationBlendSpace), "Animation Blend Space", thumbnailSize, thumbnailSize.x))
			{
				m_AnimationBlendSpaceImporter = AnimationBlendSpaceImporterPanel(m_CurrentDirectoryRelative);
				m_DrawAddPanel = false;
				m_DrawAnimationBlendSpaceImporter = true;
			}

			if (UI::ImageButtonWithTextHorizontal(EditorResources::GetAssetIconTexture(AssetType::BehaviorGraph), "Behavior Graph", thumbnailSize, thumbnailSize.x))
			{
				AssetImporter::CreateBehaviorGraph(m_CurrentDirectoryRelative);
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
		if (e.Handled)
			return;

		for (auto& [_, editor] : m_AssetEditors)
		{
			editor->OnEvent(e);
			if (e.Handled)
				return;
		}

		if (e.GetEventType() == EventType::KeyPressed)
		{
			KeyPressedEvent& key = (KeyPressedEvent&)e;
			if (key.GetKey() == Key::Escape)
			{
				if (m_InputState != InputNameState::None)
				{
					CloseInputField();
					e.Handled |= true;
				}
			}
		}

		if (!m_ContentBrowserHovered)
			return;

		Event::Dispatch<MouseButtonPressedEvent>(e, EG_BIND_FN(ContentBrowserPanel::OnMousePressedEvent));
		Event::Dispatch<KeyPressedEvent>(e, EG_BIND_FN(ContentBrowserPanel::OnKeyPressed));
	}

	void ContentBrowserPanel::OpenAssetEditor(const Ref<Asset>& asset)
	{
		switch (asset->GetAssetType())
		{
		case AssetType::Texture2D:
			AddAssetEditor<Texture2DAssetEditor, AssetTexture2D>(asset);
			break;
		case AssetType::TextureCube:
			AddAssetEditor<TextureCubeAssetEditor, AssetTextureCube>(asset);
			break;
		case AssetType::StaticMesh:
			AddAssetEditor<StaticMeshAssetEditor, AssetStaticMesh>(asset);
			break;
		case AssetType::SkeletalMesh:
			AddAssetEditor<SkeletalMeshAssetEditor, AssetSkeletalMesh>(asset);
			break;
		case AssetType::Audio:
			AddAssetEditor<AudioAssetEditor, AssetAudio>(asset);
			break;
		case AssetType::SoundGroup:
			AddAssetEditor<SoundGroupAssetEditor, AssetSoundGroup>(asset);
			break;
		case AssetType::Font:
			AddAssetEditor<FontAssetEditor, AssetFont>(asset);
			break;
		case AssetType::Material:
			AddAssetEditor<MaterialAssetEditor, AssetMaterial>(asset);
			break;
		case AssetType::PhysicsMaterial:
			AddAssetEditor<PhysicsMaterialAssetEditor, AssetPhysicsMaterial>(asset);
			break;
		case AssetType::Entity:
			AddAssetEditor<EntityAssetEditor, AssetEntity>(asset, m_EditorLayer);
			break;
		case AssetType::Scene:
		{
			m_ShowSaveScenePopup = true;
			m_SceneToOpen = Cast<AssetScene>(asset);
			break;
		}
		case AssetType::Animation:
			AddAssetEditor<AnimationAssetEditor, AssetAnimation>(asset);
			break;
		case AssetType::AnimationGraph:
			AddAssetEditor<AnimationGraphAssetEditor, AssetAnimationGraph>(asset);
			break;
		case AssetType::ParticleSystem:
			AddAssetEditor<ParticleSystemAssetEditor, AssetParticleSystem>(asset);
			break;
		case AssetType::AnimationBlendSpace:
			AddAssetEditor<AnimationBlendSpaceAssetEditor, AssetAnimationBlendSpace>(asset);
			break;
		case AssetType::BehaviorGraph:
			AddAssetEditor<BehaviorGraphAssetEditor, AssetBehaviorGraph>(asset);
			break;
		}
	}

	ContentBrowserPanel& ContentBrowserPanel::Get()
	{
		return *s_Instance;
	}

	void ContentBrowserPanel::DrawContent(const std::vector<Path>& directories, const std::vector<Path>& files, int32_t columns, bool bHintFullPath /* = false */)
	{
		constexpr ImVec2 thumbnailSize = ImVec2(ThumbnailCache::GetThumbnailSize().x, ThumbnailCache::GetThumbnailSize().y);
		bool bHoveredAnyItem = false;

		ImGui::BeginChild("##scrollable_cb");

		DrawContentBrowserPopupMenu();

		if (columns > 1)
		{
			ImGui::Columns(columns, nullptr, false);
			ImGui::SetColumnWidth(0, m_ColumnWidth);
		}

		ImGui::PushID("DIRECTORIES_FILL");
		for (auto& dir : directories)
		{
			const auto& path = dir;
			std::string pathString = path.u8string();
			std::string filename = path.filename().u8string();

			{
				const bool bSelected = m_SelectedFile == path;
				const bool bFillBg = bSelected;
				if (bFillBg)
					UI::PushButtonSelectedStyleColors();

				UI::ImageButtonWithText(m_FolderIcon, filename, thumbnailSize, bFillBg);

				if (bSelected && m_ScrollToSelected)
				{
					ImGui::SetScrollHereY(0);
					m_ScrollToSelected = false;
				}

				if (bFillBg)
					UI::PopButtonSelectedStyleColors();
			}
			
			DrawItemPopupMenu(path);
			HandleDragDropOnFolder(path);

			bHoveredAnyItem |= ImGui::IsItemHovered();
			if (ImGui::IsItemClicked())
			{
				if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
				{
					auto prevDir = m_CurrentDirectory;
					m_CurrentDirectory = path;
					m_CurrentDirectoryRelative = std::filesystem::relative(m_CurrentDirectory, m_ProjectPath);
					SetSelected("", false);
					OnDirectoryOpened(prevDir);
				}
				else
				{
					SetSelected(path, false);
				}
			}

			bHoveredAnyItem |= ImGui::IsItemHovered();
			UI::Tooltip(bHintFullPath ? pathString : filename);
			if (columns > 1)
			{
				ImGui::NextColumn();
				ImGui::SetColumnWidth(-1, m_ColumnWidth);
			}
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

			Ref<Image> image;
			Ref<Sampler> sampler = Sampler::BilinearSampler;
			const AssetType assetType = asset->GetAssetType();

			if (assetType == AssetType::Texture2D)
			{
				const auto& texture = Cast<AssetTexture2D>(asset)->GetTexture();
				image = texture->GetImage();
				sampler = texture->GetSampler();
			}
			else if (assetType == AssetType::TextureCube)
			{
				const auto& texture = Cast<AssetTextureCube>(asset)->GetTexture()->GetTexture2D();
				image = texture->GetImage();
				sampler = texture->GetSampler();
			}
			else if (ThumbnailCache::IsRenderableAssetType(assetType))
			{
				image = ThumbnailCache::Get(asset);
			}

			if (!image)
				image = EditorResources::GetAssetIconTexture(assetType)->GetImage();

			bool bClicked = false;
			ImVec2 p = ImGui::GetCursorScreenPos();

			{
				const bool bSelected = m_SelectedFile == path;
				const bool bFillBg = bSelected;
				if (bFillBg)
					UI::PushButtonSelectedStyleColors();

				ImVec4 borderColor;
				const bool bBorderColor = GetAssetBorderColor(assetType, borderColor);
				if (bBorderColor)
					ImGui::PushStyleColor(ImGuiCol_Border, borderColor);

				UI::ImageButtonWithText(image, sampler, filename, thumbnailSize, bFillBg, 2.0f);

				if (bSelected && m_ScrollToSelected)
				{
					ImGui::SetScrollHereY(0);
					m_ScrollToSelected = false;
				}

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

				constexpr ImVec2 asteriskDrawOffset = ImVec2(36.f, 36.f);
				UI::AddImage(m_AsteriskIcon, p + thumbnailSize - asteriskDrawOffset, p + thumbnailSize, ImVec2(0, 0), ImVec2(1, 1), color);
			}
			DrawItemPopupMenu(path);

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
					SetSelected(path, false);
			}
			bClicked = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && bClicked;

			// Open asset editor
			if (bClicked)
			{
				OpenAssetEditor(asset);
			}

			bHoveredAnyItem |= ImGui::IsItemHovered();
			{
				std::string tooltip = std::string("Asset Type: ") + Utils::GetEnumName(assetType) + '\n';
				tooltip += bHintFullPath ? pathString : filename;
				UI::Tooltip(tooltip);
			}
			if (columns > 1)
			{
				ImGui::NextColumn();
				ImGui::SetColumnWidth(-1, m_ColumnWidth);
			}
		}
		ImGui::PopID();

		if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && !bHoveredAnyItem)
		{
			SetSelected("", false);
		}

		ImGui::Columns(1);

		ImGui::EndChild();
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
		static std::vector<Path> paths;
		paths.clear();
		paths.push_back(temp); //Current dir
		while (!temp.empty()) //Saving all dir names separatly in vector
		{
			auto parent = temp.parent_path();
			if (parent.empty())
				break;
			paths.push_back(parent);
			temp = temp.parent_path();
		}

		temp = m_ProjectPath;

		for (auto it = paths.rbegin(); it != paths.rend(); ++it) //Drawing buttons
		{
			Path filename = it->filename();
			temp /= filename;
			if (ImGui::Button(filename.u8string().c_str()))
			{
				SetSelected("", false);
				auto prevPath = m_CurrentDirectory;
				m_CurrentDirectory = temp;
				m_CurrentDirectoryRelative = std::filesystem::relative(m_CurrentDirectory, m_ProjectPath);
				OnDirectoryOpened(prevPath);
			}

			if ((*it) != m_CurrentDirectory) // Don't move to the current dir
				HandleDragDropOnFolder(*it);

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
			const std::string filename = path.stem().u8string();

			std::size_t pos = Utils::FindSubstringI(filename, search);
			if (pos != std::string::npos)
			{
				outFiles.push_back(std::filesystem::relative(path, projectPath));
			}
		}
	}

	void ContentBrowserPanel::DrawItemPopupMenu(const Path& path, int timesCalledForASinglePath)
	{
		static bool bDoneOnce = false;
		const std::string pathString = path.u8string();
		if (ImGui::BeginPopupContextItem(pathString.c_str()))
		{
			if (!bDoneOnce)
			{
				SetSelected(path, false);
				bDoneOnce = true;
			}

			if (ImGui::MenuItem("Show In Explorer"))
				Utils::ShowInExplorer(path);

			const bool bDirectory = std::filesystem::is_directory(path);

			if (bDirectory)
			{
				ImGui::Separator();
				if (ImGui::MenuItem("Delete"))
					OnDeleteFolder(path);
			}
			else
			{
				if (ImGui::MenuItem("Show In Folder View"))
					NavigateToFile(path);

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

						if (ImGui::MenuItem("Duplicate"))
							DuplicateAsset(asset);
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

	void ContentBrowserPanel::DrawContentBrowserPopupMenu()
	{
		if (ImGui::BeginPopupContextWindow("ContentBrowserPopup", ImGuiPopupFlags_MouseButtonRight))
		{
			if (ImGui::MenuItem("Import..."))
			{
				if (HandleImport())
					m_RefreshBrowser = true;
			}

			if (ImGui::BeginMenu("Create"))
			{
				if (ImGui::MenuItem("Entity"))
					SetSelected(AssetImporter::CreateEntity(m_CurrentDirectoryRelative), true);
				if (ImGui::MenuItem("Material"))
					SetSelected(AssetImporter::CreateMaterial(m_CurrentDirectoryRelative), true);
				if (ImGui::MenuItem("Physics Material"))
					SetSelected(AssetImporter::CreatePhysicsMaterial(m_CurrentDirectoryRelative), true);
				if (ImGui::MenuItem("Sound Group"))
					SetSelected(AssetImporter::CreateSoundGroup(m_CurrentDirectoryRelative), true);
				if (ImGui::MenuItem("Particle System"))
					SetSelected(AssetImporter::CreateParticleSystem(m_CurrentDirectoryRelative), true);
				if (ImGui::MenuItem("Animation Graph"))
				{
					m_AnimationGraphImporter = AnimationGraphImporterPanel(m_CurrentDirectoryRelative);
					m_DrawAnimationGraphImporter = true;
				}
				if (ImGui::MenuItem("Animation Blend Space"))
				{
					m_AnimationBlendSpaceImporter = AnimationBlendSpaceImporterPanel(m_CurrentDirectoryRelative);
					m_DrawAnimationBlendSpaceImporter = true;
				}
				if (ImGui::MenuItem("Behavior Graph"))
				{
					SetSelected(AssetImporter::CreateBehaviorGraph(m_CurrentDirectoryRelative), true);
				}
				if (ImGui::MenuItem("Scene"))
				{
					SetSelected(AssetImporter::CreateScene(m_CurrentDirectoryRelative), true);
				}

				ImGui::EndMenu();
			}

			ImGui::Separator();

			if (ImGui::MenuItem("New Folder"))
			{
				m_bShowInputName = true;
				m_InputState = InputNameState::NewFolder;
			}

			ImGui::Separator();

			const bool bDisablePaste = m_CopiedPath.empty();
			if (bDisablePaste)
				UI::PushItemDisabled();

			if (ImGui::MenuItem("Paste"))
			{
				if (Path path = OnPasteAsset(m_CopiedPath, m_CurrentDirectoryRelative, m_bCopy); !path.empty())
				{
					SetSelected(path, true);
				}
				m_CopiedPath.clear();
			}

			if (bDisablePaste)
				UI::PopItemDisabled();

			ImGui::EndPopup();
		}
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
			m_RefreshBrowser = true;
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
			m_RefreshBrowser = true;
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
		m_PopupInput = asset ? asset->GetPath().filename().stem().u8string() : "";
	}

	void ContentBrowserPanel::OnDeleteAsset(const Ref<Asset>& asset)
	{
		m_ShowDeleteConfirmation = true;
		m_AssetToDelete = asset;
		m_DeleteConfirmationMessage = "Are you sure you want to delete " + m_AssetToDelete->GetPath().stem().u8string()
			+ "?\nDeleting it won't remove it from the current scene,\nbut the next time it's opened, it will be replaced with an empty asset";
	}

	void ContentBrowserPanel::OnDeleteFolder(const Path& path)
	{
		m_ShowDeleteConfirmation = true;
		m_FolderToDelete = path;
		m_DeleteConfirmationMessage = "Are you sure you want to delete this folder and all of its content?\nFolder: " + m_FolderToDelete.u8string();
	}

	void ContentBrowserPanel::DuplicateAsset(const Ref<Asset>& asset)
	{
		const auto& path = asset->GetPath();
		Path newFilepath = Utils::GetUniqueAssetFilepath(path.parent_path(), path.stem().u8string());
		if (!AssetManager::Duplicate(asset, newFilepath))
		{
			Application::Get().GetImGuiLayer()->AddMessage("Duplicate failed. See logs for more details");
		}
		else
		{
			m_RefreshBrowser = true;
		}
	}

	void ContentBrowserPanel::OnSaveAsset(const Ref<Asset>& asset)
	{
		if (asset->GetAssetType() == AssetType::Scene)
		{
			if (m_EditorLayer.GetOpenedSceneAsset() == asset)
			{
				m_EditorLayer.SaveScene();
				asset->SetDirty(false);
			}
		}
		else
			Asset::Save(asset);
	}

	// By user clicking in UI
	void ContentBrowserPanel::OnDirectoryOpened(const Path& previousPath)
	{
		m_BackHistory.push_back(previousPath);
		m_ForwardHistory.clear();
		m_RefreshBrowser = true;
	}

	void ContentBrowserPanel::NavigateToFile(const Path& path)
	{
		m_Search.clear();
		SetSelected(path, true);
		m_CurrentDirectory = path.parent_path();
		m_CurrentDirectoryRelative = std::filesystem::relative(m_CurrentDirectory, m_ProjectPath);
	}

	void ContentBrowserPanel::SetSelected(const Path& path, bool bScrollToIt)
	{
		m_SelectedFile = path;
		m_ScrollToSelected = bScrollToIt;
	}

	bool ContentBrowserPanel::OnKeyPressed(KeyPressedEvent& e)
	{
		KeyPressedEvent& keyEvent = (KeyPressedEvent&)e;
		const Key pressedKey = keyEvent.GetKey();
		const bool control = Input::IsKeyPressed(Key::LeftControl) || Input::IsKeyPressed(Key::RightControl);
		bool bHandled = false;

		if (!std::filesystem::is_directory(m_SelectedFile))
		{
			Ref<Asset> asset;
			if (AssetManager::Get(m_SelectedFile, &asset))
			{
				//Shortcuts
				if (keyEvent.GetRepeatCount() > 0)
					return false;

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
				case Key::W:
					DuplicateAsset(asset);
					bHandled = true;
					break;
				}
			}
		}
		else
		{
			// Shortcuts
			if (keyEvent.GetRepeatCount() == 0)
			{
				switch (pressedKey)
				{
				case Key::Delete:
					OnDeleteFolder(m_SelectedFile);
					bHandled = true;
					break;
				}
			}
		}

		if (pressedKey == Key::V && control)
		{
			if (Path path = OnPasteAsset(m_CopiedPath, m_CurrentDirectoryRelative, m_bCopy); !path.empty())
				m_SelectedFile = path;
			m_CopiedPath.clear();
			bHandled = true;
		}

		return bHandled;
	}

	bool ContentBrowserPanel::OnMousePressedEvent(MouseButtonPressedEvent& e)
	{
		Mouse button = e.GetMouseCode();
		if (button == Mouse::Button3)
		{
			GoBack();
			return true;
		}
		else if (button == Mouse::Button4)
		{
			GoForward();
			return true;
		}

		return false;
	}
}
