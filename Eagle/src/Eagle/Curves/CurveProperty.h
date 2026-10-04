#pragma once

#include "Curve.h"

namespace Eagle
{
	enum class CurvePropertyMode
	{
		Constant, // A single value, no keys needed
		Curve     // Keys over normalized time [0; 1]
	};

	template <typename T>
	struct CurveProperty
	{
		CurvePropertyMode Mode = CurvePropertyMode::Constant;
		T Constant{};
		Curve<T> Keys;

		CurveProperty() = default;
		CurveProperty(const T& constant) : Constant(constant) {}

		bool IsConstant() const { return Mode == CurvePropertyMode::Constant || Keys.IsEmpty(); }

		// `time` is normalized: 0 = start; 1 = end
		T Evaluate(float time) const
		{
			return IsConstant() ? Constant : Keys.Evaluate(time, Constant);
		}

		void SetMode(CurvePropertyMode mode)
		{
			// Switching to `Curve` for the first time makes a flat curve at the constant value
			if (mode == CurvePropertyMode::Curve && Keys.IsEmpty())
			{
				Keys.AddKey(0.f, Constant, CurveInterpolation::Linear);
				Keys.AddKey(1.f, Constant, CurveInterpolation::Linear);
			}
			Mode = mode;
		}

		// A constant if both values are equal, otherwise a linear curve from `start` to `end`.
		static CurveProperty FromStartEnd(const T& start, const T& end)
		{
			CurveProperty result(start);
			if (start != end)
			{
				result.Mode = CurvePropertyMode::Curve;
				result.Keys.AddKey(0.f, start, CurveInterpolation::Linear);
				result.Keys.AddKey(1.f, end, CurveInterpolation::Linear);
			}
			return result;
		}
	};
}
