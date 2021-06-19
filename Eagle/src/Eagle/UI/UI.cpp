#include "egpch.h"

#include "UI.h"
#include "imgui.h"
#include "Eagle/Utils/PlatformUtils.h"
#include "Eagle/Components/Components.h"

namespace Eagle::UI
{
	bool DrawTextureSelection(Ref<Texture>& modifyingTexture, const std::string& textureName)
	{
		bool bResult = false;
		uint32_t rendererID;

		if (modifyingTexture)
			rendererID = modifyingTexture->GetRendererID();
		else
			rendererID = Texture2D::NoneTexture->GetRendererID();

		//TODO: Add None
		const std::string comboID = std::string("##") + textureName;
		const char* comboItems[] = { "New", "Black", "White" };
		constexpr int basicSize = 3; //above size
		static int currentItemIdx = -1; // Here our selection data is an index.

		ImGui::Image((void*)(uint64_t)(rendererID), { 32, 32 }, { 0, 1 }, { 1, 0 });
		ImGui::SameLine();

		if (ImGui::BeginCombo(comboID.c_str(), textureName.c_str(), 0))
		{
			//Drawing basic (new, black, white) texture combo items
			for (int i = 0; i < IM_ARRAYSIZE(comboItems); ++i)
			{
				//const bool bSelected = (currentItemIdx == i);
				const bool bSelected = false;
				if (ImGui::Selectable(comboItems[i], bSelected))
					currentItemIdx = i;

				// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
				if (bSelected)
					ImGui::SetItemDefaultFocus();

				if (ImGui::IsItemClicked())
				{
					currentItemIdx = i;

					switch (currentItemIdx)
					{
					case 0: //New
					{
						const std::string& file = FileDialog::OpenFile(FileDialog::TEXTURE_FILTER);
						if (file.empty() == false)
						{
							modifyingTexture = Texture2D::Create(file);
							bResult = true;
						}
						break;
					}
					case 1: //Black
					{
						modifyingTexture = Texture2D::BlackTexture;
						bResult = true;
						break;
					}
					case 2: //White
					{
						modifyingTexture = Texture2D::WhiteTexture;
						bResult = true;
						break;
					}
					}
				}
			}

			//Drawing all existing textures
			const auto& allTextures = TextureLibrary::GetTextures();
			for (int i = 0; i < allTextures.size(); ++i)
			{
				//const bool bSelected = (currentItemIdx == i + basicSize);
				const bool bSelected = false;
				ImGui::Image((void*)(uint64_t)(allTextures[i]->GetRendererID()), { 32, 32 }, { 0, 1 }, { 1, 0 });

				ImGui::SameLine();
				std::filesystem::path path = allTextures[i]->GetPath();
				if (ImGui::Selectable(path.stem().string().c_str(), bSelected, 0, ImVec2{ ImGui::GetContentRegionAvailWidth(), 32 }))
					currentItemIdx = i + basicSize;

				// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
				if (bSelected)
					ImGui::SetItemDefaultFocus();

				if (ImGui::IsItemClicked())
				{
					currentItemIdx = i + basicSize;

					modifyingTexture = allTextures[i];
					bResult = true;
				}
			}

			ImGui::EndCombo();
		}
		return bResult;
	}

	void DrawStaticMeshSelection(StaticMeshComponent& smComponent, const std::string& smName)
	{
		const std::string comboID = std::string("##") + smName;
		const char* comboItems[] = { "New" };
		constexpr int basicSize = 1; //above size
		static int currentItemIdx = -1; // Here our selection data is an index.
		if (ImGui::BeginCombo(comboID.c_str(), smName.c_str(), 0))
		{
			//Drawing basic (new) combo items
			for (int i = 0; i < IM_ARRAYSIZE(comboItems); ++i)
			{
				//const bool bSelected = (currentItemIdx == i);
				const bool bSelected = false;
				if (ImGui::Selectable(comboItems[i], bSelected))
					currentItemIdx = i;

				// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
				if (bSelected)
					ImGui::SetItemDefaultFocus();

				if (ImGui::IsItemClicked())
				{
					currentItemIdx = i;

					switch (currentItemIdx)
					{
					case 0: //New
					{
						const std::string& file = FileDialog::OpenFile(FileDialog::MESH_FILTER);
						if (file.empty() == false)
						{
							smComponent.StaticMesh = StaticMesh::Create(file);
						}
						break;
					}
					}
				}
			}

			//Drawing all existing meshes
			const auto& allStaticMeshes = StaticMeshLibrary::GetMeshes();
			for (int i = 0; i < allStaticMeshes.size(); ++i)
			{
				//const bool bSelected = (currentItemIdx == i + basicSize);
				const bool bSelected = false;

				const std::string& smName = allStaticMeshes[i]->GetName();
				if (ImGui::Selectable(smName.c_str(), bSelected, 0, ImVec2{ ImGui::GetContentRegionAvailWidth(), 32 }))
					currentItemIdx = i + basicSize;

				// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
				if (bSelected)
					ImGui::SetItemDefaultFocus();

				if (ImGui::IsItemClicked())
				{
					currentItemIdx = i + basicSize;

					smComponent.StaticMesh = allStaticMeshes[i];
				}
			}
			ImGui::EndCombo();
		}
	}

	Button ShowMessage(const std::string& title, const std::string& message, Button buttons)
	{
		Button pressedButton = Button::None;
		ImGui::OpenPopup(title.c_str());

		// Always center this window when appearing
		ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

		if (ImGui::BeginPopupModal(title.c_str(), NULL, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::Text(message.c_str());
			ImGui::Separator();

			if (buttons & Button::OK)
			{
				if (ImGui::Button("OK", ImVec2(120, 0)))
				{
					pressedButton = Button::OK;
					ImGui::CloseCurrentPopup();
				}
				ImGui::SetItemDefaultFocus();
				ImGui::SameLine();
			}
			if (buttons & Button::Cancel)
			{
				if (ImGui::Button("Cancel", ImVec2(120, 0)))
				{
					pressedButton = Button::Cancel;
					ImGui::CloseCurrentPopup();
				}
			}
			ImGui::EndPopup();
		}

		return pressedButton;
	}
}