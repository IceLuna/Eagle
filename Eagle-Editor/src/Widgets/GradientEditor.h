#pragma once

#include "Eagle/Curves/Curve.h"

#include <imgui.h>

namespace Eagle
{
	// Edits a `Curve<glm::vec4>` as an RGBA color gradient over [0; 1]: a bar with draggable color stops.
	// Click the bar to add a stop, drag a stop to move it, right-click a stop to delete it (or select it and press Delete)
	class GradientEditor
	{
	public:
		// Draws the bar and the selected stop's settings. Returns true if the gradient changed
		bool Draw(const char* id, Curve<glm::vec4>& gradient);

		// Draws the gradient into a rectangle, over a checkerboard so that transparency is visible.
		// Also used for small previews
		static void DrawGradient(ImDrawList* drawList, const Curve<glm::vec4>& gradient, const ImVec2& min, const ImVec2& max, const glm::vec4& defaultColor = glm::vec4(1.f));

	private:
		GUID m_SelectedKey = GUID(0, 0);
		GUID m_DraggedKey = GUID(0, 0);
		GUID m_ContextKey = GUID(0, 0);
	};
}
