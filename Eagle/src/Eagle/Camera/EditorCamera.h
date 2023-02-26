#pragma once

#include "Eagle/Camera/Camera.h"
#include "Eagle/Core/Transform.h"

namespace Eagle
{
	class MouseScrolledEvent;

	class EditorCamera : public Camera
	{
	public:
		EditorCamera();

		virtual void OnUpdate(Timestep ts);
		virtual void OnEvent(Event & e);
		
		const Transform& GetTransform() const { return m_Transform; }
		void SetTransform(const Transform& transform)
		{
			constexpr float rad = glm::radians(180.f);

			m_Transform = transform;
			m_EulerRotation = transform.Rotation.EulerAngles();
			if (glm::abs(m_EulerRotation.z) == rad)
			{
				m_EulerRotation.x += -rad;
				m_EulerRotation.y -= rad;
				m_EulerRotation.y *= -1.f;
				m_EulerRotation.z = 0.f;
			}
			RecalculateView();
		}

		const glm::mat4& GetViewMatrix() const { return m_ViewMatrix; }
		glm::mat4 GetViewProjection() const { return m_Projection * m_ViewMatrix; }

		glm::vec3 GetForwardVector() const { return glm::rotate(GetOrientation().GetQuat(), glm::vec3(0.f, 0.f, -1.f)); }
		glm::vec3 GetUpVector() const { return glm::rotate(GetOrientation().GetQuat(), glm::vec3(0.f, 1.f, 0.f)); }
		glm::vec3 GetRightVector() const { return glm::rotate(GetOrientation().GetQuat(), glm::vec3(1.f, 0.f, 0.f)); }

		Rotator GetOrientation() const { return Rotator::FromEulerAngles({ m_EulerRotation.x, m_EulerRotation.y, 0.f }); }
		const glm::vec3& GetLocation() const { return m_Transform.Location; };
		const Rotator& GetRotation() const { return m_Transform.Rotation; };

		float GetMoveSpeed() const { return m_MoveSpeed; }
		void  SetMoveSpeed(float speed) { m_MoveSpeed = speed; }

		float GetRotationSpeed() const { return m_MouseRotationSpeed; }
		void  SetRotationSpeed(float speed) { m_MouseRotationSpeed = speed; }

	private:
		void RecalculateView();

	protected:
		bool OnMouseScrolled(MouseScrolledEvent& e);

	protected:
		Transform m_Transform;
		glm::vec3 m_EulerRotation = glm::vec3(0.f);
		glm::mat4 m_ViewMatrix = glm::mat4(1.f);

		//Input speeds
		float m_MouseX = 0.f;
		float m_MouseY = 0.f;

		float m_MoveSpeed = 1.75f; //0.225f;
		float m_MouseRotationSpeed = 0.03f; //0.225f;
	};
}
