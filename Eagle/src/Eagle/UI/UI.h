#pragma once

#include "Eagle/Core/EnumUtils.h"
#include "Eagle/Renderer/VidWrappers/Texture.h"
#include "imgui.h"
#include "magic_enum.hpp"

namespace Eagle
{
	class StaticMesh;
	class Font;
	class AssetTexture2D;
	class AssetTextureCube;
	class AssetMaterial;
	class AssetPhysicsMaterial;
	class AssetMesh;
	class AssetAudio;
	class AssetFont;
}

class ScriptEnumFields;

namespace Eagle::UI
{
	enum class ButtonType
	{
		None = 0,
		OK = 0b00000001,
		Cancel = 0b00000010,
		OKCancel = 0b00000011,
		Yes = 0b00000100,
		No = 0b00001000,
		YesNo = 0b00001100,
		YesNoCancel = 0b00001110
	};
	DECLARE_FLAGS(ButtonType);

	bool DrawTexture2DSelection(const std::string_view label, Ref<AssetTexture2D>& modifyingAsset, const std::string_view helpMessage = "");
	bool DrawTextureCubeSelection(const std::string_view label, Ref<AssetTextureCube>& modifyingAsset);
	bool DrawMeshSelection(const std::string_view label, Ref<AssetMesh>& modifyingAsset, const std::string_view helpMessage = "");
	bool DrawFontSelection(const std::string_view label, Ref<AssetFont>& modifyingAsset, const std::string_view helpMessage = "");
	bool DrawAudioSelection(const std::string_view label, Ref<AssetAudio>& modifyingAsset);
	bool DrawMaterialSelection(const std::string_view label, Ref<AssetMaterial>& modifyingAsset, const std::string_view helpMessage = "");
	bool DrawPhysicsMaterialSelection(const std::string_view label, Ref<AssetPhysicsMaterial>& modifyingAsset, const std::string_view helpMessage = "");
	bool DrawVec3Control(const std::string_view label, glm::vec3& values, const glm::vec3 resetValues = glm::vec3{ 0.f }, float columnWidth = 100.f);

	//Grid Name needs to be unique
	void BeginPropertyGrid(const std::string_view gridName);
	void EndPropertyGrid();

	bool Property(const std::string_view label, bool& value, const std::string_view helpMessage = "");
	bool Property(const std::string_view label, const std::vector<std::string>& customLabels, bool* values, const std::string_view helpMessage = "");
	bool PropertyText(const std::string_view label, std::string& value, const std::string_view helpMessage = "");
	bool PropertyTextMultiline(const std::string_view label, std::string& value, const std::string_view helpMessage = "");

	bool Text(const std::string_view label, const std::string_view text);
	bool TextLink(const std::string_view text, const std::string_view url);
	bool BulletLink(const std::string_view text, const std::string_view url);

	bool PropertyDrag(const std::string_view label, int& value, float speed = 1.f, int min = 0, int max = 0, const std::string_view helpMessage = "");
	bool PropertyDrag(const std::string_view label, uint32_t& value, float speed = 1.f, int min = 0, int max = 0, const std::string_view helpMessage = "");
	bool PropertyDrag(const std::string_view label, float& value, float speed = 1.f, float min = 0.f, float max = 0.f, const std::string_view helpMessage = "");
	bool PropertyDrag(const std::string_view label, glm::vec2& value, float speed = 1.f, float min = 0.f, float max = 0.f, const std::string_view helpMessage = "");
	bool PropertyDrag(const std::string_view label, glm::vec3& value, float speed = 1.f, float min = 0.f, float max = 0.f, const std::string_view helpMessage = "");
	bool PropertyDrag(const std::string_view label, glm::vec4& value, float speed = 1.f, float min = 0.f, float max = 0.f, const std::string_view helpMessage = "");

	bool PropertySlider(const std::string_view label, int& value, int min, int max, const std::string_view helpMessage = "");
	bool PropertySlider(const std::string_view label, float& value, float min, float max, const std::string_view helpMessage = "");
	bool PropertySlider(const std::string_view label, glm::vec2& value, float min, float max, const std::string_view helpMessage = "");
	bool PropertySlider(const std::string_view label, glm::vec3& value, float min, float max, const std::string_view helpMessage = "");
	bool PropertySlider(const std::string_view label, glm::vec4& value, float min, float max, const std::string_view helpMessage = "");

	bool PropertyColor(const std::string_view label, glm::vec3& value, bool bHDR = false, const std::string_view helpMessage = "");
	bool PropertyColor(const std::string_view label, glm::vec4& value, bool bHDR = false, const std::string_view helpMessage = "");

	bool InputFloat(const std::string_view label, float& value, float step = 0.f, float stepFast = 0.f, const std::string_view helpMessage = "");
	
