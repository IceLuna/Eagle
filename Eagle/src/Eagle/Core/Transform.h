#pragma once

#include <glm/glm.hpp>

namespace Eagle
{
	struct Transform
	{
	public:
		glm::vec3 Translation; //TODO: Change to Vector
		glm::vec3 Rotation; //TODO: Change to Quat
		glm::vec3 Scale3D; //TODO: Change to Rotator

	public:
		Transform() : Translation(0.f), Rotation(0.f), Scale3D(1.f) {}

		Transform(const glm::vec3& translation, const glm::vec3& rotation, const glm::vec3& scale = glm::vec3(1.f)) //TODO: Add init from Matrix
			: Translation(translation)
			, Rotation(rotation)
			, Scale3D(scale) {}
	};
}
