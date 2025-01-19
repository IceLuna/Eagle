#include "egpch.h"
#include "Texture2DAssetEditor.h"

#include "Eagle/Asset/Asset.h"
#include "Eagle/UI/UI.h"

namespace Eagle
{
	void Texture2DAssetEditor::OnImGuiRender(bool* pOpen)
	{
		const auto& textureToView = m_Asset->GetTexture();

		ImGui::SetNextWindowSize(ImVec2(720.f, 560.f), ImGuiCond_FirstUseEver);
		bool bHidden = !ImGui::Begin(m_Asset->GetPath().u8string().c_str(), pOpen);
		bDetailsVisible = (!bHidden) || (bHidden && !bDetailsDocked);
		ImVec2 availSize = ImGui::GetContentRegionAvail();
		glm::vec2 visualizeImageSize = textureToView->GetSize();
		const uint32_t mipsCount = textureToView->GetMipsCount(); // MipsCount == 1 means no mips, just the original
		m_SelectedMip = glm::clamp(m_SelectedMip, 0, (int)mipsCount - 1);

		const float tRatio = visualizeImageSize[0] / visualizeImageSize[1];
		const float wRatio = availSize[0] / availSize[1];

		visualizeImageSize = wRatio > tRatio ? glm::vec2{ visualizeImageSize[0] * availSize[1] / visualizeImageSize[1], availSize[1] }
											 : glm::vec2{ availSize[0], visualizeImageSize[1] * availSize[0] / visualizeImageSize[0] };

		UI::ImageMip(Cast<Texture2D>(textureToView), uint32_t(m_SelectedMip), { visualizeImageSize[0], visualizeImageSize[1] });

		if (bDetailsVisible)
		{
			float anisotropy = textureToView->GetAnisotropy();
			FilterMode filterMode = textureToView->GetFilterMode();
			AddressMode addressMode = textureToView->GetAddressMode();
			const float maxAnisotropy = RenderManager::GetCapabilities().MaxAnisotropy;
			const size_t gpuMemSize = textureToView->GetMemoryUsage();
			bool bCompressed = m_Asset->IsCompressed();
			bool bNormalMap = m_Asset->IsNormalMap();
			bool bNeedAlpha = m_Asset->DoesNeedAlpha();
			auto assetFormat = m_Asset->GetFormat();
			bool bChanged = false;

			const glm::ivec2 baseTextureSize = textureToView->GetSize();
			const glm::ivec2 mipTextureSize = baseTextureSize >> m_SelectedMip;
			const std::string baseSizeString = std::to_string(baseTextureSize.x) + "x" + std::to_string(baseTextureSize.y);
			const std::string mipSizeString = std::to_string(mipTextureSize.x) + "x" + std::to_string(mipTextureSize.y);

			ImGui::Begin(("Details: " + m_Asset->GetPath().u8string()).c_str());
			bDetailsDocked = ImGui::IsWindowDocked();
			UI::BeginPropertyGrid("TextureDetails");
			UI::Text("Name", m_Asset->GetPath().stem().u8string());
			UI::Text("Type", "Texture 2D");
			UI::Text("Resolution", baseSizeString);
			UI::Text("Mip resolution", mipSizeString);

			if (gpuMemSize < 1024)
				UI::Text("GPU memory usage (Bytes)", std::to_string(gpuMemSize));
			else if (gpuMemSize < 1024 * 1024)
				UI::Text("GPU memory usage (KB)", std::to_string(gpuMemSize / 1024.f));
			else
				UI::Text("GPU memory usage (MB)", std::to_string(gpuMemSize / 1024.f / 1024.f));

			if (UI::ComboEnum("Format", assetFormat, "Compression only supports RGBA8 format!"))
			{
				// Only uncompressed textures can change format
				if (bCompressed)
				{
					if (assetFormat != AssetTexture2DFormat::RGBA8)
						Application::Get().GetImGuiLayer()->AddMessage("Compressed textures only support RGBA8 format");
				}
				else
				{
					m_Asset->SetFormat(assetFormat);
					bChanged = true;
				}
			}

			if (UI::Property("Is Normal Map", bNormalMap, "Currently, it only affects the result if the compression is enabled"))
			{
				m_Asset->SetIsNormalMap(bNormalMap);
				bChanged = true;
			}

			if (UI::Property("Import alpha", bNeedAlpha, "Currently, it only affects the result if the compression is enabled"))
			{
				m_Asset->SetNeedsAlpha(bNeedAlpha);
				bChanged = true;
			}

			if (UI::Property("Compressed", bCompressed, "If set to true, the engine will try to compress the image"))
			{
				m_Asset->SetIsCompressed(bCompressed, textureToView->GetMipsCount());
				bChanged = true;
			}

			if (bCompressed)
				UI::Text("Compression format", Utils::GetEnumName(textureToView->GetFormat()), "This format can change at runtime depending on the hardware capabilities");
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
				constexpr int minMips = 1;
				const int maxMips = (int)CalculateMipCount(textureToView->GetSize());

				int generateMipsCount = textureToView->GetMipsCount();
				generateMipsCount = glm::clamp(generateMipsCount, minMips, maxMips);

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
				ImGui::SliderInt(UI::GetIDBuffer(), &generateMipsCount, minMips, maxMips);

				ImGui::PopItemWidth();

				ImGui::SameLine();

				if (ImGui::Button("Generate"))
				{
					m_Asset->SetIsCompressed(m_Asset->IsCompressed(), uint32_t(generateMipsCount));
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

		ImGui::End();
	}
}
