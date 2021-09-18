#pragma once

#include "Eagle/Renderer/Texture.h"
#include "imgui.h"

namespace Eagle
{
	class StaticMesh;
}

namespace Eagle::UI
{
	enum ButtonType : int
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


	bool DrawTextureSelection(const std::string& label, Ref<Texture>& modifyingTexture, bool bLoadAsSRGB);
	bool DrawStaticMeshSelection(const std::string& label, Ref<StaticMesh>& staticMesh, const std::string& helpMessage = "");
	bool DrawVec3Control(const std::string& label, glm::vec3& values, const glm::vec3 resetValues = glm::vec3{ 0.f }, float columnWidth = 100.f);

	//Grid Name needs to be unique
	void BeginPropertyGrid(const std::string& gridName);
	void EndPropertyGrid();

	bool Property(const std::string& label, std::string& value, const std::string& helpMessage = "");
	bool Property(const std::string& label, bool& value, const std::string& helpMessage = "");
	bool Property(const std::string& label, const std::vector<std::string>& customLabels, bool* values, const std::string& helpMessage = "");

	bool PropertyText(const std::string& label, const std::string& text);

	bool PropertyDrag(const std::string& label, int& value, float speed = 1.f, int min = 0, int max = 0);
	bool PropertyDrag(const std::string& label, float& value, float speed = 1.f, float min = 0.f, float max = 0.f);
	bool PropertyDrag(const std::string& label, glm::vec2& value, float speed = 1.f, float min = 0.f, float max = 0.f);
	bool PropertyDrag(const std::string& label, glm::vec3& value, float speed = 1.f, float min = 0.f, float max = 0.f);
	bool PropertyDrag(const std::string& label, glm::vec4& value, float speed = 1.f, float min = 0.f, float max = 0.f);

	bool PropertySlider(const std::string& label, int& value, int min, int max);
	bool PropertySlider(const std::string& label, float& value, float min, float max);
	bool PropertySlider(const std::string& label, glm::vec2& value, float min, float max);
	bool PropertySlider(const std::string& label, glm::vec3& value, float min, float max);
	bool PropertySlider(const std::string& label, glm::vec4& value, float min, float max);

	bool PropertyColor(const std::string& label, glm::vec3& value);
	bool PropertyColor(const std::string& label, glm::vec4& value);

	bool InputFloat(const std::string& label, float& value, float step = 0.f, float stepFast = 0.f);
	
	//Returns true if selection changed.
	//outSelectedIndex - index of the selected option
	bool Combo(const std::string& label, const std::string& currentSelection, const std::vector<std::string>& options, int& outSelectedIndex, const std::string& helpMessage = "");
	bool Button(const std::string& label, const std::string& buttonText, const ImVec2& size = ImVec2(0, 0));

	void PushItemDisabled();
	void PopItemDisabled();

	void HelpMarker(const std::string& text);

	ButtonType ShowMessage(const std::string& title, const std::string& message, ButtonType buttons);
	ButtonType InputPopup(const std::string& title, const std::string& hint, std::string& input);
}

namespace Eagle::UI::TextureViewer
{
	// outWindowOpened - In case X button will be clicked, this flag will be set to false.
	// outWindowOpened - if nullptr set, windows will not have X button 
	void OpenTextureViewer(const Ref<Texture>& textureToView, bool* outWindowOpened = nullptr);
}
