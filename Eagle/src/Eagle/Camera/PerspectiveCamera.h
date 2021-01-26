#pragma once

#include "Camera.h"

namespace Eagle
{
	class PerspectiveCamera : public Camera
	{
	public:
		virtual const glm::mat4& GetViewProjectionMatrix() const = 0;
		virtual const glm::mat4& GetViewMatrix() const = 0;
		virtual const glm::mat4& GetProjectionMatrix() const = 0;

		virtual void SetPosition(const glm::vec3& position) = 0;
		virtual const glm::vec3& GetPosition() const = 0;

	private:
		float FOV;
		glm::mat4 m_Projection;
		glm::mat4 m_View;
		glm::mat4 m_ViewProjection;
		glm::vec3 m_Position;
	};
}