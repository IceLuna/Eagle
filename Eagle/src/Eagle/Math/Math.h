#pragma once

#include <glm/glm.hpp>
#include "Eagle/Math/Transform.h"

namespace Eagle::Math
{
	bool DecomposeTransformMatrix(const glm::mat4& transformMatrix, glm::vec3& outLocation, glm::vec3& outRotation, glm::vec3& outScale);
	Transform DecomposeTransformMatrix(const glm::mat4& transformMatrix);

	glm::mat4 ToTransformMatrix(const Eagle::Transform& transform);

	static glm::mat4 GetRotationMatrix(const Rotator& rotation)
	{
		return rotation.ToMat4();
	}

	static glm::vec3 WorldPosFromDepth(const glm::mat4& VPInv, glm::vec2 uv, float depth)
	{
		const glm::vec4 clipSpacePos = glm::vec4(uv * 2.f - 1.f, depth, 1.0);
		glm::vec4 worldSpacePos = VPInv * clipSpacePos;
		worldSpacePos /= worldSpacePos.w;

		return worldSpacePos;
	}

	static glm::mat4 Perspective(float fov, float aspectRatio, float nearPlane, float farPlane, bool bReversedZ = true)
	{
		if (bReversedZ)
			return glm::perspective(fov, aspectRatio, farPlane, nearPlane);
		else
			return glm::perspective(fov, aspectRatio, nearPlane, farPlane);
	}

	static glm::mat4 Ortho(float left, float right, float bottom, float top, float nearPlane, float farPlane, bool bReversedZ = true)
	{
		if (bReversedZ)
			return glm::ortho(left, right, bottom, top, farPlane, nearPlane);
		else
			return glm::ortho(left, right, bottom, top, nearPlane, farPlane);
	}

	static inline glm::vec3 GetForwardVector(const Rotator& rotation)
	{
		return glm::rotate(rotation.GetQuat(), glm::vec3(0.f, 0.f, -1.f));
	}

	static inline glm::vec3 GetUpVector(const Rotator& rotation)
	{
		return glm::rotate(rotation.GetQuat(), glm::vec3(0.f, 1.f, 0.f));
	}

	static inline glm::vec3 GetRightVector(const Rotator& rotation)
	{
		return glm::rotate(rotation.GetQuat(), glm::vec3(1.f, 0.f, 0.f));
	}

	static bool IsNearlyZero(const glm::vec3& v, float tolerance = 0.0001f)
	{
		return
			glm::abs(v.x) <= tolerance
			&& glm::abs(v.y) <= tolerance
			&& glm::abs(v.z) <= tolerance;
	}

	// Returns degree of the angle between Velocity and rotation's forward vector
	// The range of return will be from[-180, 180].
	float CalculateDirection(const glm::vec3& velocity, const Rotator& rotation);

	inline glm::quat RotationAroundX(float degrees)
	{
		const float rad = glm::radians(degrees);
		return glm::angleAxis(rad, glm::vec3(1.0f, 0.0f, 0.0f));
	}

	inline glm::quat RotationAroundY(float degrees)
	{
		const float rad = glm::radians(degrees);
		return glm::angleAxis(rad, glm::vec3(0.0f, 1.0f, 0.0f));
	}

	inline glm::quat RotationAroundZ(float degrees)
	{
		const float rad = glm::radians(degrees);
		return glm::angleAxis(rad, glm::vec3(0.0f, 0.0f, 1.0f));
	}

	inline glm::quat RotateAroundX(const glm::quat& q, float degrees)
	{
		return RotationAroundX(degrees) * q;
	}

	inline glm::quat RotateAroundY(const glm::quat& q, float degrees)
	{
		return RotationAroundY(degrees) * q;
	}

	inline glm::quat RotateAroundZ(const glm::quat& q, float degrees)
	{
		return RotationAroundZ(degrees) * q;
	}
}
