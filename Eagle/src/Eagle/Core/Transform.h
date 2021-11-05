#pragma once

#include <glm/glm.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace Eagle
{
	struct Rotator
	{
	public:
		Rotator() = default;
		Rotator(float w, float x, float y, float z) : m_Rotation(w, x, y, z) {}
		Rotator(const glm::quat& rotation) : m_Rotation(rotation) {}
		Rotator(const Rotator&) = default;

		Rotator& operator=(const Rotator& other) { m_Rotation = other.m_Rotation; return *this; }
		Rotator& operator=(const glm::quat& other) { m_Rotation = other; return *this; }

		//operator glm::quat() const { return m_Rotation; }

		const glm::quat& GetQuat() const { return m_Rotation; }
		glm::quat& GetQuat() { return m_Rotation; }

		Rotator operator*(const Rotator& other) const { return Rotator(other.m_Rotation * m_Rotation); }
		Rotator& operator*=(const Rotator& other) { m_Rotation = other.m_Rotation * m_Rotation; return *this; }

		Rotator Inverse() const { return Rotator(glm::inverse(m_Rotation)); }

		void Normalize() { m_Rotation = glm::normalize(m_Rotation); }

		glm::mat4 ToMat4() const { return glm::toMat4(m_Rotation); }

		//Returns in radians
		glm::vec3 EulerAngles() const { return glm::eulerAngles(m_Rotation); }

		bool IsNormalized() const
		{
			float result = m_Rotation.x * m_Rotation.x + m_Rotation.y * m_Rotation.y + m_Rotation.z * m_Rotation.z + m_Rotation.w * m_Rotation.w;
			return glm::abs(result - 1.0f) < 1e-4;
		}

	public:
		//In radians
		static Rotator FromEulerAngles(const glm::vec3& angles)
		{
			return Rotator(glm::quat(angles));
		}

		static Rotator Unit() { static Rotator unit(1.f, 0.f, 0.f, 0.f); return unit; }

	private:
		glm::quat m_Rotation = glm::quat(1.f, 0.f, 0.f, 0.f);
	};

	struct Transform
	{
	public:
		glm::vec3 Location; //TODO: Change to Vector
		Rotator Rotation; //Radians. TODO: Change to Rotator(Quat)
		glm::vec3 Scale3D; //TODO: Change to Vector

	public:
		Transform() : Location(0.f), Rotation(), Scale3D(1.f) {}

		Transform(const glm::vec3& location, const Rotator& rotation, const glm::vec3& scale = glm::vec3(1.f)) //TODO: Add init from Matrix
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
			result.Rotation = Rotation * other.Rotation;
			result.Scale3D = Scale3D * other.Scale3D;

			return result;
		}

		Transform& operator+= (const Transform& other)
		{
			Location = Location + other.Location;
			Rotation = Rotation * other.Rotation;
			Scale3D = Scale3D * other.Scale3D;

			return *this;
		}

		Transform operator- (const Transform& other)
		{
			Transform result;
			result.Location = Location - other.Location;
			result.Rotation = Rotation * other.Rotation.Inverse();
			result.Scale3D = Scale3D - other.Scale3D;

			return result;
		}
	};
}
