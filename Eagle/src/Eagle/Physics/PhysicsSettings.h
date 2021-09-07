#pragma once

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
		float FixedTimeStep = 1.f / 120.f;
		glm::vec3 Gravity = { 0.f, -9.81f, 0.f };
		BroadphaseType BroadphaseAlgorithm = BroadphaseType::AutomaticBoxPrune;
		glm::vec3 WorldBoundsMin = glm::vec3(-100.f);
		glm::vec3 WorldBoundsMax = glm::vec3(100.f);
		uint32_t WorldBoundsSubdivisions = 2;
		FrictionType FrictionModel = FrictionType::Patch;
		uint32_t SolverIterations = 8;
		uint32_t SolverVelocityIterations = 2;
		bool DebugOnPlay = true;
		DebugType DebugType = DebugType::Live;
	};
}
