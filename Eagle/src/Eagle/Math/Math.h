#pragma once

#include <glm/glm.hpp>
#include "Eagle/Core/Transform.h"

namespace Eagle::Math
{
	bool DecomposeTransformMatrix(const glm::mat4& transformMatrix, glm::vec3& outLocation, glm::vec3& outRotation, glm::vec3& outScale);

	glm::mat4 ToTransformMatrix(const Eagle::Transform& transform);

	glm::mat4 GetRotationMatrix(const Rotator& rotation);
}
