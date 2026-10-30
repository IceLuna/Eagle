#pragma once

#include <imgui.h>
#include <glm/glm.hpp>
#include <functional>

namespace Eagle
{
	class EditorCamera;

	// Snapping of the transform gizmo. It's shared by all viewports
	struct SnapSettings
	{
		glm::vec3 Values = glm::vec3(0.1f, 10.f, 0.1f); // Translation (units), rotation (degrees), scale
		bool bEnabled[3] = { false, false, false };     // Translation, rotation, scale

		// @guizmoType. ImGuizmo::OPERATION. Holding Shift inverts the state of the toolbar's snapping toggle
		bool IsSnapping(int guizmoType) const;
		float GetValue(int guizmoType) const;

		// Index into `Values` and `bEnabled`. -1 if the operation doesn't snap
		static int GetIndex(int guizmoType);
	};

	// Buttons drawn over the top of a viewport.
	// Every part is optional, it's drawn only if its value is set
	struct ViewportToolbar
	{
		int* GuizmoType = nullptr;      // ImGuizmo::OPERATION: TRANSLATE, ROTATE, SCALE, or -1 (no gizmo)
		int* GuizmoMode = nullptr;      // ImGuizmo::MODE: WORLD or LOCAL
		SnapSettings* Snap = nullptr;
		EditorCamera* Camera = nullptr; // For its speed

		// Items of the "Show" dropdown, for example `ImGui::MenuItem()` with a checkmark
		std::function<void()> ShowMenu;

		// The name of the current view mode, and the items of its dropdown
		const char* ViewModeName = nullptr;
		std::function<void()> ViewModeMenu;

		// Draws the toolbar within the current window (the viewport's window).
		// Returns true if the mouse is over the toolbar, or one of its popups is open
		bool Draw(const char* id, const ImVec2& viewportMin, const ImVec2& viewportMax) const;
	};
}
