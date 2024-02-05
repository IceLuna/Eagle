#include "egpch.h"

#include "UI.h"
#include "Eagle/Classes/Font.h"

#include "Eagle/Audio/SoundGroup.h"
#include "Eagle/Asset/AssetManager.h"
#include "Eagle/Utils/PlatformUtils.h"
#include "Eagle/Components/Components.h"
#include "Eagle/Audio/AudioEngine.h"
#include "Eagle/Renderer/TextureCompressor.h"

#include "Platform/Vulkan/VulkanImage.h"
#include "Platform/Vulkan/VulkanTexture2D.h"
#include "Platform/Vulkan/VulkanUtils.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_vulkan.h>

#include <stb_image.h>

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

	ButtonType UI::DrawButtons(ButtonType buttons)
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

	bool Text(const std::string_view label, const std::string_view text, const std::string_view helpMessage)
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

	bool PropertyDrag(const std::string_view label, glm::ivec3& value, float speed, int min, int max, const std::string_view helpMessage)
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

		bModified = ImGui::DragInt3(s_IDBuffer, &value.x, speed, min, max);
		ImGui::PopItemWidth();
		ImGui::NextColumn();
		return bModified;
	}

	bool PropertyDrag(const std::string_view label, glm::uvec3& value, float speed, int min, int max, const std::string_view helpMessage)
	{
		glm::ivec3 temp = value;
		const bool bChanged = PropertyDrag(label, temp, speed, min, max, helpMessage);
		if (bChanged)
			value = glm::uvec3(glm::max(temp, 0)); // Clamp negatives to 0 so that we don't overflow
		return bChanged;
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

	void PushButtonSelectedStyleColors()
	{
		const float coef = 1.5f;
		const auto& colors = ImGui::GetStyle().Colors;
		ImVec4 button = colors[ImGuiCol_Button];
		button.x *= coef; button.y *= coef; button.z *= coef;

		ImVec4 buttonHovered = colors[ImGuiCol_ButtonHovered];
		buttonHovered.x *= coef; buttonHovered.y *= coef; buttonHovered.z *= coef;

		ImVec4 buttonActive = colors[ImGuiCol_ButtonActive];
		buttonActive.x *= coef; buttonActive.y *= coef; buttonActive.z *= coef;

		ImGui::PushStyleColor(ImGuiCol_Button, button);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, buttonHovered);
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, buttonActive);
	}

	void PopButtonSelectedStyleColors()
	{
		ImGui::PopStyleColor(3);
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

	void AddImage(const Ref<Texture2D>& texture, const ImVec2& min, const ImVec2& max, const ImVec2& uv0, const ImVec2& uv1, uint32_t col)
	{
		if (!texture || !texture->IsLoaded())
			return;

		if (RendererContext::Current() == RendererAPIType::Vulkan)
		{
			const Ref<Eagle::Image>& image = texture->GetImage();
			if (!image || image->GetLayout() != ImageReadAccess::PixelShaderRead)
				return;

			constexpr uint32_t mip = 0;
			ImageView imageView{ mip };
			VkSampler vkSampler = (VkSampler)texture->GetSampler()->GetHandle();
			VkImageView vkImageView = (VkImageView)image->GetImageViewHandle(imageView);

			const auto textureID = ImGui_ImplVulkan_AddTexture(vkSampler, vkImageView, s_VulkanImageLayout);
			ImGui::GetWindowDrawList()->AddImage(textureID, min, max, uv0, uv1, col);
		}
	}

	bool ImageButtonWithText(const Ref<Texture2D>& image, const std::string_view text, ImVec2 size, bool bFillFrameDefault, float textHeightOffset, ImVec2 framePadding)
	{
		ImGuiContext& g = *GImGui;
		const ImVec2 padding = g.Style.FramePadding;
		const ImVec2 textSize = ImGui::CalcTextSize(text.data(), NULL, true);
		const float itemSpacingHeight = g.Style.ItemSpacing.y;

		ImVec2 frameSize = size + framePadding;
		frameSize.y += textSize.y + 2.f * itemSpacingHeight; // 2 item spacings because we add padding at the top and at the bottom

		ImGuiWindow* window = ImGui::GetCurrentWindow();
		const ImRect bb(window->DC.CursorPos, window->DC.CursorPos + frameSize);

		ImVec2 p = ImGui::GetCursorScreenPos();
		const bool bResult = ImGui::InvisibleButton(text.data(), frameSize);

		// Save the data to restore them at the end
		const ImVec2 prevLine = window->DC.CursorPosPrevLine;
		const ImVec2 curLine = window->DC.CursorPos;
		auto buttonItemData = g.LastItemData;

		const bool bHovered = ImGui::IsItemHovered();
		const bool bHeld = bHovered && ImGui::IsMouseDown(ImGuiMouseButton_Left);
		auto& colors = ImGui::GetStyle().Colors;
		if (!bFillFrameDefault)
			ImGui::PushStyleColor(ImGuiCol_Button, colors[ImGuiCol_WindowBg]);

		const ImU32 col = ImGui::GetColorU32((bHeld && bHovered) ? ImGuiCol_ButtonActive : bHovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button);
		ImGui::RenderFrame(bb.Min, bb.Max, col, true, ImClamp((float)ImMin(padding.x, padding.y), 0.0f, g.Style.FrameRounding));
		
		if (!bFillFrameDefault)
			ImGui::PopStyleColor();

		UI::AddImage(image, p, ImVec2(p.x + size.x, p.y + size.y));

		// Centering text
		ImGui::SetCursorScreenPos(ImVec2(glm::max(p.x, p.x + 0.5f * (size.x - textSize.x)), p.y + size.y + itemSpacingHeight + textHeightOffset));

		ImGui::Text(text.data());

		window->DC.CursorPos = curLine;
		window->DC.CursorPosPrevLine = prevLine;
		g.LastItemData = buttonItemData;

		return bResult;
	}

	bool ImageButtonWithTextHorizontal(const Ref<Texture2D>& image, const std::string_view text, ImVec2 size, float frameHeight, bool bFillFrameDefault)
	{
		ImGuiWindow* window = ImGui::GetCurrentWindow();
		const float prevFontScale = window->FontWindowScale;
		const float scale = size.x / 50.f;
		ImGui::SetWindowFontScale(scale);

		ImGuiContext& g = *GImGui;
		const ImVec2 padding = g.Style.FramePadding;
		const ImVec2 textSize = ImGui::CalcTextSize(text.data(), NULL, true);
		const float itemSpacingWidth = g.Style.ItemSpacing.x;
		const float itemSpacingHeight = g.Style.ItemSpacing.y;

		const float frameWidth = glm::max(1.f, ImGui::GetContentRegionAvail().x);
		ImVec2 frameSize = ImVec2(frameWidth, frameHeight);

		const ImRect bb(window->DC.CursorPos, window->DC.CursorPos + frameSize);

		ImVec2 p = ImGui::GetCursorScreenPos();
		const bool bResult = ImGui::InvisibleButton(text.data(), frameSize);

		// Save the data to restore them at the end
		const ImVec2 prevLine = window->DC.CursorPosPrevLine;
		const ImVec2 curLine = window->DC.CursorPos;
		auto buttonItemData = g.LastItemData;

		const bool bHovered = ImGui::IsItemHovered();
		const bool bHeld = bHovered && ImGui::IsMouseDown(ImGuiMouseButton_Left);
		auto& colors = ImGui::GetStyle().Colors;
		if (!bFillFrameDefault)
			ImGui::PushStyleColor(ImGuiCol_Button, colors[ImGuiCol_WindowBg]);

		const ImU32 col = ImGui::GetColorU32((bHeld && bHovered) ? ImGuiCol_ButtonActive : bHovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button);
		ImGui::RenderFrame(bb.Min, bb.Max, col, true, ImClamp((float)ImMin(padding.x, padding.y), 0.0f, g.Style.FrameRounding));

		if (!bFillFrameDefault)
			ImGui::PopStyleColor();

		const ImVec2 imageEnd = ImVec2(p.x + size.x, p.y + size.y);
		UI::AddImage(image, p, imageEnd);

		// Centering text
		ImGui::SetCursorScreenPos(ImVec2(imageEnd.x + itemSpacingWidth, imageEnd.y - 0.5f * (bb.Max.y - bb.Min.y) - textSize.y * 0.5f));

		ImGui::Text(text.data());
		ImGui::SetWindowFontScale(prevFontScale);

		window->DC.CursorPos = curLine;
		window->DC.CursorPosPrevLine = prevLine;
		g.LastItemData = buttonItemData;

		return bResult;
	}
}

