#include "egpch.h"
#include "ContentBrowserPanel.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include "Eagle/Utils/Utils.h"
#include "Eagle/Utils/PlatformUtils.h"
#include "../EditorLayer.h"
#include "Eagle/UI/UI.h"

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
	static const std::filesystem::path s_ContentDirectory("assets");

	ContentBrowserPanel::ContentBrowserPanel(EditorLayer& editorLayer) 
	: m_CurrentDirectory(s_ContentDirectory)
	, m_EditorLayer(editorLayer)
	{}

	void ContentBrowserPanel::OnImGuiRender()
	{
		constexpr int searchBufferSize = 512;
		static char searchBuffer[searchBufferSize] = "";
		static bool bRenderingFirstTime = true;

		ImGui::Begin("Content Browser");
		ImGui::PushID("Content Browser");
		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvailWidth());
		ImGui::InputTextWithHint("##search", "Search...", searchBuffer, searchBufferSize);

		ImVec2 size = ImGui::GetContentRegionAvail();
		constexpr int columnWidth = 72;
		int columns = (int)size[0] / columnWidth;
		bContentBrowserHovered = ImGui::IsWindowHovered();

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
		std::filesystem::path temp = tempu16;
		std::string search = temp.string();
		if (search.length())
		{
			std::vector<std::filesystem::path> directories;
			if (bContentBrowserHovered)
			{
				m_SearchFiles.clear();
				GetSearchingContent(search, m_SearchFiles);
			}
			DrawContent(directories, m_SearchFiles, true);
		}
		else
		{
			DrawContent(m_Directories, m_Files);
		}

		if (bContentBrowserHovered || bRenderingFirstTime)
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

		if (bShowSaveScenePopup)
		{
			UI::Button result = UI::ShowMessage("Eagle-Editor", "Do you want to save current scene?", UI::Button::YesNoCancel);
			if (result == UI::Button::Yes)
			{
				m_EditorLayer.SaveScene();
				m_EditorLayer.OpenScene(pathOfSceneToOpen);
				bShowSaveScenePopup = false;
			}
			else if (result == UI::Button::No)
			{
				m_EditorLayer.OpenScene(pathOfSceneToOpen);
				bShowSaveScenePopup = false;
			}
			else if (result == UI::Button::Cancel)
				bShowSaveScenePopup = false;
		}

		if (bShowTextureView)
		{
			bool bHidden = !ImGui::Begin("Texture Viewer", &bShowTextureView);
			static bool detailsDocked = false;
			static bool bDetailsVisible; 
			bDetailsVisible = (!bHidden) || (bHidden && !detailsDocked);
			ImVec2 availSize = ImGui::GetContentRegionAvail();
			glm::vec2 textureSize = textureToView->GetSize();

			const float tRatio = textureSize[0] / textureSize[1];
			const float wRatio = availSize[0] / availSize[1];

			textureSize = wRatio > tRatio ? glm::vec2{ textureSize[0] * availSize[1] / textureSize[1], availSize[1] }
										  : glm::vec2{ availSize[0], textureSize[1] * availSize[0] / textureSize[0] };

			ImGui::Image((void*)(uint64_t)textureToView->GetNonSRGBRendererID(), { textureSize[0], textureSize[1] }, { 0, 1 }, { 1, 0 });

			if (bDetailsVisible)
			{
				ImGui::Begin("Details");
				detailsDocked = ImGui::IsWindowDocked();
				ImGui::Text("Name: %s", textureToView->GetPath().filename().string().c_str());
				ImGui::Text("Resolution: %dx%d", (int)textureSize[0], (int)textureSize[1]);
				bool bSRGB = textureToView->IsSRGB();
				if (ImGui::Checkbox("sRGB", &bSRGB))
					textureToView->SetSRGB(bSRGB);
				ImGui::SameLine();
				UI::HelpMarker("Most of the times 'sRGB' needs to be checked for diffuse textures and unchecked for other texture types.");
				ImGui::End();
			}
			
			ImGui::End();
		}

		ImGui::End();
	}

	void ContentBrowserPanel::OnEvent(Event& e)
	{
		if(!bContentBrowserHovered)
			return;

		if (e.GetEventType() == EventType::MouseButtonPressed)
		{
			MouseButtonEvent& mbEvent = (MouseButtonEvent&)e;
			Mouse::MouseButton button = mbEvent.GetMouseCode();
			if (button == 3)
				GoBack();
			else if (button == 4)
				GoForward();
		}
	}

	void ContentBrowserPanel::DrawContent(const std::vector<std::filesystem::path>& directories, const std::vector<std::filesystem::path>& files, bool bHintFullPath /* = false */)
	{
		bool bHoveredAnyItem = false;

		ImGui::PushID("DIRECTORIES_FILL");
		for (auto& dir : directories)
		{
			const auto& path = dir;
			std::string pathString = path.u8string();
			std::string filename = path.filename().u8string();

			ImGui::Image((void*)(uint64_t)Texture2D::FolderIconTexture->GetRendererID(), { 64, 64 }, { 0, 1 }, { 1, 0 }, m_SelectedFile == path ? ImVec4{0.75, 0.75, 0.75, 1.0} : ImVec4{ 1, 1, 1, 1 });
			//ImGuiStyle& style = ImGui::GetStyle();
			//auto oldcolor = style.Colors[21];
			//style.Colors[21] = ImVec4{0, 0, 0, 0};
			//ImGui::ImageButton((void*)(uint64_t)Texture2D::FolderIconTexture->GetRendererID(), { 64, 64 }, { 0, 1 }, { 1, 0 }, 0);
			//style.Colors[21] = oldcolor;
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

			uint32_t rendererID;
			Utils::FileFormat fileFormat = Utils::GetFileFormat(path);
			switch (fileFormat)
			{
			case Utils::FileFormat::MESH:
				rendererID = Texture2D::MeshIconTexture->GetRendererID();
				break;
			case Utils::FileFormat::TEXTURE:
				rendererID = Texture2D::TextureIconTexture->GetRendererID();
				break;
			case Utils::FileFormat::SCENE:
				rendererID = Texture2D::SceneIconTexture->GetRendererID();
				break;
			default:
				rendererID = Texture2D::UnknownIconTexture->GetRendererID();
			}
			bool bClicked = false;
			ImGui::Image((void*)(uint64_t)rendererID, { 64, 64 }, { 0, 1 }, { 1, 0 }, m_SelectedFile == path ? ImVec4{ 0.75, 0.75, 0.75, 1.0 } : ImVec4{ 1, 1, 1, 1 });
			DrawPopupMenu(path);

			if (fileFormat == Utils::FileFormat::TEXTURE || fileFormat == Utils::FileFormat::MESH)
			{
				if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
				{
					const char* cell = fileFormat == Utils::FileFormat::TEXTURE ? "TEXTURE_CELL" : "MESH_CELL";
					std::wstring wide = path.wstring();
					const wchar_t* tt = wide.c_str();
					ImGui::SetDragDropPayload(cell, tt, (wide.size() + 1) * sizeof(wchar_t));
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

			bool bChangeColor = m_SelectedFile == path;
			if (bChangeColor)
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.8f, 0.1f, 0.15f, 1.f });
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.9f, 0.2f, 0.2f, 1.f });
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.8f, 0.1f, 0.15f, 1.f });
			}

			if (ImGui::Button(filename.c_str(), { 64, 22 }) || bClicked)
			{
				if (fileFormat == Utils::FileFormat::SCENE)
				{
					bShowSaveScenePopup = true;
					pathOfSceneToOpen = path;
				}
				else if (fileFormat == Utils::FileFormat::TEXTURE)
				{
					Ref<Texture> texture;
					if (TextureLibrary::Get(path, &texture) == false)
						texture = Texture2D::Create(path);

					textureToView = texture;
					bShowTextureView = true;
				}
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

		std::filesystem::path temp = m_CurrentDirectory;
		std::vector<std::filesystem::path> paths;
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
		for (auto it = paths.rbegin(); it != paths.rend(); ++it) //Drawing buttons
		{
			temp /= *it;
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
	
	void ContentBrowserPanel::GetSearchingContent(const std::string& search, std::vector<std::filesystem::path>& outFiles)
	{
		for (auto& dirEntry : std::filesystem::recursive_directory_iterator(m_CurrentDirectory))
		{
			if (dirEntry.is_directory())
				continue;

			std::filesystem::path path = dirEntry.path();
			std::string filename = path.filename().string();

			int pos = MyFindStr(filename, search);

			if (pos != -1)
			{
				outFiles.push_back(path);
			}
		}
	}
	
	void ContentBrowserPanel::DrawPopupMenu(const std::filesystem::path& path, int timesCalledForASinglePath)
	{
		std::string pathString = path.u8string();
		pathString += std::to_string(timesCalledForASinglePath);
		if (ImGui::BeginPopupContextItem(pathString.c_str()))
		{
			if (ImGui::MenuItem("Show In Explorer"))
			{
				Utils::ShowInExplorer(path);
			}
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
	void ContentBrowserPanel::OnDirectoryOpened(const std::filesystem::path& previousPath)
	{
		m_BackHistory.push_back(previousPath);
		m_ForwardHistory.clear();
	}
}
