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
		m_Transform.Location.z = 15.f;

		RecalculateProjection();
		RecalculateView();
	}

	void EditorCamera::OnUpdate(Timestep ts)
	{
		if (Input::IsMouseButtonPressed(Mouse::ButtonRight))
		{
			float offsetX = m_MouseX - Input::GetMouseX();
			float offsetY = Input::GetMouseY() - m_MouseY;

			if (Input::IsCursorVisible())
			{
				Input::SetShowCursor(false);

				offsetX = offsetY = 0.f;
			}

			glm::vec3& Location = m_Transform.Location;
			glm::vec3& Rotation = m_EulerRotation;

			Rotation.x -= glm::radians(offsetY * m_MouseRotationSpeed);
			Rotation.y += glm::radians(offsetX * m_MouseRotationSpeed);
			Rotation.z = 0.f;

			m_Transform.Rotation = Rotator::FromEulerAngles(Rotation);

			glm::vec3 forward = GetForwardVector();
			glm::vec3 right = GetRightVector();

			if (Input::IsKeyPressed(Key::W))
				Location += (forward * (m_MoveSpeed * ts));
			if (Input::IsKeyPressed(Key::S))
				Location -= (forward * (m_MoveSpeed * ts));
			if (Input::IsKeyPressed(Key::Q))
				Location.y -= m_MoveSpeed * ts;
			if (Input::IsKeyPressed(Key::E))
				Location.y += m_MoveSpeed * ts;
			if (Input::IsKeyPressed(Key::A))
				Location -= (right * (m_MoveSpeed * ts));
			if (Input::IsKeyPressed(Key::D))
				Location += (right * (m_MoveSpeed * ts));

			m_MouseX = Input::GetMouseX();
			m_MouseY = Input::GetMouseY();

			RecalculateView();
		}
		else
		{
			if (Input::IsCursorVisible() == false)
			{
				Input::SetShowCursor(true);
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
		constexpr glm::vec3 upVector = glm::vec3(0, 1, 0);
		constexpr glm::vec3 pitchVector = glm::vec3(1, 0, 0);

		glm::mat4 camera = glm::translate(glm::mat4(1.f), m_Transform.Location);
		camera = glm::rotate(camera, m_EulerRotation.y, upVector);
		camera = glm::rotate(camera, m_EulerRotation.x, pitchVector);

		// now get the view matrix by taking the camera inverse
		m_ViewMatrix = glm::inverse(camera);
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