namespace Eagle::UI::Editor
{
	void OpenTextureEditor(const Ref<AssetTexture2D>& asset, bool* outWindowOpened)
	{
		const auto& textureToView = asset->GetTexture();

		bool bHidden = !ImGui::Begin("Texture Editor", outWindowOpened);
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

		bool bChanged = false;

		if (bDetailsVisible)
		{
			float anisotropy = textureToView->GetAnisotropy();
			FilterMode filterMode = textureToView->GetFilterMode();
			AddressMode addressMode = textureToView->GetAddressMode();
			const float maxAnisotropy = RenderManager::GetCapabilities().MaxAnisotropy;
			const size_t gpuMemSize = textureToView->GetMemSize();
			bool bCompressed = asset->IsCompressed();
			bool bNormalMap = asset->IsNormalMap();
			bool bNeedAlpha = asset->DoesNeedAlpha();
			auto assetFormat = asset->GetFormat();

			const glm::ivec2 baseTextureSize = textureToView->GetSize();
			const glm::ivec2 mipTextureSize = baseTextureSize >> selectedMip;
			const std::string baseSizeString = std::to_string(baseTextureSize.x) + "x" + std::to_string(baseTextureSize.y);
			const std::string mipSizeString = std::to_string(mipTextureSize.x) + "x" + std::to_string(mipTextureSize.y);

			ImGui::Begin("Details");
			detailsDocked = ImGui::IsWindowDocked();
			UI::BeginPropertyGrid("TextureDetails");
			UI::Text("Name", asset->GetPath().stem().u8string());
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
					asset->SetFormat(assetFormat);
					bChanged = true;
				}
			}