	//Returns true if selection changed.
	//outSelectedIndex - index of the selected option
	// ComboWithNone adds a 'None' option as first. outSelectedIndex is -1 if None is selected
	// OptionsSize can be smaller to cutoff some options
	bool Combo(const std::string_view label, uint32_t currentSelection, const std::vector<std::string>& options, int& outSelectedIndex, const std::vector<std::string>& tooltips = {}, const std::string_view helpMessage = "");
	bool Combo(const std::string_view label, uint32_t currentSelection, const std::vector<std::string>& options, size_t optionsSize, int& outSelectedIndex, const std::vector<std::string>& tooltips = {}, const std::string_view helpMessage = "");
	bool ComboWithNone(const std::string_view label, int currentSelection, const std::vector<std::string>& options, int& outSelectedIndex, const std::vector<std::string>& tooltips = {}, const std::string_view helpMessage = "");
	bool Combo(const std::string_view label, int currentValue, const ScriptEnumFields& fields, int& outSelectedValue);

	template <typename Enum>
	bool ComboEnum(const std::string_view label, Enum& current, const std::string_view helpMessage = "")
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

		const auto currentName = magic_enum::enum_name(current);
		if (ImGui::BeginCombo(GetIDBuffer(), currentName.data()))
		{
			constexpr auto entries = magic_enum::enum_entries<Enum>();
			for (size_t i = 0; i < entries.size(); ++i)
			{
				const auto& entry = entries[i];
				bool isSelected = (current == entry.first);

				if (ImGui::Selectable(entry.second.data(), isSelected))
				{
					bModified = true;
					current = entry.first;
				}

				if (isSelected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		ImGui::PopItemWidth();
		ImGui::NextColumn();
		return bModified;
	}

	bool Button(const std::string_view label, const std::string_view buttonText, const ImVec2& size = ImVec2(0, 0));

	void Tooltip(const std::string_view tooltip, float treshHold = EG_HOVER_THRESHOLD);

	void PushItemDisabled();
	void PopItemDisabled();

	void PushFrameBGColor(const glm::vec4& color);
	void PopFrameBGColor();

	void HelpMarker(const std::string_view text);

	ButtonType ShowMessage(const std::string_view title, const std::string_view message, ButtonType buttons);
	ButtonType InputPopup(const std::string_view title, const std::string_view hint, std::string& input);

	void Image(const Ref<Eagle::Image>& image, const ImVec2& size, const ImVec2& uv0 = ImVec2(0, 0), const ImVec2& uv1 = ImVec2(1, 1), const ImVec4& tint_col = ImVec4(1, 1, 1, 1), const ImVec4& border_col = ImVec4(0, 0, 0, 0));
	void ImageMip(const Ref<Eagle::Image>& image, uint32_t mip, const ImVec2& size, const ImVec2& uv0 = ImVec2(0, 0), const ImVec2& uv1 = ImVec2(1, 1), const ImVec4& tint_col = ImVec4(1, 1, 1, 1), const ImVec4& border_col = ImVec4(0, 0, 0, 0));
	void Image(const Ref<Texture2D>& texture, const ImVec2& size, const ImVec2& uv0 = ImVec2(0, 0), const ImVec2& uv1 = ImVec2(1, 1), const ImVec4& tint_col = ImVec4(1, 1, 1, 1), const ImVec4& border_col = ImVec4(0, 0, 0, 0));
	void ImageMip(const Ref<Texture2D>& texture, uint32_t mip, const ImVec2& size, const ImVec2& uv0 = ImVec2(0, 0), const ImVec2& uv1 = ImVec2(1, 1), const ImVec4& tint_col = ImVec4(1, 1, 1, 1), const ImVec4& border_col = ImVec4(0, 0, 0, 0));
	bool ImageButton(const Ref<Eagle::Image>& image, const ImVec2& size, const ImVec2& uv0 = ImVec2(0, 0), const ImVec2& uv1 = ImVec2(1, 1), const ImVec4& bg_col = ImVec4(0, 0, 0, 0), const ImVec4& tint_col = ImVec4(1, 1, 1, 1));
	bool ImageButton(const Ref<Texture2D>& texture, const ImVec2& size, const ImVec2& uv0 = ImVec2(0, 0), const ImVec2& uv1 = ImVec2(1, 1), const ImVec4& bg_col = ImVec4(0, 0, 0, 0), const ImVec4& tint_col = ImVec4(1, 1, 1, 1));

	int TextResizeCallback(ImGuiInputTextCallbackData* data);

	// Internal usage only
	void UpdateIDBuffer(const std::string_view label);
	const char* GetIDBuffer();
}

namespace Eagle::UI::Editor
{
	// outWindowOpened - In case X button is clicked, this flag will be set to false.
	// outWindowOpened - if nullptr set, windows will not have X button 
	void OpenTextureEditor(const Ref<AssetTexture2D>& asset, bool* outWindowOpened = nullptr);
	void OpenTextureEditor(const Ref<AssetTextureCube>& asset, bool* outWindowOpened = nullptr);
	void OpenMaterialEditor(const Ref<AssetMaterial>& asset, bool* outWindowOpened = nullptr);
	void OpenPhysicsMaterialEditor(const Ref<AssetPhysicsMaterial>& asset, bool* outWindowOpened = nullptr);
}
