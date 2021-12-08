#include "egpch.h"

#include "UI.h"
#include <imgui.h>
#include <imgui_internal.h>
#include "Eagle/Utils/PlatformUtils.h"
#include "Eagle/Components/Components.h"
#include "Eagle/Audio/AudioEngine.h"

namespace Eagle::UI
{
	static constexpr int s_IDBufferSize = 32;
	static uint64_t s_ID = 0;
	static char s_IDBuffer[s_IDBufferSize];

	static ButtonType DrawButtons(ButtonType buttons)
	{
		ButtonType pressedButton = ButtonType::None;

		if (buttons & ButtonType::OK)
		{
			if (ImGui::Button("OK", ImVec2(120, 0)))
			{
				pressedButton = ButtonType::OK;
				ImGui::CloseCurrentPopup();
			}
			ImGui::SetItemDefaultFocus();
			ImGui::SameLine();
		}
		if (buttons & ButtonType::Yes)
		{
			if (ImGui::Button("Yes", ImVec2(120, 0)))
			{
				pressedButton = ButtonType::Yes;
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
		}
		if (buttons & ButtonType::No)
		{
			if (ImGui::Button("No", ImVec2(120, 0)))
			{
				pressedButton = ButtonType::No;
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
		}
		if (buttons & ButtonType::Cancel)
		{
			if (ImGui::Button("Cancel", ImVec2(120, 0)))
			{
				pressedButton = ButtonType::Cancel;
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
		}
	
		return pressedButton;
	}

	static void UpdateIDBuffer(const std::string& label)
	{
		s_IDBuffer[0] = '#';
		s_IDBuffer[1] = '#';
		memset(s_IDBuffer + 2, 0, s_IDBufferSize - 2);

		for (int i = 0; i < (s_IDBufferSize - 2) && i < label.size(); ++i)
		{
			s_IDBuffer[i + 2] = label[i];
		}
	}

	bool DrawTextureSelection(const std::string& label, Ref<Texture>& modifyingTexture, bool bLoadAsSRGB)
	{
		std::string textureName = "None";
		uint32_t rendererID;
		bool bResult = false;
		ImGui::Text(label.c_str());
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);

		if (modifyingTexture)
		{
			rendererID = modifyingTexture->GetNonSRGBRendererID();
			textureName = modifyingTexture->GetPath().stem().u8string();
		}
		else
			rendererID = Texture2D::NoneTexture->GetRendererID();
			
		const std::string comboID = std::string("##") + label;
		const char* comboItems[] = { "None", "Black", "White" };
		constexpr int basicSize = 3; //above size
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
				bResult = modifyingTexture != texture;
				if (bResult)
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
		
		ImGui::PopItemWidth();
		ImGui::NextColumn();

		return bResult;
	}

	bool DrawStaticMeshSelection(const std::string& label, Ref<StaticMesh>& staticMesh, const std::string& helpMessage)
	{
		bool bResult = false;

		ImGui::Text(label.c_str());
		if (helpMessage.size())
		{
			ImGui::SameLine();
			UI::HelpMarker(helpMessage);
		}
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);

		const std::string& smName = staticMesh ? staticMesh->GetName() : "None";
		const std::string comboID = std::string("##") + label;
		const char* comboItems[] = { "None" };
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
				bResult = staticMesh != mesh;
				if (bResult)
					staticMesh = mesh;
			}

			ImGui::EndDragDropTarget();
		}

		if (bBeginCombo)
		{
			currentItemIdx = -1;
			////Initially find currently selected static mesh to scroll to it.
			if (staticMesh)
			{
				const auto& allStaticMeshes = StaticMeshLibrary::GetMeshes();
				for (int i = 0; i < allStaticMeshes.size(); ++i)
				{
					if (allStaticMeshes[i] == staticMesh)
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
						case 0: //None
						{
							bResult = staticMesh.operator bool(); //Result should be false if mesh was already None
							staticMesh.reset();
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

					staticMesh = allStaticMeshes[i];
					bResult = true;
				}
			}
			ImGui::EndCombo();
		}

		ImGui::PopItemWidth();
		ImGui::NextColumn();

		return bResult;
	}

	bool DrawSoundSelection(const std::string& label, std::filesystem::path& selectedSoundPath)
	{
		bool bResult = false;

		ImGui::Text(label.c_str());
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);

