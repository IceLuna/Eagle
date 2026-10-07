#pragma once

#include "Eagle/Curves/CurveProperty.h"

#include <string_view>

namespace Eagle
{
	struct CurvePropertyParams
	{
		float Speed = 0.05f;
		float Min = 0.f; // Min == Max - unlimited
		float Max = 0.f;
		std::string_view HelpMessage;
	};

	namespace CurveUI
	{
		// Property grid rows for a `CurveProperty`: label | Constant/Curve | value or a preview of the curve.
		// Return true if the property changed.
		// `bOutEditClicked` is set when the curve preview is clicked, so the caller can open the curve in its curve editor.
		// `bActive` highlights the preview of the curve that is currently being edited
		bool PropertyCurve(std::string_view label, CurveProperty<float>& property, const CurvePropertyParams& params, bool bActive, bool& bOutEditClicked);
		bool PropertyCurve(std::string_view label, CurveProperty<glm::vec2>& property, const CurvePropertyParams& params, bool bActive, bool& bOutEditClicked);
		bool PropertyCurve(std::string_view label, CurveProperty<glm::vec3>& property, const CurvePropertyParams& params, bool bActive, bool& bOutEditClicked);
		bool PropertyCurve(std::string_view label, CurveProperty<bool>& property, const CurvePropertyParams& params, bool bActive, bool& bOutEditClicked);

		// RGBA color, edited as a gradient when it's a curve
		bool PropertyGradient(std::string_view label, CurveProperty<glm::vec4>& property, std::string_view helpMessage, bool bActive, bool& bOutEditClicked);
	}
}
