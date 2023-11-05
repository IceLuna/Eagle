#include "egpch.h"

#include "Eagle/Core/Application.h"
#include "EditorCamera.h"
#include "Eagle/Input/Input.h"
#include "Eagle/Events/MouseEvent.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace Eagle
{
	EditorCamera::EditorCamera()
	{
		m_Transform.Location.y = 5.f;
		m_Transform.Location.z = 15.f;

		RecalculateProjection();
		RecalculateView();
	}

	void EditorCamera::OnUpdate(Timestep ts, bool bProcessInputes)
	{
		if (bProcessInputes && Input::IsMouseButtonPressed(Mouse::ButtonRight))
		{
			float offsetX = m_MouseX - Input::GetMouseX();
			float offsetY = m_MouseY - Input::GetMouseY();

			// There's a GLFW bug when mouse pos jumps on second frame, so here we're ignoring mouse delta on first two frames
			if (m_NumberOfFramesMoving++ < 2)
				offsetX = offsetY = 0.f;

			if (Input::IsMouseVisible())
				Input::SetShowMouse(false);

			glm::vec3 forward = GetForwardVector();

			// Limit cameras vertical rotation
			// Shouldn't prevent camera from moving if camera wants to move away from max rotation
			const float cosTheta = glm::dot(forward, glm::vec3(0.f, 1.f, 0.f));
			const bool bMoveInTheSameDir = glm::sign(cosTheta) == glm::sign(offsetY);
			const bool bStopXRotation = (glm::abs(cosTheta) > 0.999f) && bMoveInTheSameDir ? true : false;
			
			float rotationX = bStopXRotation ? 0.f : glm::radians(offsetY * m_MouseRotationSpeed);
			float rotationY = glm::radians(offsetX * m_MouseRotationSpeed);

			glm::quat rotX = glm::angleAxis(rotationX, glm::vec3(1,0,0));
			glm::quat rotY = glm::angleAxis(rotationY, glm::vec3(0,1,0));
			
			glm::quat& rotation = m_Transform.Rotation.GetQuat();
			glm::quat origRotation = rotation;
			rotation = rotation * rotX;
			rotation = rotY * rotation;

			// If we flipped over, reset back
			{
				forward = GetForwardVector();
				const float cosThetaAfter = glm::dot(forward, glm::vec3(0.f, 1.f, 0.f));
				const bool bLock = bMoveInTheSameDir && glm::abs(cosThetaAfter) < glm::abs(cosTheta);
				if (bLock)
				{
					rotation = origRotation;
					rotation = rotY * rotation;
				}
			}

			glm::vec3 right = GetRightVector();

			const float moveSpeed = m_MoveSpeed * ts;

			glm::vec3 direction = glm::vec3(0.f);
			if (Input::IsKeyPressed(Key::W))
				direction += (forward * moveSpeed);
			if (Input::IsKeyPressed(Key::S))
				direction -= (forward * moveSpeed);
			if (Input::IsKeyPressed(Key::Q))
				direction.y -= moveSpeed;
			if (Input::IsKeyPressed(Key::E))
				direction.y += moveSpeed;
			if (Input::IsKeyPressed(Key::A))
				direction -= (right * moveSpeed);
			if (Input::IsKeyPressed(Key::D))
				direction += (right * moveSpeed);

			// if moving by input
			if (glm::dot(direction, direction))
			{
				const float delta = UpdateAccelerationAndGetDelta(ts, true);
				m_Transform.Location += direction * delta;
				m_LastMovingDir = direction;
			}
			else
			{
				const float delta = UpdateAccelerationAndGetDelta(ts, false);
				m_Transform.Location += m_LastMovingDir * delta;
			}

			m_MouseX = Input::GetMouseX();
			m_MouseY = Input::GetMouseY();

			RecalculateView();
		}
		else
		{
			if (m_Acceleration > 0.f)
			{
				const float delta = UpdateAccelerationAndGetDelta(ts, false);
				m_Transform.Location += m_LastMovingDir * delta;
				RecalculateView();
			}

			m_NumberOfFramesMoving = 0;
			if (Input::IsMouseVisible() == false)
			{
				Input::SetShowMouse(true);
			}
		}
	}

	void EditorCamera::OnEvent(Event& e)
	{
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<MouseScrolledEvent>(EG_BIND_FN(EditorCamera::OnMouseScrolled));
	}

	void EditorCamera::RecalculateView()
	{
		const glm::mat4 R = m_Transform.Rotation.ToMat4();
		const glm::mat4 T = glm::translate(glm::mat4(1.0f), m_Transform.Location);
		m_ViewMatrix = T * R;
		m_ViewMatrix = glm::inverse(m_ViewMatrix);
	}

	float EditorCamera::UpdateAccelerationAndGetDelta(Timestep ts, bool bIncrease)
	{
		constexpr float time = 4.f;

		if (bIncrease)
			m_Acceleration = glm::min(m_Acceleration + ts * time, 1.f);
		else
			m_Acceleration = glm::max(m_Acceleration - ts * time, 0.f);
		return glm::smoothstep(0.f, 1.f, m_Acceleration);
	}

	bool EditorCamera::OnMouseScrolled(MouseScrolledEvent& e)
	{
		if (Input::IsMouseButtonPressed(Mouse::ButtonRight))
		{
			m_MoveSpeed += e.GetYOffset() * 0.25f;
			m_MoveSpeed = std::max(0.1f, m_MoveSpeed);
		}
		
		return false;
	}
}
