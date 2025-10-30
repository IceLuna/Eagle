#pragma once

#include "Eagle/Core/Core.h"

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
		glm::dvec2 Coord = glm::dvec2(0.0);
		float AnimSpeed = 1.f;
		uint32_t Index = 0; // For internal use. Vertex index into `std::vector<BlendSpaceVertex>` inside `AssetAnimationBlendSpace`
	};

	struct BlendSpaceAxisSettings
	{
		std::string Name = "Axis";
		double Min = 0.f;
		double Max = 1.f;
	};
}
