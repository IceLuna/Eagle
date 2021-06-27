#pragma once

#include <glm/glm.hpp>

namespace Eagle
{
	struct Transform
	{
	public:
		glm::vec3 Translation; //TODO: Change to Vector
		glm::vec3 Rotation; //TODO: Change to Rotator(Quat)
		glm::vec3 Scale3D; //TODO: Change to Vector

	public:
		Transform() : Translation(0.f), Rotation(0.f), Scale3D(1.f) {}

		Transform(const glm::vec3& translation, const glm::vec3& rotation, const glm::vec3& scale = glm::vec3(1.f)) //TODO: Add init from Matrix
			: Translation(translation)
			, Rotation(rotation)
			, Scale3D(scale) {}

		Transform(const Transform&) = default;
		Transform(Transform&& other) = default;

		Transform& operator= (const Transform&) = default;
		Transform& operator= (Transform&& other) = default;

		Transform operator+ (const Transform& other)
		{
			Transform result;
			result.Translation = Translation + other.Translation;
			result.Rotation = Rotation + other.Rotation;
			result.Scale3D = Scale3D + other.Scale3D;

			return result;
		}

		Transform operator- (const Transform& other)
		{
			Transform result;
			result.Translation = Translation - other.Translation;
			result.Rotation = Rotation - other.Rotation;
			result.Scale3D = Scale3D - other.Scale3D;

			return result;
		}
	};
}
