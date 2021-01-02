#pragma once

#include "Camera.h"

namespace Eagle
{

	class OrthographicCamera : public Camera
	{
	public:
		OrthographicCamera(float left, float right, float bottom, float top);

		void SetProjection(float left, float right, float bottom, float top);

		virtual const glm::mat4& GetViewProjectionMatrix() const override;
		virtual const glm::mat4& GetViewMatrix() const override;
		virtual const glm::mat4& GetProjectionMatrix() const override;

		virtual void SetPosition(const glm::vec3& position) override;
		virtual const glm::vec3& GetPosition() const override;


		virtual void SetRotation(const glm::vec3& rotation) override;
		virtual const glm::vec3& GetRotation() const override;

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