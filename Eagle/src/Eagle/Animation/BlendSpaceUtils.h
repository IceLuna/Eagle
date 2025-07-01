#pragma once

#include "Eagle/Core/Core.h"
#include "Eagle/Utils/DelaunayTriangulation.h"

#include <string>

namespace Eagle
{
	class AssetAnimation;

	enum class BlendSpaceEventsTriggerMode
	{
		None, // No events will be triggered
		HighestWeightedAnimation, // Events will be triggered only from the highest weighted anim
		AllAnimations
	};

	struct BlendSpaceVertex
	{
		Ref<AssetAnimation> Animation;
		Delaunay::Vertex Vertex;
		float AnimSpeed = 1.f;
	};

	struct BlendSpaceAxisSettings
	{
		std::string Name = "Axis";
		double Min = 0.f;
		double Max = 1.f;
	};
}
