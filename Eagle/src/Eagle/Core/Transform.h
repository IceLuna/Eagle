#pragma once

#include <glm/glm.hpp>

namespace Eagle
{
	struct Transform
	{
	public:
		glm::vec3 Location; //TODO: Change to Vector
		glm::vec3 Rotation; //Radians. TODO: Change to Rotator(Quat)
		glm::vec3 Scale3D; //TODO: Change to Vector

	public:
		Transform() : Location(0.f), Rotation(0.f), Scale3D(1.f) {}

		Transform(const glm::vec3& location, const glm::vec3& rotation, const glm::vec3& scale = glm::vec3(1.f)) //TODO: Add init from Matrix
			: Location(location)
			, Rotation(rotation)
			, Scale3D(scale) {}

		Transform(const Transform&) = default;
		Transform(Transform&& other) = default;

		Transform& operator= (const Transform&) = default;
		Transform& operator= (Transform&& other) = default;

		Transform operator+ (const Transform& other) const
		{
			Transform result;
			result.Location = Location + other.Location;
			result.Rotation = Rotation + other.Rotation;
			result.Scale3D = Scale3D * other.Scale3D;

			return result;
		}

		Transform& operator+= (const Transform& other)
		{
			Location = Location + other.Location;
			Rotation = Rotation + other.Rotation;
			Scale3D = Scale3D * other.Scale3D;

			return *this;
		}

		Transform operator- (const Transform& other)
		{
			Transform result;
			result.Location = Location - other.Location;
			result.Rotation = Rotation - other.Rotation;
			result.Scale3D = Scale3D - other.Scale3D;

			return result;
		}
	};
}
