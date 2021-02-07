#pragma once

#include "Camera.h"

namespace Eagle
{

	class OrthographicCamera : public Camera
	{
	public:
		OrthographicCamera(float left, float right, float bottom, float top);

		void SetProjection(float left, float right, float bottom, float top);

		const glm::mat4& GetViewProjectionMatrix() const;
		const glm::mat4& GetViewMatrix() const;
		const glm::mat4& GetProjectionMatrix() const;

		void SetPosition(const glm::vec3& position);
		const glm::vec3& GetPosition() const;

		void SetRotation(const glm::vec3& rotation);
		const glm::vec3& GetRotation() const;

	private:
		void RecalculateViewProjection();

	private:
		glm::mat4 m_Projection;
		glm::mat4 m_View;
		glm::mat4 m_ViewProjection;

		glm::vec3 m_Position;
		glm::vec3 m_ZRotation;
	};

}