		bool bNone = selectedSoundPath.empty();
		const std::string& soundName = bNone ? "None" : selectedSoundPath.filename().u8string();
		const std::string comboID = std::string("##") + label;
		const char* comboItems[] = { "None" };
		constexpr int basicSize = 1; //above size
		static int currentItemIdx = -1; // Here our selection data is an index.
		bool bBeginCombo = ImGui::BeginCombo(comboID.c_str(), soundName.c_str(), 0);

		//Drop event
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SOUND_CELL"))
			{
				const wchar_t* payload_n = (const wchar_t*)payload->Data;
				std::filesystem::path path = std::filesystem::path(payload_n);
				bResult = selectedSoundPath != path;
				if (bResult)
					selectedSoundPath = path;
			}

			ImGui::EndDragDropTarget();
		}

		if (bBeginCombo)
		{
			currentItemIdx = -1;
			////Initially find currently selected sound to scroll to it.
			if (!bNone)
			{
				auto& sounds = AudioEngine::GetLoadedSounds();
				int i = 0;
				for (auto& it : sounds)
				{
					if (std::filesystem::absolute(selectedSoundPath).u8string() == it.first)
					{
						currentItemIdx = i + basicSize;
						break;
					}
					i++;
				}
			}

			//Drawing basic combo items
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
							bResult = !bNone; //Result should be false if sound was already None
							selectedSoundPath = std::filesystem::path();
							break;
						}
					}
				}
			}

			//Drawing all existing meshes
			const auto& sounds = AudioEngine::GetLoadedSounds();
			int i = 0;
			for (auto& it : sounds)
			{
				const bool bSelected = (currentItemIdx == i + basicSize);
				std::filesystem::path path = it.first;
				std::string soundName = path.filename().u8string();

				if (ImGui::Selectable(soundName.c_str(), bSelected, 0, ImVec2{ ImGui::GetContentRegionAvailWidth(), 32 }))
					currentItemIdx = i + basicSize;

				// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
				if (bSelected)
					ImGui::SetItemDefaultFocus();

				if (ImGui::IsItemClicked())
				{
					currentItemIdx = i + basicSize;
					selectedSoundPath = path;
					bResult = true;
				}
			}
			ImGui::EndCombo();
		}

		ImGui::PopItemWidth();
		ImGui::NextColumn();

		return bResult;
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
		UI::Tooltip(std::to_string(values.x));
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
		UI::Tooltip(std::to_string(values.y));
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
		UI::Tooltip(std::to_string(values.z));

		ImGui::PopStyleVar();

		ImGui::Columns(1);

		ImGui::PopID();

		return bValueChanged;
	}

	void BeginPropertyGrid(const std::string& gridName)
	{
		ImGui::PushID(gridName.c_str());
		ImGui::Columns(2);
	}

	void EndPropertyGrid()
	{
		ImGui::Columns(1);
		ImGui::PopID();
	}

	bool Property(const std::string& label, std::string& value, const std::string& helpMessage)
	{
		static char buffer[256];
		bool bModified = false;
		
		UpdateIDBuffer(label);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		ImGui::Text(label.c_str());
		if (helpMessage.size())
		{
			ImGui::SameLine();
			UI::HelpMarker(helpMessage);
		}
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);

		strcpy_s(buffer, 256, value.c_str());

		if (ImGui::InputText(s_IDBuffer, buffer, 256))
		{
			value = buffer;
			bModified = true;
		}

		ImGui::PopItemWidth();
		ImGui::NextColumn();

		return bModified;
	}

	bool Property(const std::string& label, bool& value, const std::string& helpMessage)
	{
		bool bModified = false;

		UpdateIDBuffer(label);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		ImGui::Text(label.c_str());
		if (helpMessage.size())
		{
			ImGui::SameLine();
			UI::HelpMarker(helpMessage);
		}
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);

		bModified = ImGui::Checkbox(s_IDBuffer, &value);

		ImGui::PopItemWidth();
		ImGui::NextColumn();

		return bModified;
	}

	bool Property(const std::string& label, const std::vector<std::string>& customLabels, bool* values, const std::string& helpMessage)
	{
		bool bModified = false;

		UpdateIDBuffer(label);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		ImGui::Text(label.c_str());
		if (helpMessage.size())
		{
			ImGui::SameLine();
			UI::HelpMarker(helpMessage);
		}
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);
		
		size_t count = customLabels.size();
		for (size_t i = 0; i < count; ++i)
		{	
			UpdateIDBuffer(label + customLabels[i]);
			ImGui::Text(customLabels[i].c_str());
			ImGui::SameLine();
			bModified |= ImGui::Checkbox(s_IDBuffer, &values[i]);
			if (i != (count - 1))
				ImGui::SameLine();
		}

		ImGui::PopItemWidth();
		ImGui::NextColumn();

		return bModified;
	}

	bool PropertyText(const std::string& label, const std::string& text)
	{
		ImGui::Text(label.c_str());
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);
		ImGui::Text(text.c_str());
		ImGui::PopItemWidth();
		ImGui::NextColumn();
		return false;
	}

	bool PropertyDrag(const std::string& label, int& value, float speed, int min, int max, const std::string& helpMessage)
	{
		bool bModified = false;

		UpdateIDBuffer(label);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		ImGui::Text(label.c_str());
		if (helpMessage.size())
		{
			ImGui::SameLine();
			UI::HelpMarker(helpMessage);
		}
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);
		bModified = ImGui::DragInt(s_IDBuffer, &value, speed, min, max);
		ImGui::PopItemWidth();
		ImGui::NextColumn();
		return bModified;
	}

	bool PropertyDrag(const std::string& label, float& value, float speed, float min, float max, const std::string& helpMessage)
	{
		bool bModified = false;

		UpdateIDBuffer(label);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		ImGui::Text(label.c_str());
		if (helpMessage.size())
		{
			ImGui::SameLine();
			UI::HelpMarker(helpMessage);
		}
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);

		bModified = ImGui::DragFloat(s_IDBuffer, &value);
		ImGui::PopItemWidth();
		ImGui::NextColumn();
		return bModified;
	}

	bool PropertyDrag(const std::string& label, glm::vec2& value, float speed, float min, float max, const std::string& helpMessage)
	{
		bool bModified = false;

		UpdateIDBuffer(label);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		ImGui::Text(label.c_str());
		if (helpMessage.size())
		{
			ImGui::SameLine();
			UI::HelpMarker(helpMessage);
		}
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);

		bModified = ImGui::DragFloat2(s_IDBuffer, &value.x, speed, min, max);
		ImGui::PopItemWidth();
		ImGui::NextColumn();
		return bModified;
	}

	bool PropertyDrag(const std::string& label, glm::vec3& value, float speed, float min, float max, const std::string& helpMessage)
	{
		bool bModified = false;

		UpdateIDBuffer(label);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		ImGui::Text(label.c_str());
		if (helpMessage.size())
		{
			ImGui::SameLine();
			UI::HelpMarker(helpMessage);
		}
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);

		bModified = ImGui::DragFloat3(s_IDBuffer, &value.x, speed, min, max);
		ImGui::PopItemWidth();
		ImGui::NextColumn();
		return bModified;
	}

	bool PropertyDrag(const std::string& label, glm::vec4& value, float speed, float min, float max, const std::string& helpMessage)
	{
		bool bModified = false;

		UpdateIDBuffer(label);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		ImGui::Text(label.c_str());
		if (helpMessage.size())
		{
			ImGui::SameLine();
			UI::HelpMarker(helpMessage);
		}
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);

		bModified = ImGui::DragFloat4(s_IDBuffer, &value.x, speed, min, max);
		ImGui::PopItemWidth();
		ImGui::NextColumn();
		return bModified;
	}

	bool PropertySlider(const std::string& label, int& value, int min, int max, const std::string& helpMessage)
	{
		bool bModified = false;

		UpdateIDBuffer(label);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		ImGui::Text(label.c_str());
		if (helpMessage.size())
		{
			ImGui::SameLine();
			UI::HelpMarker(helpMessage);
		}
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);

		bModified = ImGui::SliderInt(s_IDBuffer, &value, min, max);
		ImGui::PopItemWidth();
		ImGui::NextColumn();
		return bModified;
	}

	bool PropertySlider(const std::string& label, float& value, float min, float max, const std::string& helpMessage)
	{
		bool bModified = false;

		UpdateIDBuffer(label);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		ImGui::Text(label.c_str());
		if (helpMessage.size())
		{
			ImGui::SameLine();
			UI::HelpMarker(helpMessage);
		}
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);

		bModified = ImGui::SliderFloat(s_IDBuffer, &value, min, max);
		ImGui::PopItemWidth();
		ImGui::NextColumn();
		return bModified;
	}

	bool PropertySlider(const std::string& label, glm::vec2& value, float min, float max, const std::string& helpMessage)
	{
		bool bModified = false;

		UpdateIDBuffer(label);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		ImGui::Text(label.c_str());
		if (helpMessage.size())
		{
			ImGui::SameLine();
			UI::HelpMarker(helpMessage);
		}
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);

		bModified = ImGui::SliderFloat2(s_IDBuffer, &value.x, min, max);
		ImGui::PopItemWidth();
		ImGui::NextColumn();
		return bModified;
	}

	bool PropertySlider(const std::string& label, glm::vec3& value, float min, float max, const std::string& helpMessage)
	{
		bool bModified = false;

		UpdateIDBuffer(label);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		ImGui::Text(label.c_str());
		if (helpMessage.size())
		{
			ImGui::SameLine();
			UI::HelpMarker(helpMessage);
		}
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);

		bModified = ImGui::SliderFloat3(s_IDBuffer, &value.x, min, max);
		ImGui::PopItemWidth();
		ImGui::NextColumn();
		return bModified;
	}

	bool PropertySlider(const std::string& label, glm::vec4& value, float min, float max, const std::string& helpMessage)
	{
		bool bModified = false;

		UpdateIDBuffer(label);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		ImGui::Text(label.c_str());
		if (helpMessage.size())
		{
			ImGui::SameLine();
			UI::HelpMarker(helpMessage);
		}
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);

		bModified = ImGui::SliderFloat4(s_IDBuffer, &value.x, min, max);
		ImGui::PopItemWidth();
		ImGui::NextColumn();
		return bModified;
	}

	bool PropertyColor(const std::string& label, glm::vec3& value, const std::string& helpMessage)
	{
		bool bModified = false;

		UpdateIDBuffer(label);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		ImGui::Text(label.c_str());
		if (helpMessage.size())
		{
			ImGui::SameLine();
			UI::HelpMarker(helpMessage);
		}
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);

		bModified = ImGui::ColorEdit3(s_IDBuffer, &value.x);
		ImGui::PopItemWidth();
		ImGui::NextColumn();
		return bModified;
	}

	bool PropertyColor(const std::string& label, glm::vec4& value, const std::string& helpMessage)
	{
		bool bModified = false;

		UpdateIDBuffer(label);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		ImGui::Text(label.c_str());
		if (helpMessage.size())
		{
			ImGui::SameLine();
			UI::HelpMarker(helpMessage);
		}
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);

		bModified = ImGui::ColorEdit4(s_IDBuffer, &value.x);
		ImGui::PopItemWidth();
		ImGui::NextColumn();
		return bModified;
	}

	bool InputFloat(const std::string& label, float& value, float step, float stepFast, const std::string& helpMessage)
	{
		UpdateIDBuffer(label);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		ImGui::Text(label.c_str());
		if (helpMessage.size())
		{
			ImGui::SameLine();
			UI::HelpMarker(helpMessage);
		}
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);

		bool result = ImGui::InputFloat(s_IDBuffer, &value, step, stepFast);

		ImGui::PopItemWidth();
		ImGui::NextColumn();
		return result;
	}

	bool Combo(const std::string& label, uint32_t currentSelection, const std::vector<std::string>& options, int& outSelectedIndex, const std::vector<std::string>& tooltips, const std::string& helpMessage)
	{
		const std::string& currentString = options[currentSelection];
		size_t tooltipsSize = tooltips.size();
		bool bModified = false;
		UpdateIDBuffer(label);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		ImGui::Text(label.c_str());
		if (helpMessage.size())
		{
			ImGui::SameLine();
			UI::HelpMarker(helpMessage);
		}
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);

		if (ImGui::BeginCombo(s_IDBuffer, currentString.c_str()))
		{
			for (int i = 0; i < options.size(); ++i)
			{
				bool isSelected = (currentString == options[i]);

				if (ImGui::Selectable(options[i].c_str(), isSelected))
				{
					bModified = true;
					outSelectedIndex = i;
				}

				if (i < tooltipsSize)
					if (!tooltips[i].empty())
						Tooltip(tooltips[i]);

				if (isSelected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		if (currentSelection < tooltipsSize)
			if (!tooltips[currentSelection].empty())
				Tooltip(tooltips[currentSelection]);

		ImGui::PopItemWidth();
		ImGui::NextColumn();
		return bModified;
	}

	bool Button(const std::string& label, const std::string& buttonText, const ImVec2& size)
	{
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		ImGui::Text(label.c_str());
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);

		bool result = ImGui::Button(buttonText.c_str(), size);

		ImGui::PopItemWidth();
		ImGui::NextColumn();
		return result;
	}

	void Tooltip(const std::string& tooltip, float treshHold)
	{
		if (ImGui::IsItemHovered() && GImGui->HoveredIdTimer > treshHold)
		{
			ImGui::BeginTooltip();
			ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
			ImGui::TextUnformatted(tooltip.c_str());
			ImGui::PopTextWrapPos();
			ImGui::EndTooltip();
		}
	}

	void PushItemDisabled()
	{
		ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
	}

	void PopItemDisabled()
	{
		ImGui::PopItemFlag();
		ImGui::PopStyleVar();
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

	ButtonType ShowMessage(const std::string& title, const std::string& message, ButtonType buttons)
	{
		ButtonType pressedButton = ButtonType::None;
		ImGui::OpenPopup(title.c_str());

		// Always center this window when appearing
		ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

		if (ImGui::BeginPopupModal(title.c_str(), NULL, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::Text(message.c_str());
			ImGui::Separator();

			pressedButton = DrawButtons(buttons);

			ImGui::EndPopup();
		}

		return pressedButton;
	}
	
	ButtonType InputPopup(const std::string& title, const std::string& hint, std::string& input)
	{
		ButtonType buttons = ButtonType::OKCancel;

		ButtonType pressedButton = ButtonType::None;
		ImGui::OpenPopup(title.c_str());

		// Always center this window when appearing
		ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

		if (ImGui::BeginPopupModal(title.c_str(), NULL, ImGuiWindowFlags_AlwaysAutoResize))
		{
			char buf[128] = {0};
			memcpy_s(buf, sizeof(buf), input.c_str(), input.size());
			if (ImGui::InputTextWithHint("##InputPopup", hint.c_str(), buf, sizeof(buf)))
				input = buf;

			ImGui::Separator();

			//Manually drawing OK because it needs to be disabled if input is empty.
			if (buttons & ButtonType::OK)
			{
				if (input.size() == 0)
					UI::PushItemDisabled();
				if (ImGui::Button("OK", ImVec2(120, 0)))
				{
					pressedButton = ButtonType::OK;
					ImGui::CloseCurrentPopup();
				}
				if (input.size() == 0)
					UI::PopItemDisabled();
				ImGui::SetItemDefaultFocus();
				ImGui::SameLine();
				buttons = ButtonType(buttons & (~ButtonType::OK)); //Removing OK from mask to draw buttons as usual without OK cuz we already drew it
			}
			pressedButton = DrawButtons(buttons);
			
			ImGui::EndPopup();
		}

		return pressedButton;
	}
}

namespace Eagle::UI::TextureViewer
{
	void OpenTextureViewer(const Ref<Texture>& textureToView, bool* outWindowOpened)
	{
		bool bHidden = !ImGui::Begin("Texture Viewer", outWindowOpened);
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
			static const std::string sRGBHelpMessage = "Most of the times 'sRGB' needs to be checked for diffuse textures and unchecked for other texture types.";
			std::string textureSizeString = std::to_string((int)textureSize.x) + "x" + std::to_string((int)textureSize.y);
			ImGui::Begin("Details");
			detailsDocked = ImGui::IsWindowDocked();
			UI::BeginPropertyGrid("TextureViewDetails");
			UI::PropertyText("Name", textureToView->GetPath().filename().u8string());
			UI::PropertyText("Resolution", textureSizeString);
			bool bSRGB = textureToView->IsSRGB();
			if (UI::Property("sRGB", bSRGB, sRGBHelpMessage))
				textureToView->SetSRGB(bSRGB);
			UI::EndPropertyGrid();
			ImGui::End();
		}

		ImGui::End();
	}
}
