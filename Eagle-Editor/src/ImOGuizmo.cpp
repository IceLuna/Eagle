#include "ImOGuizmo.h"

#include <imoguizmo/imoguizmo.hpp>

namespace Eagle::ImOGuizmo
{
	bool Render(float x, float y, float size, float* viewMatrix, float* projMatrix, float pivotDistance)
	{
		::ImOGuizmo::SetRect(x, y, size);
		return ::ImOGuizmo::DrawGizmo(viewMatrix, projMatrix, pivotDistance);
	}
}
