#pragma once

#include "Eagle/Core/EnumUtils.h"
#include "Eagle/Renderer/VidWrappers/Texture.h"
#include "imgui.h"

namespace Eagle
{
	class StaticMesh;
	class Font;
}

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

	bool DrawTexture2DSelection(const std::string_view label, Ref<Texture2D>& modifyingTexture, const std::string_view helpMessage = "");
	bool DrawTextureCubeSelection(const std::string_view label, Ref<TextureCube>& modifyingTexture);
	bool DrawStaticMeshSelection(const std::string_view label, Ref<StaticMesh>& staticMesh, const std::string_view helpMessage = "");
	bool DrawFontSelection(const std::string_view label, Ref<Font>& modifyingFont, const std::string_view helpMessage = "");
	bool DrawSoundSelection(const std::string_view label, Path& selectedSoundPath);
	bool DrawVec3Control(const std::string_view label, glm::vec3& values, const glm::vec3 resetValues = glm::vec3{ 0.f }, float columnWidth = 100.f);

	//Grid Name needs to be unique
	void BeginPropertyGrid(const std::string_view gridName);
	void EndPropertyGrid();

	bool Property(const std::string_view label, bool& value, const std::string_view helpMessage = "");
	bool Property(const std::string_view label, const std::vector<std::string>& customLabels, bool* values, const std::string_view helpMessage = "");
	bool PropertyText(const std::string_view label, std::string& value, const std::string_view helpMessage = "");
	bool PropertyTextMultiline(const std::string_view label, std::string& value, const std::string_view helpMessage = "");

	bool Text(const std::string_view label, const std::string_view text);

	bool PropertyDrag(const std::string_view label, int& value, float speed = 1.f, int min = 0, int max = 0, const std::string_view helpMessage = "");
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
	bool Combo(const std::string_view label, uint32_t currentSelection, const std::vector<std::string>& options, int& outSelectedIndex, const std::vector<std::string>& tooltips = {}, const std::string_view helpMessage = "");
	bool ComboWithNone(const std::string_view label, int currentSelection, const std::vector<std::string>& options, int& outSelectedIndex, const std::vector<std::string>& tooltips = {}, const std::string_view helpMessage = "");
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
	bool ImageButton(const Ref<Eagle::Image>& image, const ImVec2& size, const ImVec2& uv0 = ImVec2(0, 0), const ImVec2& uv1 = ImVec2(1, 1), const ImVec4& bg_col = ImVec4(0, 0, 0, 0), const ImVec4& tint_col = ImVec4(1, 1, 1, 1));
	bool ImageButton(const Ref<Texture2D>& texture, const ImVec2& size, const ImVec2& uv0 = ImVec2(0, 0), const ImVec2& uv1 = ImVec2(1, 1), const ImVec4& bg_col = ImVec4(0, 0, 0, 0), const ImVec4& tint_col = ImVec4(1, 1, 1, 1));

	int TextResizeCallback(ImGuiInputTextCallbackData* data);
}

namespace Eagle::UI::TextureViewer
{
	// outWindowOpened - In case X button is clicked, this flag will be set to false.
	// outWindowOpened - if nullptr set, windows will not have X button 
	void OpenTextureViewer(const Ref<Texture2D>& textureToView, bool* outWindowOpened = nullptr);
}
