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

		virtual void OnUpdate(Timestep ts, bool bProcessInputes);
		virtual void OnEvent(Event & e);
		
		const Transform& GetTransform() const { return m_Transform; }
		void SetTransform(const Transform& transform)
		{
			m_Transform = transform;
			RecalculateView();
		}

		const glm::mat4& GetViewMatrix() const { return m_ViewMatrix; }
		glm::mat4 GetViewProjection() const { return m_Projection * m_ViewMatrix; }

		glm::vec3 GetForwardVector() const { return glm::rotate(GetRotation().GetQuat(), glm::vec3(0.f, 0.f, -1.f)); }
		glm::vec3 GetUpVector() const { return glm::rotate(GetRotation().GetQuat(), glm::vec3(0.f, 1.f, 0.f)); }
		glm::vec3 GetRightVector() const { return glm::rotate(GetRotation().GetQuat(), glm::vec3(1.f, 0.f, 0.f)); }

		const glm::vec3& GetLocation() const { return m_Transform.Location; };
		const Rotator& GetRotation() const { return m_Transform.Rotation; };

		float GetMoveSpeed() const { return m_MoveSpeed; }
		void  SetMoveSpeed(float speed) { m_MoveSpeed = speed; }

		float GetRotationSpeed() const { return m_MouseRotationSpeed; }
		void  SetRotationSpeed(float speed) { m_MouseRotationSpeed = speed; }

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

		float m_MoveSpeed = 1.75f; //0.225f;
		float m_MouseRotationSpeed = 0.03f; //0.225f;
		uint64_t m_NumberOfFramesMoving = 0; // Counts frames while camera is activated

		glm::vec3 m_LastMovingDir = glm::vec3(0.f);
		float m_Acceleration = 0.f; // in range from [0; 1]. Used for smooth movement
	};
}
