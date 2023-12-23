#include "egpch.h"

#include "UI.h"
#include "Font.h"

#include "Eagle/Utils/PlatformUtils.h"
#include "Eagle/Components/Components.h"
#include "Eagle/Audio/AudioEngine.h"

#include "Platform/Vulkan/VulkanImage.h"
#include "Platform/Vulkan/VulkanTexture2D.h"
#include "Platform/Vulkan/VulkanUtils.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_vulkan.h>

namespace Eagle::UI
{
	static constexpr int s_IDBufferSize = 32;
	static uint64_t s_ID = 0;
	static char s_IDBuffer[s_IDBufferSize];
	static const VkImageLayout s_VulkanImageLayout = ImageLayoutToVulkan(ImageReadAccess::PixelShaderRead);

	int TextResizeCallback(ImGuiInputTextCallbackData* data)
	{
		if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
		{
			// Resize string callback
			// If for some reason we refuse the new length (BufTextLen) and/or capacity (BufSize) we need to set them back to what we want.
			std::string* str = (std::string*)data->UserData;
			assert(data->Buf == str->c_str());
			str->resize(data->BufTextLen);
			data->Buf = str->data();
		}
		return 0;
	}

	static ButtonType DrawButtons(ButtonType buttons)
	{
		ButtonType pressedButton = ButtonType::None;

		if (HasFlags(buttons, ButtonType::OK))
		{
			if (ImGui::Button("OK", ImVec2(120, 0)))
			{
				pressedButton = ButtonType::OK;
				ImGui::CloseCurrentPopup();
			}
			ImGui::SetItemDefaultFocus();
			ImGui::SameLine();
		}
		if (HasFlags(buttons, ButtonType::Yes))
		{
			if (ImGui::Button("Yes", ImVec2(120, 0)))
			{
				pressedButton = ButtonType::Yes;
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
		}
		if (HasFlags(buttons, ButtonType::No))
		{
			if (ImGui::Button("No", ImVec2(120, 0)))
			{
				pressedButton = ButtonType::No;
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
		}
		if (HasFlags(buttons, ButtonType::Cancel))
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

	void UpdateIDBuffer(const std::string_view label)
	{
		s_IDBuffer[0] = '#';
		s_IDBuffer[1] = '#';
		memset(s_IDBuffer + 2, 0, s_IDBufferSize - 2);

		for (int i = 0; i < (s_IDBufferSize - 2) && i < label.size(); ++i)
		{
			s_IDBuffer[i + 2] = label[i];
		}
	}

	const char* GetIDBuffer()
	{
		return s_IDBuffer;
	}

	bool DrawTexture2DSelection(const std::string_view label, Ref<Texture2D>& modifyingTexture, const std::string_view helpMessage)
	{
		std::string textureName = "None";
		bool bResult = false;
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6.f);
		ImGui::Text(label.data());
		if (helpMessage.size())
		{
			ImGui::SameLine();
			UI::HelpMarker(helpMessage);
		}
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);
		const bool bTextureValid = modifyingTexture.operator bool();

		if (bTextureValid)
			textureName = modifyingTexture->GetPath().stem().u8string();
			
		const std::string comboID = std::string("##") + std::string(label);
		const char* comboItems[] = { "None", "Black", "Gray", "White", "Red", "Green", "Blue" };
		constexpr int basicSize = sizeof(comboItems) / sizeof(char*); //above size
		static int currentItemIdx = -1; // Here our selection data is an index.

		UI::Image(bTextureValid ? modifyingTexture : Texture2D::NoneIconTexture, { 32, 32 });
		ImGui::SameLine();

		//Drop event
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("TEXTURE_CELL"))
			{
				const wchar_t* payload_n = (const wchar_t*)payload->Data;
				Path filepath(payload_n);
				Ref<Texture> texture;

				if (TextureLibrary::Get(filepath, &texture) == false)
				{
					texture = Texture2D::Create(filepath);
				}
				bResult = modifyingTexture != texture;
				if (bResult)
					modifyingTexture = Cast<Texture2D>(texture);
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
				else if (modifyingTexture == Texture2D::GrayTexture)
				{
					currentItemIdx = 2;
					bFound = true;
				}
				else if (modifyingTexture == Texture2D::WhiteTexture)
				{
					currentItemIdx = 3;
					bFound = true;
				}
				else if (modifyingTexture == Texture2D::RedTexture)
				{
					currentItemIdx = 4;
					bFound = true;
				}
				else if (modifyingTexture == Texture2D::GreenTexture)
				{
					currentItemIdx = 5;
					bFound = true;
				}
				else if (modifyingTexture == Texture2D::BlueTexture)
				{
					currentItemIdx = 6;
					bFound = true;
				}
				const auto& allTextures = TextureLibrary::GetTextures();

				if (!bFound)
				{
					uint32_t i = 0;
					for (const auto& data : allTextures)
					{
						const auto& texture = data.second;
						if (texture == modifyingTexture)
						{
							currentItemIdx = i + basicSize;
							break;
						}
						i++;
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
					case 2: //Gray
					{
						modifyingTexture = Texture2D::GrayTexture;
						bResult = true;
						break;
					}
					case 3: //White
					{
						modifyingTexture = Texture2D::WhiteTexture;
						bResult = true;
						break;
					}
					case 4: //Red
					{
						modifyingTexture = Texture2D::RedTexture;
						bResult = true;
						break;
					}
					case 5: //Green
					{
						modifyingTexture = Texture2D::GreenTexture;
						bResult = true;
						break;
					}
					case 6: //Blue
					{
						modifyingTexture = Texture2D::BlueTexture;
						bResult = true;
						break;
					}
					}
				}
			}

			//Drawing all existing textures
			const auto& allTextures = TextureLibrary::GetTextures();
			uint32_t i = 0;
			for (const auto& data : allTextures)
			{
				const auto& texture = data.second;
				Ref<Texture2D> currentTexture = Cast<Texture2D>(texture);
				if (!currentTexture)
				{
					++i;
					continue;
				}

				const bool bSelected = (currentItemIdx == i + basicSize);
				ImGui::PushID((int)texture->GetGUID().GetHash());
				bool bSelectableTriggered = ImGui::Selectable("##label", bSelected, ImGuiSelectableFlags_AllowItemOverlap, {0.0f, 32.f});
				bool bSelectableClicked = ImGui::IsItemClicked();
				ImGui::SameLine();
				UI::Image(currentTexture, { 32, 32 });

				ImGui::SameLine();
				const Path& path = texture->GetPath();
				ImGui::Text("%s", path.stem().u8string().c_str());
				if (bSelectableTriggered)
					currentItemIdx = i + basicSize;

				// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
				if (bSelected)
					ImGui::SetItemDefaultFocus();

				if (bSelectableClicked)
				{
					currentItemIdx = i + basicSize;

					modifyingTexture = currentTexture;
					bResult = true;
				}
				ImGui::PopID();
				++i;
			}

			ImGui::EndCombo();
		}
		
