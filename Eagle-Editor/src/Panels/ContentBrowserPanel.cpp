#include "egpch.h"
#include "ContentBrowserPanel.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include "Eagle/Utils/PlatformUtils.h"
#include "../EditorLayer.h"
#include "Eagle/UI/UI.h"
#include "Eagle/Core/Project.h"

namespace Eagle
{
	static const Path s_ProjectPath = Project::GetProjectPath();
	static const Path s_ContentDirectory = Project::GetContentPath();

	char ContentBrowserPanel::searchBuffer[searchBufferSize];

	ContentBrowserPanel::ContentBrowserPanel(EditorLayer& editorLayer)
		: m_CurrentDirectory(s_ContentDirectory)
		, m_EditorLayer(editorLayer)
	{
		m_MeshIcon = Texture2D::Create("assets/textures/Editor/meshicon.png", {}, false);
		m_TextureIcon = Texture2D::Create("assets/textures/Editor/textureicon.png", {}, false);
		m_SceneIcon = Texture2D::Create("assets/textures/Editor/sceneicon.png", {}, false);
		m_SoundIcon = Texture2D::Create("assets/textures/Editor/soundicon.png", {}, false);
		m_UnknownIcon = Texture2D::Create("assets/textures/Editor/unknownicon.png", {}, false);
		m_FolderIcon = Texture2D::Create("assets/textures/Editor/foldericon.png", {}, false);
		m_FontIcon = Texture2D::Create("assets/textures/Editor/fonticon.png", {}, false);
	}