			if (UI::Property("Is Normal Map", bNormalMap, "Currently, it only affects the result if the compression is enabled"))
			{
				asset->SetIsNormalMap(bNormalMap);
				bChanged = true;
			}
			
			if (UI::Property("Imported alpha", bNeedAlpha, "Currently, it only affects the result if the compression is enabled"))
			{
				asset->SetNeedsAlpha(bNeedAlpha);
				bChanged = true;
			}

			if (UI::Property("Compressed", bCompressed, "If set to true, the engine will try to compress the image"))
			{
				asset->SetIsCompressed(bCompressed, textureToView->GetMipsCount());
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
				static std::vector<std::string> modesStrings = { "Nearest", "Bilinear", "Trilinear"};
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
				static std::vector<std::string> modesStrings = { "Wrap", "Mirror", "Clamp", "Clamp to Black", "Clamp to White" };

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
				{
					asset->SetIsCompressed(asset->IsCompressed(), uint32_t(generateMipsCount));
					bChanged = true;
				}
			}

			UI::EndPropertyGrid();

			if (bChanged)
				asset->SetDirty(true);

			ImGui::Separator();
			ImGui::Separator();

			if (ImGui::Button("Save asset"))
				Asset::Save(asset);

			ImGui::End();
		}

		ImGui::End();
	}

	void OpenTextureEditor(const Ref<AssetTextureCube>& asset, bool* outWindowOpened)
	{
		const Ref<TextureCube>& textureCube = asset->GetTexture();
		const Ref<Texture2D>& textureToView = textureCube->GetTexture2D();

		bool bHidden = !ImGui::Begin("Texture Editor", outWindowOpened);
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
			UI::Text("Name", asset->GetPath().stem().u8string());
			UI::Text("Resolution", textureSizeString);
			UI::Text("Format", Utils::GetEnumName(asset->GetFormat()));

			{
				static const Texture2D* s_LastTexture = nullptr;
				static int layerSize = 1u;
				if (s_LastTexture != textureToView.get()) // Texture changed, reset mips slider
				{
					s_LastTexture = textureToView.get();
					layerSize = (int)textureCube->GetSize().x;
				}

				UpdateIDBuffer("Generate Layer size");
				ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
				ImGui::Text("Layer Size");
				ImGui::SameLine();
				UI::HelpMarker("Resolution of a cube side");

				ImGui::NextColumn();

				{
					const float labelwidth = ImGui::CalcTextSize("Generate", NULL, true).x;
					const float buttonWidth = labelwidth + GImGui->Style.FramePadding.x * 8.0f;
					const float width = ImGui::GetColumnWidth();
					ImGui::PushItemWidth(width - buttonWidth);
				}

				if (ImGui::DragInt(s_IDBuffer, &layerSize, 16.f, 32, 4096))
					layerSize = glm::clamp(layerSize, 16, 4096);

				ImGui::PopItemWidth();

				ImGui::SameLine();

				if (ImGui::Button("Generate"))
				{
					asset->SetLayerSize(uint32_t(layerSize));
					asset->SetDirty(true);
				}
			}

			UI::EndPropertyGrid();

			ImGui::Separator();
			ImGui::Separator();

			if (ImGui::Button("Save asset"))
				Asset::Save(asset);

			ImGui::End();
		}

		ImGui::End();
	}
	
	void OpenMaterialEditor(const Ref<AssetMaterial>& asset, bool* outWindowOpened)
	{
		static const char* s_MetalnessHelpMsg = "Controls how 'metal-like' surface looks like.\nDefault is 0";
		static const char* s_RoughnessHelpMsg = "Controls how rough surface looks like.\nRoughness of 0 is a mirror reflection and 1 is completely matte.\nDefault is 0.5";
		static const char* s_AOHelpMsg = "Can be used to affect how ambient lighting is applied to an object. If it's 0, ambient lighting won't affect it. Default is 1.0";
		static const char* s_BlendModeHelpMsg = "Translucent materials do not cast shadows!\nUse translucent materials with caution cause rendering them can be expensive";
		static const char* s_OpacityHelpMsg = "Controls the translucency of the material. 0 - fully transparent, 1 - fully opaque. Default is 0.5";
		static const char* s_OpacityMaskHelpMsg = "When in Masked mode, a material is either completely visible or completely invisible.\nValues below 0.5 are invisible";

		const auto& material = asset->GetMaterial();
		bool bChanged = false;

		bool bHidden = !ImGui::Begin("Material Editor", outWindowOpened);
		UI::BeginPropertyGrid("MaterialDetails");

		UI::Text("Name", asset->GetPath().stem().u8string());

		Material::BlendMode blendMode = material->GetBlendMode();
		if (UI::ComboEnum("Blend Mode", blendMode, s_BlendModeHelpMsg))
		{
			material->SetBlendMode(blendMode);
			bChanged = true;
		}

		Ref<AssetTexture2D> temp = material->GetAlbedoAsset();
		if (UI::DrawAssetSelection("Albedo", temp))
		{
			material->SetAlbedoAsset(temp);
			bChanged = true;
		}

		temp = material->GetMetallnessAsset();
		if (UI::DrawAssetSelection("Metalness", temp, s_MetalnessHelpMsg))
		{
			material->SetMetallnessAsset(temp);
			bChanged = true;
		}

		temp = material->GetNormalAsset();
		if (UI::DrawAssetSelection("Normal", temp))
		{
			material->SetNormalAsset(temp);
			bChanged = true;
		}

		temp = material->GetRoughnessAsset();
		if (UI::DrawAssetSelection("Roughness", temp, s_RoughnessHelpMsg))
		{
			material->SetRoughnessAsset(temp);
			bChanged = true;
		}

		temp = material->GetAOAsset();
		if (UI::DrawAssetSelection("Ambient Occlusion", temp, s_AOHelpMsg))
		{
			material->SetAOAsset(temp);
			bChanged = true;
		}

		temp = material->GetEmissiveAsset();
		if (UI::DrawAssetSelection("Emissive Color", temp))
		{
			material->SetEmissiveAsset(temp);
			bChanged = true;
		}

		// Disable if not translucent
		{
			const bool bTranslucent = blendMode == Material::BlendMode::Translucent;
			if (!bTranslucent)
				UI::PushItemDisabled();

			temp = material->GetOpacityAsset();
			if (UI::DrawAssetSelection("Opacity", temp, s_OpacityHelpMsg))
			{
				material->SetOpacityAsset(temp);
				bChanged = true;
			}

			if (!bTranslucent)
				UI::PopItemDisabled();
		}

		// Disable if not masked
		{
			const bool bMasked = blendMode == Material::BlendMode::Masked;
			if (!bMasked)
				UI::PushItemDisabled();

			temp = material->GetOpacityMaskAsset();
			if (UI::DrawAssetSelection("Opacity Mask", temp, s_OpacityMaskHelpMsg))
			{
				material->SetOpacityMaskAsset(temp);
				bChanged = true;
			}

			if (!bMasked)
				UI::PopItemDisabled();
		}

		glm::vec3 emissiveIntensity = material->GetEmissiveIntensity();
		if (UI::PropertyColor("Emissive Intensity", emissiveIntensity, true, "HDR"))
		{
			material->SetEmissiveIntensity(emissiveIntensity);
			bChanged = true;
		}

		glm::vec4 tintColor = material->GetTintColor();
		if (UI::PropertyColor("Tint Color", tintColor, true))
		{
			material->SetTintColor(tintColor);
			bChanged = true;
		}

		float tiling = material->GetTilingFactor();
		if (UI::PropertySlider("Tiling Factor", tiling, 1.f, 128.f))
		{
			material->SetTilingFactor(tiling);
			bChanged = true;
		}

		UI::EndPropertyGrid();

		if (bChanged)
			asset->SetDirty(true);

		ImGui::Separator();
		ImGui::Separator();
		if (ImGui::Button("Save asset"))
			Asset::Save(asset);

		ImGui::End();
	}

	void OpenPhysicsMaterialEditor(const Ref<AssetPhysicsMaterial>& asset, bool* outWindowOpened)
	{
		static const char* s_StaticFrictionHelpMsg = "Static friction defines the amount of friction that is applied between surfaces that are not moving lateral to each-other";
		static const char* s_DynamicFrictionHelpMsg = "Dynamic friction defines the amount of friction applied between surfaces that are moving relative to each-other";

		const auto& material = asset->GetMaterial();

		bool bHidden = !ImGui::Begin("Physics Material Editor", outWindowOpened);
		UI::BeginPropertyGrid("PhysicsMaterialDetails");

		UI::Text("Name", asset->GetPath().stem().u8string());

		bool bPhysicsMaterialChanged = false;
		bPhysicsMaterialChanged |= UI::PropertyDrag("Static Friction", material->StaticFriction, 0.1f, 0.f, 0.f, s_StaticFrictionHelpMsg);
		bPhysicsMaterialChanged |= UI::PropertyDrag("Dynamic Friction", material->DynamicFriction, 0.1f, 0.f, 0.f, s_DynamicFrictionHelpMsg);
		bPhysicsMaterialChanged |= UI::PropertyDrag("Bounciness", material->Bounciness, 0.1f);

		UI::EndPropertyGrid();

		if (bPhysicsMaterialChanged)
			asset->SetDirty(true);

		ImGui::Separator();
		ImGui::Separator();
		if (ImGui::Button("Save asset"))
			Asset::Save(asset);

		ImGui::End();
	}

	void OpenAudioEditor(const Ref<AssetAudio>& asset, bool* outWindowOpened)
	{
		const auto& audio = asset->GetAudio();
		bool bChanged = false;

		bool bHidden = !ImGui::Begin("Audio Editor", outWindowOpened);
		UI::BeginPropertyGrid("AudioDetails");

		UI::Text("Name", asset->GetPath().stem().u8string());

		float volume = audio->GetVolume();
		if (UI::PropertyDrag("Volume", volume, 0.05f))
		{
			audio->SetVolume(glm::max(volume, 0.f));
			bChanged = true;
		}

		auto soundGroupAsset = asset->GetSoundGroupAsset();
		if (UI::DrawAssetSelection("Sound Group", soundGroupAsset))
		{
			asset->SetSoundGroupAsset(soundGroupAsset);
			bChanged = true;
		}

		UI::EndPropertyGrid();

		ImGui::Separator();
		ImGui::Separator();

		if (ImGui::Button("Play"))
			audio->Play();

		ImGui::SameLine();

		if (bChanged)
			asset->SetDirty(true);

		if (ImGui::Button("Save asset"))
			Asset::Save(asset);

		ImGui::End();
	}
	
	void OpenSoundGroupEditor(const Ref<AssetSoundGroup>& asset, bool* outWindowOpened)
	{
		const auto& soundGroup = asset->GetSoundGroup();
		bool bChanged = false;

		bool bHidden = !ImGui::Begin("Sound Group Editor", outWindowOpened);
		UI::BeginPropertyGrid("SoundGroupDetails");

		UI::Text("Name", asset->GetPath().stem().u8string());

		float volume = soundGroup->GetVolume();
		if (UI::PropertyDrag("Volume", volume, 0.05f))
		{
			soundGroup->SetVolume(glm::max(volume, 0.f));
			bChanged = true;
		}

		float pitch = soundGroup->GetPitch();
		if (UI::PropertyDrag("Pitch", pitch, 0.05f, 0.f, 10.f))
		{
			soundGroup->SetPitch(glm::clamp(pitch, 0.f, 10.f));
			bChanged = true;
		}

		bool bPaused = soundGroup->IsPaused();
		if (UI::Property("Is Paused", bPaused))
		{
			soundGroup->SetPaused(bPaused);
			bChanged = true;
		}

		bool bMuted = soundGroup->IsMuted();
		if (UI::Property("Is Muted", bMuted))
		{
			soundGroup->SetMuted(bMuted);
			bChanged = true;
		}

		UI::EndPropertyGrid();

		if (bChanged)
			asset->SetDirty(true);

		ImGui::Separator();
		ImGui::Separator();
		if (ImGui::Button("Save asset"))
			Asset::Save(asset);

		ImGui::End();
	}
}