		ImGui::PopItemWidth();
		ImGui::NextColumn();

		return bResult;
	}

	bool DrawTextureCubeSelection(const std::string_view label, Ref<TextureCube>& modifyingTexture)
	{
		std::string textureName = "None";
		bool bResult = false;
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6.f);
		ImGui::Text(label.data());
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);
		bool bTextureValid = modifyingTexture.operator bool();

		if (bTextureValid)
		{
			textureName = modifyingTexture->GetPath().stem().u8string();
		}

		const std::string comboID = std::string("##") + std::string(label);
		const char* comboItems[] = { "None" };
		constexpr int basicSize = 1; //above size
		static int currentItemIdx = -1; // Here our selection data is an index.

		UI::Image(bTextureValid ? modifyingTexture->GetTexture2D() : Texture2D::NoneIconTexture, {32, 32});
		ImGui::SameLine();

		//Drop event
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("TEXTURE_CUBE_CELL"))
			{
				const wchar_t* payload_n = (const wchar_t*)payload->Data;
				Path filepath(payload_n);
				Ref<Texture> texture;

				if (TextureLibrary::Get(filepath, &texture) == false)
				{
					texture = TextureCube::Create(filepath, TextureCube::SkyboxSize);
				}
				bResult = modifyingTexture != texture;
				if (bResult)
					modifyingTexture = Cast<TextureCube>(texture);
			}

			ImGui::EndDragDropTarget();
		}

		if (ImGui::BeginCombo(comboID.c_str(), textureName.c_str(), 0))
		{
			//Initially find currently selected texture to scroll to it.
			if (modifyingTexture)
			{
				bool bFound = false;
				uint32_t i = 0;
				const auto& allTextures = TextureLibrary::GetTextures();
				for (auto& data : allTextures)
				{
					const auto& texture = data.second;
					if (texture == modifyingTexture)
					{
						currentItemIdx = i + basicSize;
						break;
					}
					i++;
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
					}
				}
			}

			//Drawing all existing textures
			uint32_t i = 0;
			const auto& allTextures = TextureLibrary::GetTextures();
			for (auto& data : allTextures)
			{
				const auto& texture = data.second;
				Ref<TextureCube> currentTexture = Cast<TextureCube>(texture);
				if (!currentTexture)
				{
					++i;
					continue;
				}

				const bool bSelected = (currentItemIdx == i + basicSize);
				ImGui::PushID((int)texture->GetGUID().GetHash());
				bool bSelectableTriggered = ImGui::Selectable("##label", bSelected, ImGuiSelectableFlags_AllowItemOverlap, { 0.0f, 32.f });
				bool bSelectableClicked = ImGui::IsItemClicked();
				ImGui::SameLine();
				UI::Image(currentTexture->GetTexture2D(), {32, 32});

				ImGui::SameLine();
				const Path& path = texture->GetPath();
				ImGui::Text("%s", path.stem().u8string().c_str());
				if (bSelectableTriggered)
					currentItemIdx = i + basicSize;

				// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
				if (bSelected)
					ImGui::SetItemDefaultFocus();

				if (bSelectableClicked)
				{
					currentItemIdx = i + basicSize;

					modifyingTexture = currentTexture;
					bResult = true;
				}
				ImGui::PopID();
				++i;
			}

			ImGui::EndCombo();
		}

		ImGui::PopItemWidth();
		ImGui::NextColumn();

		return bResult;
	}

	bool DrawStaticMeshSelection(const std::string_view label, Ref<StaticMesh>& staticMesh, const std::string_view helpMessage)
	{
		bool bResult = false;

		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		ImGui::Text(label.data());
		if (helpMessage.size())
		{
			ImGui::SameLine();
			UI::HelpMarker(helpMessage);
		}
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);

		const std::string& smName = staticMesh ? staticMesh->GetName() : "None";
		const std::string comboID = std::string("##") + std::string(label);
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
				Path filepath(payload_n);
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
				if (ImGui::Selectable(smName.c_str(), bSelected, 0, ImVec2{ ImGui::GetContentRegionAvail().x, 32 }))
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

	bool DrawFontSelection(const std::string_view label, Ref<Font>& modifyingFont, const std::string_view helpMessage)
	{
		bool bResult = false;

		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		ImGui::Text(label.data());
		if (helpMessage.size())
		{
			ImGui::SameLine();
			UI::HelpMarker(helpMessage);
		}
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);

		const bool bValid = modifyingFont.operator bool();
		const std::string& name = bValid ? modifyingFont->GetPath().stem().u8string() : "None";
		const std::string comboID = std::string("##") + std::string(label);
		const char* comboItems[] = { "None" };
		constexpr int basicSize = 1; //above size
		static int currentItemIdx = -1; // Here our selection data is an index.
		bool bBeginCombo = ImGui::BeginCombo(comboID.c_str(), name.c_str(), 0);

		//Drop event
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("FONT_CELL"))
			{
				const wchar_t* payload_n = (const wchar_t*)payload->Data;
				Path filepath(payload_n);
				Ref<Font> font;

				if (FontLibrary::Get(filepath, &font) == false)
				{
					font = Font::Create(filepath);
				}
				bResult = modifyingFont != font;
				if (bResult)
					modifyingFont = font;
			}

			ImGui::EndDragDropTarget();
		}

		if (bBeginCombo)
		{
			currentItemIdx = -1;
			// Initially find currently selected font to scroll to it.
			if (modifyingFont)
			{
				uint32_t i = 0;
				const auto& allFonts = FontLibrary::GetFonts();
				for (auto& data : allFonts)
				{
					const auto& font = data.second;
					if (font == modifyingFont)
					{
						currentItemIdx = i + basicSize;
						break;
					}
					i++;
				}
			}
			// Drawing basic (new) combo items
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
							bResult = bValid; //Result should be false if mesh was already None
							modifyingFont.reset();
							break;
						}
					}
				}
			}

			//Drawing all existing fonts
			uint32_t i = 0;
			const auto& allFonts = FontLibrary::GetFonts();
			for (auto& data : allFonts)
			{
				const auto& font = data.second;
				const bool bSelected = (currentItemIdx == i + basicSize);

				const std::string& name = font->GetPath().stem().u8string();
				if (ImGui::Selectable(name.c_str(), bSelected, 0, ImVec2{ ImGui::GetContentRegionAvail().x, 32 }))
					currentItemIdx = i + basicSize;

				// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
				if (bSelected)
					ImGui::SetItemDefaultFocus();

				if (ImGui::IsItemClicked())
				{
					currentItemIdx = i + basicSize;

					modifyingFont = font;
					bResult = true;
				}
				i++;
			}
			ImGui::EndCombo();
		}

		ImGui::PopItemWidth();
		ImGui::NextColumn();

		return bResult;
	}

	bool DrawSoundSelection(const std::string_view label, Path& selectedSoundPath)
	{
		bool bResult = false;

		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		ImGui::Text(label.data());
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);

		bool bNone = selectedSoundPath.empty();
		const std::string& soundName = bNone ? "None" : selectedSoundPath.filename().u8string();
		const std::string comboID = std::string("##") + std::string(label);
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
				Path path = Path(payload_n);
				bResult = selectedSoundPath != path;
				if (bResult)
					selectedSoundPath = path;
			}

			ImGui::EndDragDropTarget();
		}

		if (bBeginCombo)
		{
			currentItemIdx = -1;
			// Initially find currently selected sound to scroll to it.
			if (!bNone)
			{
				const std::string selectedSoundStr = selectedSoundPath.u8string();
				auto& sounds = AudioEngine::GetLoadedSounds();
				int i = 0;
				for (auto& it : sounds)
				{
					if (selectedSoundStr == it.first)
					{
						currentItemIdx = i + basicSize;
						break;
					}
					i++;
				}
			}

			// Drawing basic combo items
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
							selectedSoundPath = Path();
							break;
						}
					}
				}
			}

			//Drawing all existing sounds
			const auto& sounds = AudioEngine::GetLoadedSounds();
			int i = 0;
			for (auto& it : sounds)
			{
				const bool bSelected = (currentItemIdx == i + basicSize);
				Path path = it.first;
				std::string soundName = path.filename().u8string();

				if (ImGui::Selectable(soundName.c_str(), bSelected, 0, ImVec2{ ImGui::GetContentRegionAvail().x, 32 }))
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
				++i;
			}
			ImGui::EndCombo();
		}

		ImGui::PopItemWidth();
		ImGui::NextColumn();

		return bResult;
	}

	bool DrawVec3Control(const std::string_view label, glm::vec3& values, const glm::vec3 resetValues /* = glm::vec3{ 0.f }*/, float columnWidth /*= 100.f*/)
	{
		bool bValueChanged = false;
		ImGuiIO& io = ImGui::GetIO();
		auto boldFont = io.Fonts->Fonts[0];

		ImGui::PushID(label.data());

		ImGui::Columns(2, nullptr, false);
		ImGui::SetColumnWidth(0, columnWidth);
		ImGui::Text(label.data());
		ImGui::NextColumn();

		ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0.f, 0.f });

		float lineHeight = (GImGui->Font->FontSize * boldFont->Scale) + GImGui->Style.FramePadding.y * 2.f;
		ImVec2 buttonSize = { lineHeight + 3.f, lineHeight };

		//X
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.8f, 0.1f, 0.15f, 1.f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.9f, 0.2f, 0.2f, 1.f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.8f, 0.1f, 0.15f, 1.f });
		ImGui::PushItemFlag(ImGuiItemFlags_NoTabStop, true);
		ImGui::PushFont(boldFont);
		if (ImGui::Button("X", buttonSize))
		{
			values.x = resetValues.x;
			bValueChanged = true;
		}
		ImGui::PopFont();
		ImGui::PopItemFlag();
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
		ImGui::PushItemFlag(ImGuiItemFlags_NoTabStop, true);
		ImGui::PushFont(boldFont);
		if (ImGui::Button("Y", buttonSize))
		{
			values.y = resetValues.y;
			bValueChanged = true;
		}
		ImGui::PopFont();
		ImGui::PopItemFlag();
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
		ImGui::PushItemFlag(ImGuiItemFlags_NoTabStop, true);
		ImGui::PushFont(boldFont);
		if (ImGui::Button("Z", buttonSize))
		{
			values.z = resetValues.z;
			bValueChanged = true;
		}
		ImGui::PopFont();
		ImGui::PopItemFlag();
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

	void BeginPropertyGrid(const std::string_view gridName)
	{
		ImGui::PushID(gridName.data());
		ImGui::Columns(2);
	}

	void EndPropertyGrid()
	{
		ImGui::Columns(1);
		ImGui::PopID();
	}

	bool Property(const std::string_view label, bool& value, const std::string_view helpMessage)
	{
		bool bModified = false;

		UpdateIDBuffer(label);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		ImGui::Text(label.data());
		if (helpMessage.size())
		{
			ImGui::SameLine();
			UI::HelpMarker(helpMessage);
		}
		ImGui::SameLine();
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);

		bModified = ImGui::Checkbox(s_IDBuffer, &value);

		ImGui::PopItemWidth();
		ImGui::NextColumn();

		return bModified;
	}

	bool Property(const std::string_view label, const std::vector<std::string>& customLabels, bool* values, const std::string_view helpMessage)
	{
		bool bModified = false;

		UpdateIDBuffer(label);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		ImGui::Text(label.data());
		if (helpMessage.size())
		{
			ImGui::SameLine();
			UI::HelpMarker(helpMessage);
		}
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);
		
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		const size_t count = customLabels.size();

		// It's done that way because for some reason `text` and `checkbox` are misaligned
		if (count)
		{
			size_t i = 0;
			UpdateIDBuffer(std::string(label) + customLabels[i]);
			ImGui::Text(customLabels[i].c_str());
			ImGui::SameLine();
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 3.f);
			bModified |= ImGui::Checkbox(s_IDBuffer, &values[i]);
			if (i != (count - 1))
				ImGui::SameLine();
		}

		for (size_t i = 1; i < count; ++i)
		{	
			UpdateIDBuffer(std::string(label) + customLabels[i]);
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 3.f);
			ImGui::Text(customLabels[i].c_str());
			ImGui::SameLine();
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 3.f);
			bModified |= ImGui::Checkbox(s_IDBuffer, &values[i]);
			if (i != (count - 1))
				ImGui::SameLine();
		}

		ImGui::PopItemWidth();
		ImGui::NextColumn();

		return bModified;
	}

	bool PropertyText(const std::string_view label, std::string& value, const std::string_view helpMessage)
	{
		bool bModified = false;

		UpdateIDBuffer(label);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		ImGui::Text(label.data());
		if (helpMessage.size())
		{
			ImGui::SameLine();
			UI::HelpMarker(helpMessage);
		}
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);

		if (ImGui::InputText(s_IDBuffer, value.data(), value.length() + 1, ImGuiInputTextFlags_CallbackResize, TextResizeCallback, &value))
			bModified = true;
		ImGui::SetItemKeyOwner(ImGuiMod_Alt);

		ImGui::PopItemWidth();
		ImGui::NextColumn();

		return bModified;
	}

	bool PropertyTextMultiline(const std::string_view label, std::string& value, const std::string_view helpMessage)
	{
		bool bModified = false;

		UpdateIDBuffer(label);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		ImGui::Text(label.data());
		if (helpMessage.size())
		{
			ImGui::SameLine();
			UI::HelpMarker(helpMessage);
		}
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);

		if (ImGui::InputTextMultiline(s_IDBuffer, value.data(), value.length() + 1, ImVec2{ 0, 0 }, ImGuiInputTextFlags_CtrlEnterForNewLine | ImGuiInputTextFlags_CallbackResize | ImGuiInputTextFlags_AllowTabInput, TextResizeCallback, &value))
			bModified = true;
		ImGui::SetItemKeyOwner(ImGuiMod_Alt);

		ImGui::PopItemWidth();
		ImGui::NextColumn();

		return bModified;
	}

	bool Text(const std::string_view label, const std::string_view text)
	{
		UpdateIDBuffer(label);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		ImGui::Text(label.data());
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);
		ImGui::Text(text.data());
		ImGui::PopItemWidth();
		ImGui::NextColumn();
		return false;
	}

	bool TextLink(const std::string_view text, const std::string_view url)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{ 0.333f, 0.611f, 0.839f, 1.f });
		ImGui::Text(text.data());
		const bool bClicked = ImGui::IsItemClicked();
		const bool bHovered = ImGui::IsItemHovered();
		ImGui::PopStyleColor();

		// Underline
		{
			ImVec2 min = ImGui::GetItemRectMin();
			ImVec2 max = ImGui::GetItemRectMax();
			min.y = max.y;
			ImGui::GetWindowDrawList()->AddLine(min, max, bHovered ? 0xFFFF5C25 : 0xFFD69C55, 1.0f);
		}

		if (bClicked)
			Utils::OpenLink(url);

		return bClicked;
	}

	bool BulletLink(const std::string_view text, const std::string_view url)
	{
		ImGui::Bullet();
		ImGui::SameLine();

		return TextLink(text, url);
	}

	bool PropertyDrag(const std::string_view label, int& value, float speed, int min, int max, const std::string_view helpMessage)
	{
		bool bModified = false;

		UpdateIDBuffer(label);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		ImGui::Text(label.data());
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

	bool PropertyDrag(const std::string_view label, uint32_t& value, float speed, int min, int max, const std::string_view helpMessage)
	{
		int temp = (int)value;
		const bool bChanged = PropertyDrag(label, temp, speed, min, max, helpMessage);
		if (bChanged)
			value = uint32_t(glm::max(temp, 0)); // Clamp negatives to 0 so that we don't overflow
		return bChanged;
	}

	bool PropertyDrag(const std::string_view label, float& value, float speed, float min, float max, const std::string_view helpMessage)
	{
		bool bModified = false;

		UpdateIDBuffer(label);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		ImGui::Text(label.data());
		if (helpMessage.size())
		{
			ImGui::SameLine();
			UI::HelpMarker(helpMessage);
		}
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);

		bModified = ImGui::DragFloat(s_IDBuffer, &value, speed, min, max);
		ImGui::PopItemWidth();
		ImGui::NextColumn();
		return bModified;
	}

	bool PropertyDrag(const std::string_view label, glm::vec2& value, float speed, float min, float max, const std::string_view helpMessage)
	{
		bool bModified = false;

		UpdateIDBuffer(label);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		ImGui::Text(label.data());
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

	bool PropertyDrag(const std::string_view label, glm::vec3& value, float speed, float min, float max, const std::string_view helpMessage)
	{
		bool bModified = false;

		UpdateIDBuffer(label);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		ImGui::Text(label.data());
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

	bool PropertyDrag(const std::string_view label, glm::vec4& value, float speed, float min, float max, const std::string_view helpMessage)
	{
		bool bModified = false;

		UpdateIDBuffer(label);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		ImGui::Text(label.data());
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

	bool PropertySlider(const std::string_view label, int& value, int min, int max, const std::string_view helpMessage)
	{
		bool bModified = false;

		UpdateIDBuffer(label);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		ImGui::Text(label.data());
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

	bool PropertySlider(const std::string_view label, float& value, float min, float max, const std::string_view helpMessage)
	{
		bool bModified = false;

		UpdateIDBuffer(label);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		ImGui::Text(label.data());
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

	bool PropertySlider(const std::string_view label, glm::vec2& value, float min, float max, const std::string_view helpMessage)
	{
		bool bModified = false;

		UpdateIDBuffer(label);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		ImGui::Text(label.data());
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

	bool PropertySlider(const std::string_view label, glm::vec3& value, float min, float max, const std::string_view helpMessage)
	{
		bool bModified = false;

		UpdateIDBuffer(label);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		ImGui::Text(label.data());
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

	bool PropertySlider(const std::string_view label, glm::vec4& value, float min, float max, const std::string_view helpMessage)
	{
		bool bModified = false;

		UpdateIDBuffer(label);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		ImGui::Text(label.data());
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

	bool PropertyColor(const std::string_view label, glm::vec3& value, bool bHDR, const std::string_view helpMessage)
	{
		bool bModified = false;

		UpdateIDBuffer(label);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		ImGui::Text(label.data());
		if (helpMessage.size())
		{
			ImGui::SameLine();
			UI::HelpMarker(helpMessage);
		}
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);

		ImGuiColorEditFlags flags = 0;
		if (bHDR)
			flags = ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float;

		bModified = ImGui::ColorEdit3(s_IDBuffer, &value.x, flags);
		ImGui::PopItemWidth();
		ImGui::NextColumn();
		return bModified;
	}

	bool PropertyColor(const std::string_view label, glm::vec4& value, bool bHDR, const std::string_view helpMessage)
	{
		bool bModified = false;

		UpdateIDBuffer(label);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		ImGui::Text(label.data());
		if (helpMessage.size())
		{
			ImGui::SameLine();
			UI::HelpMarker(helpMessage);
		}
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);

		ImGuiColorEditFlags flags = 0;
		if (bHDR)
			flags = ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float;

		bModified = ImGui::ColorEdit4(s_IDBuffer, &value.x, flags);
		ImGui::PopItemWidth();
		ImGui::NextColumn();
		return bModified;
	}

	bool InputFloat(const std::string_view label, float& value, float step, float stepFast, const std::string_view helpMessage)
	{
		UpdateIDBuffer(label);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		ImGui::Text(label.data());
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

	bool Combo(const std::string_view label, uint32_t currentSelection, const std::vector<std::string>& options, size_t optionsSize, int& outSelectedIndex, const std::vector<std::string>& tooltips, const std::string_view helpMessage)
	{
		currentSelection = glm::clamp(currentSelection, 0u, uint32_t(optionsSize) - 1u);
		const std::string_view currentString = options[currentSelection];
		size_t tooltipsSize = tooltips.size();
		bool bModified = false;
		UpdateIDBuffer(label);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		ImGui::Text(label.data());
		if (helpMessage.size())
		{
			ImGui::SameLine();
			UI::HelpMarker(helpMessage);
		}
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);

		if (ImGui::BeginCombo(s_IDBuffer, currentString.data()))
		{
			for (int i = 0; i < optionsSize; ++i)
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

	bool Combo(const std::string_view label, uint32_t currentSelection, const std::vector<std::string>& options, int& outSelectedIndex, const std::vector<std::string>& tooltips, const std::string_view helpMessage)
	{
		return Combo(label, currentSelection, options, options.size(), outSelectedIndex, tooltips, helpMessage);
	}

	bool ComboWithNone(const std::string_view label, int currentSelection, const std::vector<std::string>& options, int& outSelectedIndex, const std::vector<std::string>& tooltips, const std::string_view helpMessage)
	{
		size_t tooltipsSize = tooltips.size();
		bool bModified = false;
		UpdateIDBuffer(label);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		ImGui::Text(label.data());
		if (helpMessage.size())
		{
			ImGui::SameLine();
			UI::HelpMarker(helpMessage);
		}
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);

		const bool bNoneSelected = currentSelection == -1;
		const std::string& currentString = bNoneSelected ? "None" : options[currentSelection];
		if (ImGui::BeginCombo(s_IDBuffer, currentString.c_str()))
		{
			// None
			{
				if (ImGui::Selectable("None", bNoneSelected))
				{
					bModified = true;
					outSelectedIndex = -1;
				}

				if (bNoneSelected)
					ImGui::SetItemDefaultFocus();
			}

			for (int i = 0; i < options.size(); ++i)
			{
				bool isSelected = !bNoneSelected && (currentString == options[i]);

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

	bool Combo(const std::string_view label, int currentValue, const ScriptEnumFields& fields, int& outSelectedValue)
	{
		std::string_view currentString;
		for (auto& [value, name] : fields)
		{
			if (value == currentValue)
				currentString = name;
		}

		bool bModified = false;
		UpdateIDBuffer(label);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		ImGui::Text(label.data());
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);

		if (ImGui::BeginCombo(s_IDBuffer, currentString.data()))
		{
			for (auto& [value, name] : fields)
			{
				bool isSelected = (currentValue == value);

				if (ImGui::Selectable(name.c_str(), isSelected))
				{
					bModified = true;
					outSelectedValue = value;
				}

				if (isSelected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		ImGui::PopItemWidth();
		ImGui::NextColumn();
		return bModified;
	}

	bool Button(const std::string_view label, const std::string_view buttonText, const ImVec2& size)
	{
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		ImGui::Text(label.data());
		ImGui::SameLine();
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);

		bool result = ImGui::Button(buttonText.data(), size);

		ImGui::PopItemWidth();
		ImGui::NextColumn();
		return result;
	}

	void Tooltip(const std::string_view tooltip, float treshHold)
	{
		if (ImGui::IsItemHovered() && GImGui->HoveredIdTimer > treshHold)
		{
			ImGui::BeginTooltip();
			ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
			ImGui::TextUnformatted(tooltip.data());
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

	void PushFrameBGColor(const glm::vec4& color)
	{
		ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(color.r, color.g, color.b, color.a));
	}

	void PopFrameBGColor()
	{
		ImGui::PopStyleColor();
	}

	void HelpMarker(const std::string_view text)
	{
		// We don't want help marker to be disabled so we check the current state.
		// If the current state is disabled, we enable it and later restore the state
		const bool bDisabled = (ImGui::GetItemFlags() & ImGuiItemFlags_Disabled) == ImGuiItemFlags_Disabled;
		if (bDisabled)
			UI::PopItemDisabled();

		ImGui::TextDisabled("(?)");
		if (ImGui::IsItemHovered())
		{
			ImGui::BeginTooltip();
			ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
			ImGui::TextUnformatted(text.data());
			ImGui::PopTextWrapPos();
			ImGui::EndTooltip();
		}

		if (bDisabled)
			UI::PushItemDisabled();
	}

	ButtonType ShowMessage(const std::string_view title, const std::string_view message, ButtonType buttons)
	{
		ButtonType pressedButton = ButtonType::None;
		ImGui::OpenPopup(title.data());

		// Always center this window when appearing
		ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

		if (ImGui::BeginPopupModal(title.data(), NULL, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::Text(message.data());
			ImGui::Separator();

			pressedButton = DrawButtons(buttons);

			ImGui::EndPopup();
		}

		return pressedButton;
	}
	
	ButtonType InputPopup(const std::string_view title, const std::string_view hint, std::string& input)
	{
		ButtonType buttons = ButtonType::OKCancel;

		ButtonType pressedButton = ButtonType::None;
		ImGui::OpenPopup(title.data());

		// Always center this window when appearing
		ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

		if (ImGui::BeginPopupModal(title.data(), NULL, ImGuiWindowFlags_AlwaysAutoResize))
		{
			char buf[128] = {0};
			memcpy_s(buf, sizeof(buf), input.c_str(), input.size());
			if (ImGui::InputTextWithHint("##MyInputPopup", hint.data(), buf, sizeof(buf)))
				input = buf;

			ImGui::Separator();

			//Manually drawing OK because it needs to be disabled if input is empty.
			if (HasFlags(buttons, ButtonType::OK))
			{
				const bool bDisable = input.size() == 0;
				if (bDisable)
					UI::PushItemDisabled();
				if (ImGui::Button("OK", ImVec2(120, 0)))
				{
					pressedButton = ButtonType::OK;
					ImGui::CloseCurrentPopup();
				}
				if (bDisable)
					UI::PopItemDisabled();
				ImGui::SetItemDefaultFocus();
				ImGui::SameLine();
				buttons = ButtonType(buttons & (~ButtonType::OK)); //Removing OK from mask to draw buttons as usual without OK cuz we already drew it
			}
			if (auto pressed = DrawButtons(buttons); pressed != ButtonType::None)
				pressedButton = pressed;
			
			ImGui::EndPopup();
		}

		return pressedButton;
	}

	void Image(const Ref<Eagle::Image>& image, const ImVec2& size, const ImVec2& uv0, const ImVec2& uv1, const ImVec4& tint_col, const ImVec4& border_col)
	{
		if (!image)
			return;

		if (RendererContext::Current() == RendererAPIType::Vulkan)
		{
			VkSampler vkSampler = (VkSampler)Sampler::PointSampler->GetHandle();
			VkImageView vkImageView = (VkImageView)image->GetImageViewHandle();

			const auto textureID = ImGui_ImplVulkan_AddTexture(vkSampler, vkImageView, s_VulkanImageLayout);
			ImGui::Image(textureID, size, uv0, uv1, tint_col, border_col);
		}
	}

	void Image(const Ref<Texture2D>& texture, const ImVec2& size, const ImVec2& uv0, const ImVec2& uv1, const ImVec4& tint_col, const ImVec4& border_col)
	{
		if (!texture || !texture->IsLoaded())
			return;

		if (RendererContext::Current() == RendererAPIType::Vulkan)
		{
			const Ref<Eagle::Image>& image = texture->GetImage();
			if (!image || image->GetLayout() != ImageReadAccess::PixelShaderRead)
				return;

			VkSampler vkSampler = (VkSampler)texture->GetSampler()->GetHandle();
			VkImageView vkImageView = (VkImageView)image->GetImageViewHandle();

			const auto textureID = ImGui_ImplVulkan_AddTexture(vkSampler, vkImageView, s_VulkanImageLayout);
			ImGui::Image(textureID, size, uv0, uv1, tint_col, border_col);
		}
	}

	void ImageMip(const Ref<Texture2D>& texture, uint32_t mip, const ImVec2& size, const ImVec2& uv0, const ImVec2& uv1, const ImVec4& tint_col, const ImVec4& border_col)
	{
		if (!texture || !texture->IsLoaded())
			return;

		if (RendererContext::Current() == RendererAPIType::Vulkan)
		{
			const Ref<Eagle::Image>& image = texture->GetImage();
			if (!image || image->GetLayout() != ImageReadAccess::PixelShaderRead)
				return;

			ImageView imageView{ mip };
			VkSampler vkSampler = (VkSampler)texture->GetSampler()->GetHandle();
			VkImageView vkImageView = (VkImageView)image->GetImageViewHandle(imageView);

			const auto textureID = ImGui_ImplVulkan_AddTexture(vkSampler, vkImageView, s_VulkanImageLayout);
			ImGui::Image(textureID, size, uv0, uv1, tint_col, border_col);
		}
	}

	void ImageMip(const Ref<Eagle::Image>& image, uint32_t mip, const ImVec2& size, const ImVec2& uv0, const ImVec2& uv1, const ImVec4& tint_col, const ImVec4& border_col)
	{
		if (!image || image->GetLayout() != ImageReadAccess::PixelShaderRead)
			return;

		if (RendererContext::Current() == RendererAPIType::Vulkan)
		{
			ImageView imageView{ mip };

			VkSampler vkSampler = (VkSampler)Sampler::PointSampler->GetHandle();
			VkImageView vkImageView = (VkImageView)image->GetImageViewHandle(imageView);

			const auto textureID = ImGui_ImplVulkan_AddTexture(vkSampler, vkImageView, s_VulkanImageLayout);
			ImGui::Image(textureID, size, uv0, uv1, tint_col, border_col);
		}
	}

	bool ImageButton(const Ref<Eagle::Image>& image, const ImVec2& size, const ImVec2& uv0, const ImVec2& uv1, const ImVec4& bg_col, const ImVec4& tint_col)
	{
		if (!image || image->GetLayout() != ImageReadAccess::PixelShaderRead)
			return false;

		if (RendererContext::Current() == RendererAPIType::Vulkan)
		{
			VkSampler vkSampler = (VkSampler)Sampler::PointSampler->GetHandle();
			VkImageView vkImageView = (VkImageView)image->GetImageViewHandle();

			const auto textureID = ImGui_ImplVulkan_AddTexture(vkSampler, vkImageView, s_VulkanImageLayout);
			ImGuiID id = (ImGuiID)((((uint64_t)vkImageView) >> 32) ^ (uint64_t)vkImageView);

			return ImGui::ImageButtonEx(id, textureID, size, uv0, uv1, bg_col, tint_col);
		}
		return false;
	}

	bool ImageButton(const Ref<Texture2D>& texture, const ImVec2& size, const ImVec2& uv0, const ImVec2& uv1, const ImVec4& bg_col, const ImVec4& tint_col)
	{
		if (!texture || !texture->IsLoaded())
			return false;

		if (RendererContext::Current() == RendererAPIType::Vulkan)
		{
			const Ref<Eagle::Image>& image = texture->GetImage();
			if (!image || image->GetLayout() != ImageReadAccess::PixelShaderRead)
				return false;

			VkSampler vkSampler = (VkSampler)texture->GetSampler()->GetHandle();
			VkImageView vkImageView = (VkImageView)image->GetImageViewHandle();

			const auto textureID = ImGui_ImplVulkan_AddTexture(vkSampler, vkImageView, s_VulkanImageLayout);
			ImGuiID id = (ImGuiID)texture->GetGUID().GetHash();
			return ImGui::ImageButtonEx(id, textureID, size, uv0, uv1, bg_col, tint_col);
		}
		return false;
	}
	
}

namespace Eagle::UI::TextureViewer
{
	void OpenTextureViewer(const Ref<Texture2D>& textureToView, bool* outWindowOpened)
	{
		bool bHidden = !ImGui::Begin("Texture Viewer", outWindowOpened);
		static bool detailsDocked = false;
		static bool bDetailsVisible;
		bDetailsVisible = (!bHidden) || (bHidden && !detailsDocked);
		ImVec2 availSize = ImGui::GetContentRegionAvail();
		glm::vec2 visualizeImageSize = textureToView->GetSize();
		const uint32_t mipsCount = textureToView->GetMipsCount(); // MipsCount == 1 means no mips, just the original
		static int selectedMip = 0;
		selectedMip = glm::clamp(selectedMip, 0, (int)mipsCount - 1);

		const float tRatio = visualizeImageSize[0] / visualizeImageSize[1];
		const float wRatio = availSize[0] / availSize[1];

		visualizeImageSize = wRatio > tRatio ? glm::vec2{ visualizeImageSize[0] * availSize[1] / visualizeImageSize[1], availSize[1] }
		: glm::vec2{ availSize[0], visualizeImageSize[1] * availSize[0] / visualizeImageSize[0] };

		UI::ImageMip(Cast<Texture2D>(textureToView), uint32_t(selectedMip), { visualizeImageSize[0], visualizeImageSize[1] });
		if (bDetailsVisible)
		{
			float anisotropy = textureToView->GetAnisotropy();
			FilterMode filterMode = textureToView->GetFilterMode();
			AddressMode addressMode = textureToView->GetAddressMode();
			const float maxAnisotropy = RenderManager::GetCapabilities().MaxAnisotropy;

			const glm::ivec2 textureSize = glm::ivec2(textureToView->GetSize()) >> selectedMip;
			const std::string textureSizeString = std::to_string(textureSize.x) + "x" + std::to_string(textureSize.y);

			ImGui::Begin("Details");
			detailsDocked = ImGui::IsWindowDocked();
			UI::BeginPropertyGrid("TextureViewDetails");
			UI::Text("Name", textureToView->GetPath().filename().u8string());
			UI::Text("Resolution", textureSizeString);
			if (UI::PropertySlider("Anisotropy", anisotropy, 1.f, maxAnisotropy))
				textureToView->SetAnisotropy(anisotropy);

			// Filter mode
			{
				static std::vector<std::string> modesStrings = { "Nearest", "Bilinear", "Trilinear"};
				int selectedIndex = 0;
				if (UI::Combo("Filtering", (uint32_t)filterMode, modesStrings, selectedIndex))
				{
					filterMode = FilterMode(selectedIndex);
					textureToView->SetFilterMode(filterMode);
				}
			}

			// Address Mode
			{
				static std::vector<std::string> modesStrings = { "Wrap", "Mirror", "Clamp", "Clamp to Black", "Clamp to White" };

				int selectedIndex = 0;
				if (UI::Combo("Wrapping", (uint32_t)addressMode, modesStrings, selectedIndex))
				{
					addressMode = AddressMode(selectedIndex);
					textureToView->SetAddressMode(addressMode);
				}
			}

			// Visualize mips
			{
				static std::vector<std::string> modesStrings;
				if (modesStrings.size() != mipsCount)
				{
					modesStrings.resize(mipsCount);
					for (uint32_t i = 0; i < mipsCount; ++i)
						modesStrings[i] = "Mip #" + std::to_string(i);
				}

				UI::Combo("Visualize", (uint32_t)selectedMip, modesStrings, selectedMip, {}, "Mip #0 is the original texture");
			}

			// Generate Mips
			{
				static const Texture2D* s_LastTexture = nullptr;
				constexpr int minMips = 1;
				const int maxMips = (int)CalculateMipCount(textureToView->GetSize());

				static int generateMipsCount = 1;
				if (s_LastTexture != textureToView.get()) // Texture changed, reset mips slider
				{
					s_LastTexture = textureToView.get();
					generateMipsCount = textureToView->GetMipsCount();
				}
				generateMipsCount = glm::clamp(generateMipsCount, minMips, maxMips);

				UpdateIDBuffer("Generate Mips");
				ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
				ImGui::Text("Generate Mips");
				ImGui::SameLine();
				UI::HelpMarker("Mips count = 1 means no mips, only the original texture");

				ImGui::NextColumn();

				{
					const float labelwidth = ImGui::CalcTextSize("Generate", NULL, true).x;
					const float buttonWidth = labelwidth + GImGui->Style.FramePadding.x * 8.0f;
					const float width = ImGui::GetColumnWidth();
					ImGui::PushItemWidth(width - buttonWidth);
				}
				ImGui::SliderInt(s_IDBuffer, &generateMipsCount, minMips, maxMips);

				ImGui::PopItemWidth();

				ImGui::SameLine();

				if (ImGui::Button("Generate"))
					textureToView->GenerateMips(uint32_t(generateMipsCount));
			}

			UI::EndPropertyGrid();
			ImGui::End();
		}

		ImGui::End();
	}

	void OpenTextureViewer(const Ref<TextureCube>& cubeTexture, bool* outWindowOpened)
	{
		const Ref<Texture2D>& textureToView = cubeTexture->GetTexture2D();

		bool bHidden = !ImGui::Begin("Texture Viewer", outWindowOpened);
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
			UI::BeginPropertyGrid("TextureViewDetails");
			UI::Text("Name", textureToView->GetPath().filename().u8string());
			UI::Text("Resolution", textureSizeString);

			UI::EndPropertyGrid();
			ImGui::End();
		}

		ImGui::End();
	}
}
