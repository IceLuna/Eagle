#include "egpch.h"

#include "UI.h"
#include <imgui.h>
#include <imgui_internal.h>
#include "Eagle/Utils/PlatformUtils.h"
#include "Eagle/Components/Components.h"

namespace Eagle::UI
{
	bool DrawTextureSelection(Ref<Texture>& modifyingTexture, const std::string& textureName, bool bLoadAsSRGB)
	{
		bool bResult = false;
		uint32_t rendererID;

		if (modifyingTexture)
			rendererID = modifyingTexture->GetNonSRGBRendererID();
		else
			rendererID = Texture2D::NoneTexture->GetRendererID();

		//TODO: Add None
		const std::string comboID = std::string("##") + textureName;
		const char* comboItems[] = { "None", "New", "Black", "White" };
		constexpr int basicSize = 4; //above size
		static int currentItemIdx = -1; // Here our selection data is an index.

		ImGui::Image((void*)(uint64_t)(rendererID), { 32, 32 }, { 0, 1 }, { 1, 0 });
		ImGui::SameLine();

		//Drop event
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("TEXTURE_CELL"))
			{
				const wchar_t* payload_n = (const wchar_t*)payload->Data;
				std::filesystem::path filepath(payload_n);
				Ref<Texture> texture;

				if (TextureLibrary::Get(filepath, &texture) == false)
				{
					texture = Texture2D::Create(filepath, bLoadAsSRGB);
				}
				modifyingTexture = texture;
			}

