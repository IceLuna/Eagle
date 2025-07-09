#pragma once

#include "Eagle/Math/AABB.h"
#include <glm/glm.hpp>

namespace Eagle
{
	enum class BroadphaseType
	{
		SweepAndPrune,
		MultiBoxPrune,
		AutomaticBoxPrune
	};

	enum class FrictionType
	{
		Patch,
		OneDirectional,
		TwoDirectional
	};

	enum class DebugType
	{
		ToFile,
		Live
	};

	struct PhysicsSettings
	{
		constexpr static uint32_t s_MinUpdateRate = 30u;
		constexpr static uint32_t s_MaxUpdateRate = 360u;

		static constexpr uint32_t MinPositionSolverIterations = 1u;
		static constexpr uint32_t MaxPositionSolverIterations = 255u;

		static constexpr uint32_t MinVelocitySolverIterations = 0u;
		static constexpr uint32_t MaxVelocitySolverIterations = 255u;
		
		uint32_t UpdateRate = 120u; // 120 fps
		glm::vec3 Gravity = { 0.f, -9.81f, 0.f };
		BroadphaseType BroadphaseAlgorithm = BroadphaseType::AutomaticBoxPrune;
		AABB WorldAABB = AABB(glm::vec3(-1000.f), glm::vec3(1000.f));
		uint32_t WorldBoundsSubdivisions = 2;
		FrictionType FrictionModel = FrictionType::Patch;
		bool bDebugOnPlay = false;
		bool bEditorScene = false;
		DebugType DebugType = DebugType::Live;
	};
}
