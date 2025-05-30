#pragma once

#include "Eagle/Camera/Camera.h"
#include "Eagle/Math/Math.h"

namespace Eagle
{
	class MouseScrolledEvent;

	class EditorCamera : public Camera
	{
	public:
		EditorCamera();

		virtual void OnUpdate(Timestep ts, bool bProcessInputs);
		virtual void OnEvent(Event & e);
		
		void SetTransform(const Transform& transform)
		{
			m_Transform = transform;
			RecalculateView();
		}
		void SetLocation(const glm::vec3& pos)
		{
			m_Transform.Location = pos;
			RecalculateView();
		}

		const glm::mat4& GetViewMatrix() const { return m_ViewMatrix; }
		glm::mat4 GetViewProjection() const { return m_Projection * m_ViewMatrix; }

		glm::vec3 GetForwardVector() const { return Math::GetForwardVector(GetRotation()); }
		glm::vec3 GetUpVector() const { return Math::GetUpVector(GetRotation()); }
		glm::vec3 GetRightVector() const { return Math::GetRightVector(GetRotation()); }

		const Transform& GetTransform() const { return m_Transform; }
		const glm::vec3& GetLocation() const { return m_Transform.Location; };
		const Rotator& GetRotation() const { return m_Transform.Rotation; };

		float GetMoveSpeed() const { return m_MoveSpeed; }
		void  SetMoveSpeed(float speed) { m_MoveSpeed = speed; }

		float GetRotationSpeed() const { return m_MouseRotationSpeed; }
		void  SetRotationSpeed(float speed) { m_MouseRotationSpeed = speed; }

		void LookAt(const glm::vec3& pos);

	private:
		void RecalculateView();
		float UpdateAccelerationAndGetDelta(Timestep ts, bool bIncrease);

	protected:
		bool OnMouseScrolled(MouseScrolledEvent& e);

	protected:
		Transform m_Transform;
		glm::mat4 m_ViewMatrix = glm::mat4(1.f);

		//Input speeds
		float m_MouseX = 0.f;
		float m_MouseY = 0.f;

		float m_MoveSpeed = 12.25f;
		float m_MouseRotationSpeed = 0.03f;
		uint64_t m_NumberOfFramesMoving = 0; // Counts frames while camera is activated

		glm::vec3 m_LastMovingDir = glm::vec3(0.f);
		float m_Acceleration = 0.f; // in range from [0; 1]. Used for smooth movement
	};
}
