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

		bool operator== (const Rotator& other) const
		{
			return m_Rotation == other.m_Rotation;
		}

		const glm::quat& GetQuat() const { return m_Rotation; }
		glm::quat& GetQuat() { return m_Rotation; }

		Rotator operator*(const Rotator& other) const { return Rotator(m_Rotation * other.m_Rotation); }
		Rotator& operator*=(const Rotator& other) { m_Rotation = m_Rotation * other.m_Rotation; return *this; }

		Rotator Inverse() const { return Rotator(glm::inverse(m_Rotation)); }
		Rotator Conjugate() const { return Rotator(glm::conjugate(m_Rotation)); }

		Rotator& Normalize() { m_Rotation = glm::normalize(m_Rotation); return *this; }

		glm::mat4 ToMat4() const { return glm::toMat4(m_Rotation); }

		// Returns in radians
		glm::vec3 EulerAngles() const { return glm::eulerAngles(m_Rotation); }

	public:

		static Rotator FromEulerAngles(const glm::vec3& radians)
		{
			glm::quat result(1.f, 0.f, 0.f, 0.f);
			result = glm::rotate(result, radians.z, { 0.f, 0.f, 1.f });
			result = glm::rotate(result, radians.y, { 0.f, 1.f, 0.f });
			result = glm::rotate(result, radians.x, { 1.f, 0.f, 0.f });
			return Rotator(result);
		}

		static Rotator FromEulerAngles(float x, float y, float z)
		{
			glm::quat result(1.f, 0.f, 0.f, 0.f);
			result = glm::rotate(result, z, { 0.f, 0.f, 1.f });
			result = glm::rotate(result, y, { 0.f, 1.f, 0.f });
			result = glm::rotate(result, x, { 1.f, 0.f, 0.f });
			return Rotator(result);
		}

		static Rotator Unit() { static Rotator unit(1.f, 0.f, 0.f, 0.f); return unit; }

	private:
		glm::quat m_Rotation = glm::quat(1.f, 0.f, 0.f, 0.f);
	};

	struct Transform
	{
	public:
		glm::vec3 Location;
		Rotator Rotation;
		glm::vec3 Scale3D;

	public:
		constexpr Transform() : Location(0.f), Rotation(), Scale3D(1.f) {}
		constexpr Transform(const glm::vec3& location) : Location(location), Rotation(), Scale3D(1.f) {}
		constexpr Transform(const glm::vec3& location, const Rotator& rotation, const glm::vec3& scale = glm::vec3(1.f))
			: Location(location)
			, Rotation(rotation)
			, Scale3D(scale) {}

		constexpr Transform(const Transform&) = default;
		constexpr Transform(Transform&& other) = default;

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

		Transform operator- (const Transform& other) const
		{
			Transform result;
			result.Location = Location - other.Location;
			result.Rotation = Rotation * other.Rotation.Conjugate();
			result.Scale3D = Scale3D / other.Scale3D;

			return result;
		}

		constexpr bool operator== (const Transform& other) const
		{
			return Location == other.Location && Rotation == other.Rotation && Scale3D == other.Scale3D;
		}

		static Transform Blend(const Transform& tr1, const Transform& tr2, float weight)
		{
			Transform result;
			result.Location = glm::mix(tr1.Location, tr2.Location, weight);
			result.Rotation = glm::slerp(tr1.Rotation.GetQuat(), tr2.Rotation.GetQuat(), weight);
			result.Scale3D = glm::mix(tr1.Scale3D, tr2.Scale3D, weight);

			return result;
		}

		static Transform Blend(const Transform& tr1, const Transform& tr2, const Transform& tr3, const glm::vec3& buv)
		{
			Transform result;
			result.Location = tr1.Location * buv.x + tr2.Location * buv.y + tr3.Location * buv.z;
			result.Scale3D = tr1.Scale3D * buv.x + tr2.Scale3D * buv.y + tr3.Scale3D * buv.z;

			glm::quat q1 = tr1.Rotation.GetQuat();
			glm::quat q2 = tr2.Rotation.GetQuat();
			glm::quat q3 = tr3.Rotation.GetQuat();
			// Ensure all quaternions point in same hemisphere (dot > 0)
			if (glm::dot(q1, q2) < 0)
				q2 = -q2;
			if (glm::dot(q1, q3) < 0)
				q3 = -q3;

			result.Rotation = glm::normalize(buv.x * q1 + buv.y * q2 + buv.z * q3);
			return result;
		}
	};
}
