#include "egpch.h"
#include "Texture2DAssetEditor.h"

#include "Eagle/Asset/Asset.h"
#include "Eagle/UI/UI.h"

#include <imgui_internal.h>

namespace Eagle
{
	static constexpr int s_MinMips = 1;
	static const char* s_CompressionHelpMsg = "If set to true, the engine will try to compress the image. "
		"Most of the time, medium compression is good enough. But if you see some banding/blocks, choose a higher quality, especially for normal maps.";

	Texture2DAssetEditor::Texture2DAssetEditor(const Ref<AssetTexture2D>& asset)
		: m_Asset(asset)
	{
		const auto& textureToView = m_Asset->GetTexture();
		m_GenerateMipsCount = textureToView->GetMipsCount();
		m_WindowName = AssetEditor::GetAssetWindowName(m_Asset);
		m_ViewportWindowName = "##" + std::to_string(m_Asset->GetGUID().GetHash());
	}

	void Texture2DAssetEditor::OnImGuiRender(bool* pOpen)
	{
		const auto& textureToView = m_Asset->GetTexture();

		ImGui::SetNextWindowSize(AssetEditor::GetDefaultWindowSize(), ImGuiCond_FirstUseEver);
		bool bHidden = !ImGui::Begin(m_ViewportWindowName.c_str());
		bDetailsVisible = (!bHidden) || (bHidden && !bDetailsDocked);
		ImVec2 availSize = ImGui::GetContentRegionAvail();
		glm::vec2 visualizeImageSize = textureToView->GetSize();
		const uint32_t mipsCount = textureToView->GetMipsCount(); // MipsCount == 1 means no mips, just the original
		m_SelectedMip = glm::clamp(m_SelectedMip, 0, (int)mipsCount - 1);

		const int maxMips = (int)CalculateMipCount(textureToView->GetSize());
		m_GenerateMipsCount = glm::clamp(m_GenerateMipsCount, s_MinMips, maxMips);

		const float tRatio = visualizeImageSize[0] / visualizeImageSize[1];
		const float wRatio = availSize[0] / availSize[1];

		visualizeImageSize = wRatio > tRatio ? glm::vec2{ visualizeImageSize[0] * availSize[1] / visualizeImageSize[1], availSize[1] }
											 : glm::vec2{ availSize[0], visualizeImageSize[1] * availSize[0] / visualizeImageSize[0] };

		UI::ImageMip(Cast<Texture2D>(textureToView), uint32_t(m_SelectedMip), { visualizeImageSize[0], visualizeImageSize[1] });

		ImGui::End();

		if (bDetailsVisible)
		{
			float anisotropy = textureToView->GetAnisotropy();
			FilterMode filterMode = textureToView->GetFilterMode();
			AddressMode addressMode = textureToView->GetAddressMode();
			const float maxAnisotropy = RenderManager::GetCapabilities().MaxAnisotropy;
			const size_t gpuMemSize = textureToView->GetMemoryUsage();
			TextureCompressor::Quality compression = m_Asset->GetCompressionQuality();
			bool bNormalMap = m_Asset->IsNormalMap();
			auto assetFormat = m_Asset->GetFormat();
			bool bChanged = false;

			const glm::ivec2 baseTextureSize = textureToView->GetSize();
			const glm::ivec2 mipTextureSize = baseTextureSize >> m_SelectedMip;
			const std::string baseSizeString = std::to_string(baseTextureSize.x) + "x" + std::to_string(baseTextureSize.y);
			const std::string mipSizeString = std::to_string(mipTextureSize.x) + "x" + std::to_string(mipTextureSize.y);

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
			UI::BeginPropertyGrid("TextureDetails");
			UI::Text("Name", m_Asset->GetPath().stem().string());
			UI::Text("Type", "Texture 2D");
			UI::Text("Resolution", baseSizeString);
			UI::Text("Mip resolution", mipSizeString);

			if (gpuMemSize < 1024)
				UI::Text("GPU memory usage (Bytes)", std::to_string(gpuMemSize));
			else if (gpuMemSize < 1024 * 1024)
				UI::Text("GPU memory usage (KB)", std::to_string(gpuMemSize / 1024.f));
			else
				UI::Text("GPU memory usage (MB)", std::to_string(gpuMemSize / 1024.f / 1024.f));

			if (UI::ComboEnum("Format", assetFormat))
			{
				m_Asset->SetFormat(assetFormat);
				bChanged = true;
			}

			if (UI::Property("Is Normal Map", bNormalMap, "Currently, it only affects the result if the compression is enabled"))
			{
				m_Asset->SetIsNormalMap(bNormalMap);
				bChanged = true;
			}

			if (UI::ComboEnum("Compression", compression, s_CompressionHelpMsg))
			{
				m_Asset->SetCompression(compression, textureToView->GetMipsCount());
				bChanged = true;
			}

			UI::Text("GPU format", Utils::GetEnumName(textureToView->GetFormat()), "When compressed, this format can change at runtime depending on the hardware capabilities");
			ImGui::Separator();
			if (UI::PropertySlider("Anisotropy", anisotropy, 1.f, maxAnisotropy))
			{
				textureToView->SetAnisotropy(anisotropy);
				bChanged = true;
			}

			// Filter mode
			{
				static const std::vector<std::string> modesStrings = { "Nearest", "Bilinear", "Trilinear" };
				int selectedIndex = 0;
				if (UI::Combo("Filtering", (uint32_t)filterMode, modesStrings, selectedIndex))
				{
					filterMode = FilterMode(selectedIndex);
					textureToView->SetFilterMode(filterMode);
					bChanged = true;
				}
			}

			// Address Mode
			{
				static const std::vector<std::string> modesStrings = { "Wrap", "Mirror", "Clamp", "Clamp to Black", "Clamp to White" };

				int selectedIndex = 0;
				if (UI::Combo("Wrapping", (uint32_t)addressMode, modesStrings, selectedIndex))
				{
					addressMode = AddressMode(selectedIndex);
					textureToView->SetAddressMode(addressMode);
					bChanged = true;
				}
			}

			// Visualize mips
			{
				if (m_MipNames.size() != mipsCount)
				{
					m_MipNames.resize(mipsCount);
					for (uint32_t i = 0; i < mipsCount; ++i)
						m_MipNames[i] = "Mip #" + std::to_string(i);
				}

				UI::Combo("Visualize", (uint32_t)m_SelectedMip, m_MipNames, m_SelectedMip, {}, "Mip #0 is the original texture");
			}

			// Generate Mips
			{
				UI::UpdateIDBuffer("Generate Mips");
				ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
				ImGui::Text("Generate Mips");
				ImGui::SameLine();
				UI::HelpMarker("Mips count = 1 means no mips, only the original texture");

				ImGui::NextColumn();

				{
					const float labelwidth = ImGui::CalcTextSize("Generate", NULL, true).x;
					const float buttonWidth = labelwidth + ImGui::GetStyle().FramePadding.x * 8.0f;
					const float width = ImGui::GetColumnWidth();
					ImGui::PushItemWidth(width - buttonWidth);
				}
				if (ImGui::SliderInt(UI::GetIDBuffer(), &m_GenerateMipsCount, s_MinMips, maxMips))
				{
					m_GenerateMipsCount = glm::clamp(m_GenerateMipsCount, s_MinMips, maxMips);
				}

				ImGui::PopItemWidth();

				ImGui::SameLine();

				if (ImGui::Button("Generate"))
				{
					m_Asset->SetCompression(m_Asset->GetCompressionQuality(), uint32_t(m_GenerateMipsCount));
					bChanged = true;
				}
			}

			UI::EndPropertyGrid();

			if (bChanged)
			{
				m_Asset->SetDirty(true);
				m_Asset->OnModified();
			}

			ImGui::Separator();
			ImGui::Separator();

			if (ImGui::Button("Save asset"))
				Asset::Save(m_Asset);

			ImGui::End();
		}
	}
}
