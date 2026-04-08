#include "egpch.h"
#include "TextureCubeAssetEditor.h"

#include "Eagle/Asset/Asset.h"
#include "Eagle/UI/UI.h"

#include <imgui_internal.h>

namespace Eagle
{
	TextureCubeAssetEditor::TextureCubeAssetEditor(const Ref<AssetTextureCube>& asset)
		: m_Asset(asset)
	{
		const auto& texture = m_Asset->GetTexture();
		m_LayersSize = (int)texture->GetSize().x;
		m_PrefilterSize = (int)texture->GetPrefilterSize();
		m_WindowName = AssetEditor::GetAssetWindowName(m_Asset);
		m_ViewportWindowName = "##" + std::to_string(m_Asset->GetGUID().GetHash());
	}

	void TextureCubeAssetEditor::OnImGuiRender(bool* pOpen)
	{
		const Ref<TextureCube>& textureCube = m_Asset->GetTexture();
		const Ref<Texture2D>& textureToView = textureCube->GetTexture2D();

		ImGui::SetNextWindowSize(AssetEditor::GetDefaultWindowSize(), ImGuiCond_FirstUseEver);
		bool bHidden = !ImGui::Begin(m_ViewportWindowName.c_str());
		bDetailsVisible = (!bHidden) || (bHidden && !bDetailsDocked);
		ImVec2 availSize = ImGui::GetContentRegionAvail();
		glm::vec2 visualizeImageSize = textureToView->GetSize();

		const float tRatio = visualizeImageSize[0] / visualizeImageSize[1];
		const float wRatio = availSize[0] / availSize[1];

		visualizeImageSize = wRatio > tRatio ? glm::vec2{ visualizeImageSize[0] * availSize[1] / visualizeImageSize[1], availSize[1] }
											 : glm::vec2{ availSize[0], visualizeImageSize[1] * availSize[0] / visualizeImageSize[0] };

		UI::Image(Cast<Texture2D>(textureToView), { visualizeImageSize[0], visualizeImageSize[1] });

		ImGui::End();

		if (bDetailsVisible)
		{
			const glm::ivec2 textureSize = glm::ivec2(textureToView->GetSize());
			const std::string textureSizeString = std::to_string(textureSize.x) + "x" + std::to_string(textureSize.y);
			size_t gpuMemSize = textureCube->GetMemoryUsage();
			auto assetFormat = m_Asset->GetFormat();
			bool bChanged = false;

			ImGui::SetNextWindowSize(AssetEditor::GetDefaultWindowSize(), ImGuiCond_FirstUseEver);
			ImGui::Begin(m_WindowName.c_str(), pOpen);

			const bool bFirstUseEver = (ImGui::GetCurrentWindow()->SetWindowDockAllowFlags & ImGuiCond_FirstUseEver) == ImGuiCond_FirstUseEver;
			if (bFirstUseEver && !m_ViewportWindowName.empty())
			{
				ImGuiID parent_node = ImGui::DockBuilderAddNode();
				ImGui::DockBuilderSetNodePos(parent_node, ImGui::GetWindowPos());
				ImGui::DockBuilderSetNodeSize(parent_node, ImGui::GetWindowSize());
				ImGuiID nodeA;
				ImGuiID nodeB;
				ImGui::DockBuilderSplitNode(parent_node, ImGuiDir_Right, 0.5f, &nodeB, &nodeA);

				ImGui::DockBuilderDockWindow(m_ViewportWindowName.data(), nodeA);
				ImGui::DockBuilderDockWindow(m_WindowName.c_str(), nodeB);

				// Disable tab bar for the viewport
				if (ImGuiDockNode* dock = ImGui::DockContextFindNodeByID(GImGui, nodeA))
				{
					dock->SetLocalFlags(ImGuiDockNodeFlags_NoTabBar);
				}

				const auto defaultSize = AssetEditor::GetDefaultWindowSize();
				ImGui::SetWindowSize(ImVec2(defaultSize.x * 1.5f, defaultSize.y));
			}

			bDetailsDocked = ImGui::IsWindowDocked();
			UI::BeginPropertyGrid("TextureCubeDetails");
			UI::Text("Name", Utils::AsString(m_Asset->GetPath().stem()));
			UI::Text("Type", "Texture Cube");
			UI::Text("Resolution", textureSizeString);

			if (gpuMemSize < 1024)
				UI::Text("GPU memory usage (Bytes)", std::to_string(gpuMemSize), "Cube + Prefilter + Irradiance images");
			else if (gpuMemSize < 1024 * 1024)
				UI::Text("GPU memory usage (KB)", std::to_string(gpuMemSize / 1024.f), "Cube + Prefilter + Irradiance images");
			else
				UI::Text("GPU memory usage (MB)", std::to_string(gpuMemSize / 1024.f / 1024.f), "Cube + Prefilter + Irradiance images");

			if (UI::ComboEnum("Format", assetFormat))
			{
				// IBL generation might take some time, which for some reason results in vulkan validation error (from ImGui)
				// saying that ImageView is destroyed before commands finish executing. But deferring this call fixes it.
				// It's strange because the error seems to come from ImGui, but ImGui used 2D texture, which is created fast.
				Application::Get().CallNextFrame([asset = m_Asset, format = assetFormat]()
				{
					asset->SetFormat(format);
				});
				bChanged = true;
			}

			{
				UI::UpdateIDBuffer("Generate Layer size");
				ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
				ImGui::Text("Layer Size");
				ImGui::SameLine();
				UI::HelpMarker("Resolution of a cube side");

				ImGui::NextColumn();

				{
					const float labelwidth = ImGui::CalcTextSize("Generate", NULL, true).x;
					const float buttonWidth = labelwidth + ImGui::GetStyle().FramePadding.x * 8.0f;
					const float width = ImGui::GetColumnWidth();
					ImGui::PushItemWidth(width - buttonWidth);
				}

				if (ImGui::DragInt(UI::GetIDBuffer(), &m_LayersSize, 16.f, 32, 4096))
					m_LayersSize = glm::clamp(m_LayersSize, 16, 4096);

				ImGui::PopItemWidth();

				ImGui::SameLine();

				if (ImGui::Button("Generate"))
				{
					m_Asset->SetLayerSize(uint32_t(m_LayersSize));
					bChanged = true;
				}
			}

			{
				ImGui::NextColumn();
				UI::UpdateIDBuffer("Generate Prefilter size");
				ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
				ImGui::Text("Prefilter Size");
				ImGui::SameLine();
				UI::HelpMarker("The quality of IBL reflection");

				ImGui::NextColumn();

				{
					const float labelwidth = ImGui::CalcTextSize("Generate", NULL, true).x;
					const float buttonWidth = labelwidth + ImGui::GetStyle().FramePadding.x * 8.0f;
					const float width = ImGui::GetColumnWidth();
					ImGui::PushItemWidth(width - buttonWidth);
				}

				if (ImGui::DragInt(UI::GetIDBuffer(), &m_PrefilterSize, 16.f, 32, 4096))
					m_PrefilterSize = glm::clamp(m_PrefilterSize, 16, 4096);

				ImGui::PopItemWidth();

				ImGui::SameLine();

				ImGui::PushID(UI::GetIDBuffer());
				if (ImGui::Button("Generate"))
				{
					m_Asset->SetPrefilterSize(uint32_t(m_PrefilterSize));
					bChanged = true;
				}
				ImGui::PopID();
			}

			UI::EndPropertyGrid();

			ImGui::Separator();
			ImGui::Separator();

			if (bChanged)
			{
				m_Asset->SetDirty(true);
				m_Asset->OnModified();
			}

			if (ImGui::Button("Save asset"))
				Asset::Save(m_Asset);

			ImGui::End();
		}
	}
}