			ImGui::EndDragDropTarget();
		}

		if (ImGui::BeginCombo(comboID.c_str(), textureName.c_str(), 0))
		{
			//Initially find currently selected texture to scroll to it.
			if (modifyingTexture)
			{
				bool bFound = false;
				if (modifyingTexture == Texture2D::BlackTexture)
				{
					currentItemIdx = 1;
					bFound = true;
				}
				else if (modifyingTexture == Texture2D::WhiteTexture)
				{
					currentItemIdx = 2;
					bFound = true;
				}
				const auto& allTextures = TextureLibrary::GetTextures();

				if (!bFound)
					for (int i = 0; i < allTextures.size(); ++i)
					{
						if (allTextures[i] == modifyingTexture)
						{
							currentItemIdx = i + basicSize;
							break;
						}
					}
			}
			else currentItemIdx = -1;

			//Drawing basic (none, new, black, white) texture combo items
			for (int i = 0; i < IM_ARRAYSIZE(comboItems); ++i)
			{
				const bool bSelected = (currentItemIdx == i);
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
					case 0: //None
					{
						modifyingTexture.reset();
						bResult = true;
						break;
					}
					case 1: //New
					{
						const std::filesystem::path& file = FileDialog::OpenFile(FileDialog::TEXTURE_FILTER);
						if (file.empty() == false)
						{
							modifyingTexture = Texture2D::Create(file, bLoadAsSRGB);
							bResult = true;
						}
						break;
					}
					case 2: //Black
					{
						modifyingTexture = Texture2D::BlackTexture;
						bResult = true;
						break;
					}
					case 3: //White
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
				const bool bSelected = (currentItemIdx == i + basicSize);
				ImGui::PushID(allTextures[i]->GetRendererID());
				bool bSelectableTriggered = ImGui::Selectable("##label", bSelected, ImGuiSelectableFlags_AllowItemOverlap, {0.0f, 32.f});
				bool bSelectableClicked = ImGui::IsItemClicked();
				ImGui::SameLine();
				ImGui::Image((void*)(uint64_t)(allTextures[i]->GetNonSRGBRendererID()), { 32, 32 }, { 0, 1 }, { 1, 0 });

				ImGui::SameLine();
				std::filesystem::path path = allTextures[i]->GetPath();
				ImGui::Text("%s", path.stem().u8string().c_str());
				if (bSelectableTriggered)
					currentItemIdx = i + basicSize;

				// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
				if (bSelected)
					ImGui::SetItemDefaultFocus();

				if (bSelectableClicked)
				{
					currentItemIdx = i + basicSize;

					modifyingTexture = allTextures[i];
					bResult = true;
				}
				ImGui::PopID();
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
		bool bBeginCombo = ImGui::BeginCombo(comboID.c_str(), smName.c_str(), 0);

		//Drop event
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MESH_CELL"))
			{
				const wchar_t* payload_n = (const wchar_t*)payload->Data;
				std::filesystem::path filepath(payload_n);
				Ref<StaticMesh> mesh;

				if (StaticMeshLibrary::Get(filepath, &mesh) == false)
				{
					mesh = StaticMesh::Create(filepath);
				}
				smComponent.StaticMesh = mesh;
			}

			ImGui::EndDragDropTarget();
		}

		if (bBeginCombo)
		{
			currentItemIdx = -1;
			////Initially find currently selected static mesh to scroll to it.
			if (smComponent.StaticMesh)
			{
				const auto& allStaticMeshes = StaticMeshLibrary::GetMeshes();
				for (int i = 0; i < allStaticMeshes.size(); ++i)
				{
					if (allStaticMeshes[i] == smComponent.StaticMesh)
					{
						currentItemIdx = i + basicSize;
						break;
					}
				}
			}
			//Drawing basic (new) combo items
			for (int i = 0; i < IM_ARRAYSIZE(comboItems); ++i)
			{
				const bool bSelected = (currentItemIdx == i);
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
						const std::filesystem::path& file = FileDialog::OpenFile(FileDialog::MESH_FILTER);
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
				const bool bSelected = (currentItemIdx == i + basicSize);

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

	bool DrawVec3Control(const std::string& label, glm::vec3& values, const glm::vec3 resetValues /* = glm::vec3{ 0.f }*/, float columnWidth /*= 100.f*/)
	{
		bool bValueChanged = false;
		ImGuiIO& io = ImGui::GetIO();
		auto boldFont = io.Fonts->Fonts[0];

		ImGui::PushID(label.c_str());

		ImGui::Columns(2, nullptr, false);
		ImGui::SetColumnWidth(0, columnWidth);
		ImGui::Text(label.c_str());
		ImGui::NextColumn();

		ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0.f, 0.f });

		float lineHeight = (GImGui->Font->FontSize * boldFont->Scale) + GImGui->Style.FramePadding.y * 2.f;
		ImVec2 buttonSize = { lineHeight + 3.f, lineHeight };

		//X
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.8f, 0.1f, 0.15f, 1.f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.9f, 0.2f, 0.2f, 1.f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.8f, 0.1f, 0.15f, 1.f });
		ImGui::PushFont(boldFont);
		if (ImGui::Button("X", buttonSize))
		{
			values.x = resetValues.x;
			bValueChanged = true;
		}
		ImGui::PopFont();
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		bValueChanged |= ImGui::DragFloat("##X", &values.x, 0.1f);
		ImGui::PopItemWidth();
		ImGui::SameLine();

		//Y
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.2f, 0.7f, 0.2f, 1.f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.3f, 0.8f, 0.3f, 1.f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.2f, 0.7f, 0.2f, 1.f });
		ImGui::PushFont(boldFont);
		if (ImGui::Button("Y", buttonSize))
		{
			values.y = resetValues.y;
			bValueChanged = true;
		}
		ImGui::PopFont();
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		bValueChanged |= ImGui::DragFloat("##Y", &values.y, 0.1f);
		ImGui::PopItemWidth();
		ImGui::SameLine();

		//Z
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.1f, 0.25f, 0.8f, 1.f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.2f, 0.35f, 0.9f, 1.f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.1f, 0.25f, 0.8f, 1.f });
		ImGui::PushFont(boldFont);
		if (ImGui::Button("Z", buttonSize))
		{
			values.z = resetValues.z;
			bValueChanged = true;
		}
		ImGui::PopFont();
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		bValueChanged |= ImGui::DragFloat("##Z", &values.z, 0.1f);
		ImGui::PopItemWidth();

		ImGui::PopStyleVar();

		ImGui::Columns(1);

		ImGui::PopID();

		return bValueChanged;
	}

	void HelpMarker(const std::string& text)
	{
		ImGui::TextDisabled("(?)");
		if (ImGui::IsItemHovered())
		{
			ImGui::BeginTooltip();
			ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
			ImGui::TextUnformatted(text.c_str());
			ImGui::PopTextWrapPos();
			ImGui::EndTooltip();
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
			if (buttons & Button::Yes)
			{
				if (ImGui::Button("Yes", ImVec2(120, 0)))
				{
					pressedButton = Button::Yes;
					ImGui::CloseCurrentPopup();
				}
				ImGui::SameLine();
			}
			if (buttons & Button::No)
			{
				if (ImGui::Button("No", ImVec2(120, 0)))
				{
					pressedButton = Button::No;
					ImGui::CloseCurrentPopup();
				}
				ImGui::SameLine();
			}
			if (buttons & Button::Cancel)
			{
				if (ImGui::Button("Cancel", ImVec2(120, 0)))
				{
					pressedButton = Button::Cancel;
					ImGui::CloseCurrentPopup();
				}
				ImGui::SameLine();
			}
			ImGui::EndPopup();
		}

		return pressedButton;
	}
}