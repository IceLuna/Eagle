#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Eagle
{
	class Camera
	{
	public:
		virtual ~Camera() = default;

		virtual const glm::mat4& GetViewProjectionMatrix() const = 0;
		virtual const glm::mat4& GetViewMatrix() const = 0;
		virtual const glm::mat4& GetProjectionMatrix() const = 0;

		virtual void SetPosition(const glm::vec3& position) = 0;
		virtual const glm::vec3& GetPosition() const = 0;

		virtual void SetRotation(const glm::vec3& rotation) = 0;
		virtual const glm::vec3& GetRotation() const = 0;
	};
}