#include "egpch.h"
#include "ContentBrowserPanel.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include "Eagle/Renderer/Texture.h"
#include "Eagle/Utils/Utils.h"
#include "Eagle/Utils/PlatformUtils.h"
#include "../EditorLayer.h"

namespace Eagle
{
	static const std::filesystem::path s_ContentDirectory("assets");

	ContentBrowserPanel::ContentBrowserPanel(EditorLayer& editorLayer) 
	: m_CurrentDirectory(s_ContentDirectory)
	, m_EditorLayer(editorLayer)
	{}

	void ContentBrowserPanel::OnImGuiRender()
	{
		ImGui::Begin("Content Browser");
		ImGui::PushID("Content Browser");
		ImVec2 size = ImGui::GetContentRegionAvail();
		constexpr int columnWidth = 72;
		int columns = size[0] / columnWidth;

		if (m_CurrentDirectory != s_ContentDirectory)
			if (ImGui::Button("<-"))
				m_CurrentDirectory = m_CurrentDirectory.parent_path();
		
		if (columns > 1)
		{
			ImGui::Columns(columns, nullptr, false);
			ImGui::SetColumnWidth(0, columnWidth);
		}

		for (auto& dir : std::filesystem::directory_iterator(m_CurrentDirectory))
		{
			const auto& path = dir.path();
			std::string pathString = path.string();
			std::string filename = path.filename().string();

			if (dir.is_directory())
			{
				ImGui::Image((void*)(uint64_t)Texture2D::FolderIconTexture->GetRendererID(), {64, 64}, { 0, 1 }, { 1, 0 });
				if (ImGui::IsItemClicked())
				{
					m_CurrentDirectory = path;
				}
				if (ImGui::Button(filename.c_str(), {64, 22}))
				{
					m_CurrentDirectory = path;
				}
			}
			else
			{
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
				ImGui::Text("%s", filename.c_str());
				bClicked |= ImGui::IsItemClicked();
				bClicked = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && bClicked;

				if (bClicked)
				{
					if (fileFormat == Utils::FileFormat::SCENE)
					{
						if (Dialog::YesNoQuestion("Eagle-Editor", "Do you want to save current scene?"))
							m_EditorLayer.SaveScene();
						m_EditorLayer.OpenScene(pathString);
					}
				}
			}
			ImGui::NextColumn();
		}
		ImGui::Columns(1);
		ImGui::PopID();
		ImGui::End();
	}
}
