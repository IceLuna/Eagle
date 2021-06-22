#include "egpch.h"
#include "ContentBrowserPanel.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include "Eagle/Renderer/Texture.h"
#include "Eagle/Utils/Utils.h"
#include "../EditorLayer.h"
#include "Eagle/UI/UI.h"

namespace Eagle
{
	static const std::filesystem::path s_ContentDirectory("assets");

	ContentBrowserPanel::ContentBrowserPanel(EditorLayer& editorLayer) 
	: m_CurrentDirectory(s_ContentDirectory)
	, m_EditorLayer(editorLayer)
	{}

	void ContentBrowserPanel::OnImGuiRender()
	{
		static Ref<Texture> textureToView;
		static std::string pathOfSceneToOpen;
		static bool bShowSaveScenePopup = false;
		static bool bShowTextureView = false;
		static bool bRenderingFirstTime = true;

		ImGui::Begin("Content Browser");
		ImGui::PushID("Content Browser");

		ImVec2 size = ImGui::GetContentRegionAvail();
		constexpr int columnWidth = 72;
		int columns = (int)size[0] / columnWidth;
		bool bWindowHovered = ImGui::IsWindowHovered();
		bool bHoveredAnyItem = false;

		//Drawing Path-History buttons on top.
		{
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
					m_CurrentDirectory = temp;
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
		
		if (columns > 1)
		{
			ImGui::Columns(columns, nullptr, false);
			ImGui::SetColumnWidth(0, columnWidth);
		}

		for (auto& dir : m_Directories)
		{
			const auto& path = dir;
			std::string pathString = path.u8string();
			std::string filename = path.filename().u8string();

			ImGui::Image((void*)(uint64_t)Texture2D::FolderIconTexture->GetRendererID(), { 64, 64 }, { 0, 1 }, { 1, 0 });

			bHoveredAnyItem |= ImGui::IsItemHovered();
			if (ImGui::IsItemClicked())
			{
				if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
				{
					m_CurrentDirectory = path;
					m_SelectedFile.clear();
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
				m_CurrentDirectory = path;
				m_SelectedFile.clear();
			}
			if (bChangeColor)
				ImGui::PopStyleColor(3);

			bHoveredAnyItem |= ImGui::IsItemHovered();
			if (ImGui::IsItemHovered() && GImGui->HoveredIdTimer > EG_HOVER_THRESHOLD)
			{
				ImGui::BeginTooltip();
				ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
				ImGui::TextUnformatted(filename.c_str());
				ImGui::PopTextWrapPos();
				ImGui::EndTooltip();
			}
			ImGui::NextColumn();
		}

		for (auto& file : m_Files)
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
			ImGui::Image((void*)(uint64_t)rendererID, { 64, 64 }, { 0, 1 }, { 1, 0 });
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
					pathOfSceneToOpen = pathString;
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
			if (bChangeColor)
				ImGui::PopStyleColor(3);
			bHoveredAnyItem |= ImGui::IsItemHovered();
			if (ImGui::IsItemHovered() && GImGui->HoveredIdTimer > EG_HOVER_THRESHOLD)
			{
				ImGui::BeginTooltip();
				ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
				ImGui::TextUnformatted(filename.c_str());
				ImGui::PopTextWrapPos();
				ImGui::EndTooltip();
			}
			ImGui::NextColumn();
		}

		if (bWindowHovered || bRenderingFirstTime)
		{
			bRenderingFirstTime = false;
			m_Directories.clear();
			m_Files.clear();

			for (auto& dir : std::filesystem::directory_iterator(m_CurrentDirectory))
			{
				const auto& path = dir.path();
				std::string pathString = path.string();
				std::string filename = path.filename().string();

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
			ImGui::Begin("Texture Viewer", &bShowTextureView);
			ImVec2 availSize = ImGui::GetContentRegionAvail();
			glm::vec2 textureSize = textureToView->GetSize();

			const float tRatio = textureSize[0] / textureSize[1];
			const float wRatio = availSize[0] / availSize[1];

			/*textureSize[0] = availSize[0];
			textureSize[1] = textureSize[0] / textureAspectRatio;
			if (textureSize[1] > availSize[1])
			{
				float coef = availSize[1] / textureSize[1];
				textureSize[1] = availSize[1];
				textureSize[0] *= coef;
			}*/

			textureSize = wRatio > tRatio ? glm::vec2{textureSize[0] * availSize[1] / textureSize[1], availSize[1]} 
										  : glm::vec2{availSize[0], textureSize[1] * availSize[0] / textureSize[0]};

			ImGui::Image((void*)(uint64_t)textureToView->GetRendererID(), { textureSize[0], textureSize[1] }, { 0, 1 }, { 1, 0 });

			ImGui::End();
		}

		if (ImGui::IsMouseDown(0) && !bHoveredAnyItem)
		{
			m_SelectedFile.clear();
		}

		ImGui::End();
	}
}