	void ContentBrowserPanel::OnImGuiRender()
	{
		static bool bRenderingFirstTime = true;
		static bool bShowInputFolderName = false;

		ImGui::Begin("Content Browser");
		ImGui::PushID("Content Browser");
		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
		ImGui::InputTextWithHint("##search", "Search...", searchBuffer, searchBufferSize);

		if (ImGui::BeginPopupContextWindow("ContentBrowserPopup", ImGuiPopupFlags_MouseButtonRight))
		{
			if (ImGui::MenuItem("Create folder"))
			{
				bShowInputFolderName = true;
			}

			ImGui::EndPopup();
		}
		if (bShowInputFolderName)
		{
			static std::string input;
			UI::ButtonType pressedButton = UI::InputPopup("Eagle-Editor", "Folder name", input);
			if (pressedButton != UI::ButtonType::None)
			{
				if (pressedButton == UI::ButtonType::OK)
				{
					const char* buf_end = NULL;
					ImWchar* wData = new ImWchar[input.size()];
					ImTextStrFromUtf8(wData, sizeof(wData), input.c_str(), NULL, &buf_end);
					std::u16string tempu16((const char16_t*)wData);
					Path temp = tempu16;
					delete[] wData;

					Path newPath = m_CurrentDirectory / temp;
					std::filesystem::create_directory(newPath);
				}
				input = "";
				bShowInputFolderName = false;
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

			for (auto& dir : std::filesystem::directory_iterator(m_CurrentDirectory))
			{
				const auto& path = dir.path();

				if (dir.is_directory())
					m_Directories.push_back(path);
				else
					m_Files.push_back(path);
			}
		}

		ImGui::Columns(1);
		ImGui::PopID();

		m_ShowSaveScenePopup = m_ShowSaveScenePopup && m_EditorLayer.GetEditorState() == EditorState::Edit;

		if (m_ShowSaveScenePopup)
		{
			UI::ButtonType result = UI::ShowMessage("Eagle-Editor", "Do you want to save current scene?", UI::ButtonType::YesNoCancel);
			if (result == UI::ButtonType::Yes)
			{
				m_EditorLayer.SaveScene();
				m_EditorLayer.OpenScene(m_PathOfSceneToOpen);
				m_ShowSaveScenePopup = false;
			}
			else if (result == UI::ButtonType::No)
			{
				m_EditorLayer.OpenScene(m_PathOfSceneToOpen);
				m_ShowSaveScenePopup = false;
			}
			else if (result == UI::ButtonType::Cancel)
				m_ShowSaveScenePopup = false;
		}

		if (m_ShowTextureView)
		{
			if (auto texture2D = Cast<Texture2D>(m_TextureToView))
				UI::TextureViewer::OpenTextureViewer(texture2D, &m_ShowTextureView);
			else if (auto cubeTexture = Cast<TextureCube>(m_TextureToView))
				UI::TextureViewer::OpenTextureViewer(cubeTexture, &m_ShowTextureView);
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
			if (button == Button3)
				GoBack();
			else if (button == Button4)
				GoForward();
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
			std::string filename = path.filename().u8string();

			Utils::FileFormat fileFormat = Utils::GetSupportedFileFormat(path);
			Ref<Texture> texture;

			if (fileFormat == Utils::FileFormat::Texture)
			{
				if (!TextureLibrary::Get(path, &texture))
					texture = GetFileIconTexture(fileFormat); // If didn't find a texture, use texture icon
			}
			else if (fileFormat == Utils::FileFormat::TextureCube)
			{
				if (TextureLibrary::Get(path, &texture))
					texture = Cast<TextureCube>(texture)->GetTexture2D(); // Get cube's 2D representation
				else
					texture = GetFileIconTexture(fileFormat); // If didn't find a texture, use texture icon
			}
			else
				texture = GetFileIconTexture(fileFormat);

			bool bClicked = false;
			UI::Image(Cast<Texture2D>(texture), { 64, 64 }, { 0, 0 }, { 1, 1 }, m_SelectedFile == path ? ImVec4{ 0.75, 0.75, 0.75, 1.0 } : ImVec4{ 1, 1, 1, 1 });
			DrawPopupMenu(path);

			//Handling Drag Event.
			if (IsDraggableFileFormat(fileFormat))
			{
				if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
				{
					const char* cellTag = GetDragCellTag(fileFormat);
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
				if (fileFormat == Utils::FileFormat::Scene)
				{
					m_ShowSaveScenePopup = true;
					m_PathOfSceneToOpen = path;
				}
				else if (fileFormat == Utils::FileFormat::Texture)
				{
					Ref<Texture> texture;
					if (TextureLibrary::Get(path, &texture) == false)
						texture = Texture2D::Create(path);

					m_TextureToView = texture;
					m_ShowTextureView = true;
				}
				else if (fileFormat == Utils::FileFormat::TextureCube)
				{
					Ref<Texture> texture;
					if (TextureLibrary::Get(path, &texture) == false)
						m_TextureToView = TextureCube::Create(path, TextureCube::SkyboxSize);
					else
						m_TextureToView = texture;

					m_ShowTextureView = true;
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

		std::string contentPath = m_CurrentDirectory.u8string();
		contentPath = contentPath.substr(contentPath.find("Content"));
		Path temp = contentPath;
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

		temp.clear(); //Clearing to reuse in loop.
		temp = s_ProjectPath;

		for (auto it = paths.rbegin(); it != paths.rend(); ++it) //Drawing buttons
		{
			temp /= (*it);
			std::string filename = (*it).u8string();
			if (ImGui::Button(filename.c_str()))
			{
				m_SelectedFile.clear();
				auto prevPath = m_CurrentDirectory;
				m_CurrentDirectory = temp;
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
		for (auto& dirEntry : std::filesystem::recursive_directory_iterator(m_CurrentDirectory))
		{
			if (dirEntry.is_directory())
				continue;

			Path path = dirEntry.path();
			std::string filename = path.filename().u8string();

			std::size_t pos = Utils::FindSubstringI(filename, search);
			if (pos != std::string::npos)
			{
				outFiles.push_back(path);
			}
		}
	}

	void ContentBrowserPanel::DrawPopupMenu(const Path& path, int timesCalledForASinglePath)
	{
		static bool bDoneOnce = false;
		std::string pathString = path.u8string();
		pathString += std::to_string(timesCalledForASinglePath);
		if (ImGui::BeginPopupContextItem(pathString.c_str()))
		{
			if (!bDoneOnce)
			{
				m_SelectedFile = path;
				bDoneOnce = true;
			}

			if (!std::filesystem::is_directory(path))
			{
				if (ImGui::MenuItem("Show In Folder View"))
					SelectFile(path);
			}

			if (ImGui::MenuItem("Show In Explorer"))
				Utils::ShowInExplorer(path);
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
			m_ForwardHistory.pop_back();
		}
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
	}
	
	Ref<Texture2D>& ContentBrowserPanel::GetFileIconTexture(const Utils::FileFormat& fileFormat)
	{
		switch (fileFormat)
		{
			case Utils::FileFormat::Texture:
			case Utils::FileFormat::TextureCube:
				return m_TextureIcon;
			case Utils::FileFormat::Mesh:
				return m_MeshIcon;
			case Utils::FileFormat::Scene:
				return m_SceneIcon;
			case Utils::FileFormat::Sound:
				return m_SoundIcon;
			case Utils::FileFormat::Font:
				return m_FontIcon;
			default:
				return m_UnknownIcon;
		}
	}
}
