#include "egpch.h"
#include "TextureCubeAssetEditor.h"

#include "Eagle/Asset/Asset.h"
#include "Eagle/UI/UI.h"

namespace Eagle
{
	void TextureCubeAssetEditor::OnImGuiRender(bool* pOpen)
	{
		const Ref<TextureCube>& textureCube = m_Asset->GetTexture();
		const Ref<Texture2D>& textureToView = textureCube->GetTexture2D();

		ImGui::SetNextWindowSize(ImVec2(720.f, 560.f), ImGuiCond_FirstUseEver);
		bool bHidden = !ImGui::Begin(m_Asset->GetPath().u8string().c_str(), pOpen);
		static bool detailsDocked = false;
		static bool bDetailsVisible;
		bDetailsVisible = (!bHidden) || (bHidden && !detailsDocked);
		ImVec2 availSize = ImGui::GetContentRegionAvail();
		glm::vec2 visualizeImageSize = textureToView->GetSize();

		const float tRatio = visualizeImageSize[0] / visualizeImageSize[1];
		const float wRatio = availSize[0] / availSize[1];

		visualizeImageSize = wRatio > tRatio ? glm::vec2{ visualizeImageSize[0] * availSize[1] / visualizeImageSize[1], availSize[1] }
		: glm::vec2{ availSize[0], visualizeImageSize[1] * availSize[0] / visualizeImageSize[0] };

		UI::Image(Cast<Texture2D>(textureToView), { visualizeImageSize[0], visualizeImageSize[1] });
		if (bDetailsVisible)
		{
			const glm::ivec2 textureSize = glm::ivec2(textureToView->GetSize());
			const std::string textureSizeString = std::to_string(textureSize.x) + "x" + std::to_string(textureSize.y);

			ImGui::Begin("Details");
			detailsDocked = ImGui::IsWindowDocked();
			UI::BeginPropertyGrid("TextureDetails");
			UI::Text("Name", m_Asset->GetPath().stem().u8string());
			UI::Text("Resolution", textureSizeString);
			UI::Text("Format", Utils::GetEnumName(m_Asset->GetFormat()));

			{
				static const Texture2D* s_LastTexture = nullptr;
				static int layerSize = 1u;
				if (s_LastTexture != textureToView.get()) // Texture changed, reset mips slider
				{
					s_LastTexture = textureToView.get();
					layerSize = (int)textureCube->GetSize().x;
				}

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

				if (ImGui::DragInt(UI::GetIDBuffer(), &layerSize, 16.f, 32, 4096))
					layerSize = glm::clamp(layerSize, 16, 4096);

				ImGui::PopItemWidth();

				ImGui::SameLine();

				if (ImGui::Button("Generate"))
				{
					m_Asset->SetLayerSize(uint32_t(layerSize));
					m_Asset->SetDirty(true);
				}
			}

			UI::EndPropertyGrid();

			ImGui::Separator();
			ImGui::Separator();

			if (ImGui::Button("Save asset"))
				Asset::Save(m_Asset);

			ImGui::End();
		}

		ImGui::End();
	}
}
