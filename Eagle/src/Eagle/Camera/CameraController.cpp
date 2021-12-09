#include "egpch.h"

#include "CameraController.h"
#include "Eagle/Events/MouseEvent.h"
#include "Eagle/Input/Input.h"

#include "Eagle/Components/Components.h"

namespace Eagle
{
	void CameraController::OnCreate()
	{
		if (m_Entity.HasComponent<CameraComponent>())
		{
			auto& cameraComponent = m_Entity.GetComponent<CameraComponent>();
			m_EulerRotation = cameraComponent.GetWorldTransform().Rotation.EulerAngles();
			constexpr float rad = glm::radians(180.f);
			if (glm::abs(m_EulerRotation.z) == rad)
			{
				m_EulerRotation.x += -rad;
				m_EulerRotation.y -= rad;
				m_EulerRotation.y *= -1.f;
				m_EulerRotation.z = 0.f;
			}
		}
	}

	void CameraController::OnUpdate(Timestep ts)
	{
		if (m_Entity.HasComponent<CameraComponent>())
		{
			auto& cameraComponent = m_Entity.GetComponent<CameraComponent>();
			if (cameraComponent.Primary)
			{
				Transform transform = cameraComponent.GetWorldTransform();

				if (Input::IsMouseButtonPressed(Mouse::ButtonRight))
				{
					float offsetX = m_MouseX - Input::GetMouseX();
					float offsetY = Input::GetMouseY() - m_MouseY;

					if (Input::IsCursorVisible())
					{
						Input::SetShowCursor(false);

						offsetX = offsetY = 0.f;
					}

					glm::vec3& Location = transform.Location;
					glm::vec3& Rotation = m_EulerRotation;

					Rotation.x -= glm::radians(offsetY * m_MouseRotationSpeed);
					Rotation.y += glm::radians(offsetX * m_MouseRotationSpeed);

					constexpr float maxRadians = glm::radians(89.9f);
					constexpr float minRadians = glm::radians(-89.9f);

					if (Rotation.x >= maxRadians)
						Rotation.x = maxRadians;
					else if (Rotation.x <= minRadians)
						Rotation.x = minRadians;

					transform.Rotation = Rotator::FromEulerAngles(Rotation);

					glm::vec3 forward = cameraComponent.GetForwardVector();
					glm::vec3 right = cameraComponent.GetRightVector();

					if (Input::IsKeyPressed(Key::W))
					{
						Location += (forward * (m_MoveSpeed * ts));
					}
					if (Input::IsKeyPressed(Key::S))
					{
						Location -= (forward * (m_MoveSpeed * ts));
					}
					if (Input::IsKeyPressed(Key::Q))
					{
						Location.y -= m_MoveSpeed * ts;
					}
					if (Input::IsKeyPressed(Key::E))
					{
						Location.y += m_MoveSpeed * ts;
					}
					if (Input::IsKeyPressed(Key::A))
					{
						Location -= (right * (m_MoveSpeed * ts));
					}
					if (Input::IsKeyPressed(Key::D))
					{
						Location += (right * (m_MoveSpeed * ts));
					}

					m_MouseX = Input::GetMouseX();
					m_MouseY = Input::GetMouseY();

					cameraComponent.SetWorldTransform(transform);
				}
				else
				{
					if (Input::IsCursorVisible() == false)
					{
						Input::SetShowCursor(true);
					}
				}
			}
		}
	}

	void CameraController::OnEvent(Event& e)
	{
		EventDispatcher dispatcher(e);
	}
}