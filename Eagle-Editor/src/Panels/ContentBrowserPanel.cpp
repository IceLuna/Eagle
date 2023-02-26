#include "egpch.h"
#include "ContentBrowserPanel.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include "Eagle/Utils/PlatformUtils.h"
#include "../EditorLayer.h"
#include "Eagle/UI/UI.h"
#include "Eagle/Core/Project.h"

#include <locale>

// templated version of my_equal so it could work with both char and wchar_t
template<typename charT>
struct my_equal {
	my_equal(const std::locale& loc) : loc_(loc) {}
	bool operator()(charT ch1, charT ch2) {
		return std::toupper(ch1, loc_) == std::toupper(ch2, loc_);
	}
private:
	const std::locale& loc_;
};

// find substring (case insensitive)
template<typename T>
static int MyFindStr(const T& str1, const T& str2, const std::locale& loc = std::locale("RU_ru"))
{
	typename T::const_iterator it = std::search(str1.begin(), str1.end(),
		str2.begin(), str2.end(), my_equal<typename T::value_type>(loc));
	if (it != str1.end()) return int(it - str1.begin());
	else return -1; // not found
}

namespace Eagle
{
	static const Path s_ProjectPath = Project::GetProjectPath();
	static const Path s_ContentDirectory = Project::GetContentPath();

	char ContentBrowserPanel::searchBuffer[searchBufferSize];

	ContentBrowserPanel::ContentBrowserPanel(EditorLayer& editorLayer)
		: m_CurrentDirectory(s_ContentDirectory)
		, m_EditorLayer(editorLayer)
	{}

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
			UI::TextureViewer::OpenTextureViewer(textureToView, &m_ShowTextureView);

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
			if (button == 3)
				GoBack();
			else if (button == 4)
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

			UI::Image(Texture2D::FolderIconTexture, { 64, 64 });
			
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
			if (ImGui::IsItemHovered() && GImGui->HoveredIdTimer > EG_HOVER_THRESHOLD)
			{
				ImGui::BeginTooltip();
				ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
				if (bHintFullPath)
					ImGui::TextUnformatted(pathString.c_str());
				else
					ImGui::TextUnformatted(filename.c_str());
				ImGui::PopTextWrapPos();
				ImGui::EndTooltip();
			}
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

			if (fileFormat == Utils::FileFormat::TEXTURE)
			{
				if (!TextureLibrary::Get(path, &texture))
					texture = GetFileIconTexture(fileFormat); // If didn't find a texture, use texture icon
			}
			else if (fileFormat == Utils::FileFormat::TEXTURE_CUBE)
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
				if (fileFormat == Utils::FileFormat::SCENE)
				{
					m_ShowSaveScenePopup = true;
					m_PathOfSceneToOpen = path;
				}
				else if (fileFormat == Utils::FileFormat::TEXTURE)
				{
					Ref<Texture> texture;
					if (TextureLibrary::Get(path, &texture) == false)
						texture = Texture2D::Create(path);

					textureToView = texture;
					m_ShowTextureView = true;
				}
				else if (fileFormat == Utils::FileFormat::TEXTURE_CUBE)
				{
					Ref<Texture> texture;
					if (TextureLibrary::Get(path, &texture) == false)
						texture = TextureCube::Create(path, TextureCube::SkyboxSize)->GetTexture2D();
					else
						texture = Cast<TextureCube>(texture)->GetTexture2D();

					textureToView = texture;
					m_ShowTextureView = true;
				}
			}
			DrawPopupMenu(path, 1);
			if (bChangeColor)
				ImGui::PopStyleColor(3);

			bHoveredAnyItem |= ImGui::IsItemHovered();
			//Show hint if file is hovered long enough
			if (ImGui::IsItemHovered() && GImGui->HoveredIdTimer > EG_HOVER_THRESHOLD)
			{
				ImGui::BeginTooltip();
				ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
				if (bHintFullPath)
					ImGui::TextUnformatted(pathString.c_str());
				else
					ImGui::TextUnformatted(filename.c_str());
				ImGui::PopTextWrapPos();
				ImGui::EndTooltip();
			}
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

			int pos = MyFindStr(filename, search);

			if (pos != -1)
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

	//By user clicking in UI
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
			case Utils::FileFormat::TEXTURE:
			case Utils::FileFormat::TEXTURE_CUBE:
				return Texture2D::TextureIconTexture;
			case Utils::FileFormat::MESH:
				return Texture2D::MeshIconTexture;
			case Utils::FileFormat::SCENE:
				return Texture2D::SceneIconTexture;
			case Utils::FileFormat::SOUND:
				return Texture2D::SoundIconTexture;
			default:
				return Texture2D::UnknownIconTexture;
		}
	}
}
