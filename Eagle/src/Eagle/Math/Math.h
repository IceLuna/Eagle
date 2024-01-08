#pragma once

#include <glm/glm.hpp>
#include "Eagle/Core/Transform.h"

namespace Eagle::Math
{
	bool DecomposeTransformMatrix(const glm::mat4& transformMatrix, glm::vec3& outLocation, glm::vec3& outRotation, glm::vec3& outScale);

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
}
