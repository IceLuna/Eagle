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
		static bool bShowSaveScenePopup = false;
		static bool bRenderingFirstTime = true;
		static std::string pathOfSceneToOpen;

		ImGui::Begin("Content Browser");
		ImGui::PushID("Content Browser");

		bool bWindowHovered = ImGui::IsWindowHovered();
		ImVec2 size = ImGui::GetContentRegionAvail();
		constexpr int columnWidth = 72;
		int columns = (int)size[0] / columnWidth;

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
					m_CurrentDirectory = temp;

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
			if (ImGui::IsItemClicked() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
			{
				m_CurrentDirectory = path;
			}
			if (ImGui::Button(filename.c_str(), { 64, 22 }))
			{
				m_CurrentDirectory = path;
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
			bClicked |= ImGui::IsItemClicked();
			bClicked = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && bClicked;

			if (ImGui::Button(filename.c_str(), { 64, 22 }) || bClicked)
			{
				if (fileFormat == Utils::FileFormat::SCENE)
				{
					bShowSaveScenePopup = true;
					pathOfSceneToOpen = pathString;
				}
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

		ImGui::End();
	}
}
