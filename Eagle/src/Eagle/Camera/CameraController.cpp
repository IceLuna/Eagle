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
			m_EulerRotation.z = 0.f;
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

				static bool bSecondMouseUpdateFrame = false;
				if (Input::IsMouseButtonPressed(Mouse::ButtonRight))
				{
					float offsetX = m_MouseX - Input::GetMouseX();
					float offsetY = Input::GetMouseY() - m_MouseY;

					//Otherwise, mouse pos jumps on second frame. Idk why...
					if (bSecondMouseUpdateFrame)
					{
						offsetX = offsetY = 0.f;
						bSecondMouseUpdateFrame = false;
					}

					if (Input::IsCursorVisible())
					{
						Input::SetShowCursor(false);

						offsetX = offsetY = 0.f;
						bSecondMouseUpdateFrame = true;
					}

					glm::vec3& Location = transform.Location;
					glm::vec3& Rotation = m_EulerRotation;

					Rotation.x -= glm::radians(offsetY * m_MouseRotationSpeed);
					Rotation.y += glm::radians(offsetX * m_MouseRotationSpeed);
					Rotation.z = 0.f;

					transform.Rotation = Rotator::FromEulerAngles(Rotation);

					glm::vec3 forward = cameraComponent.GetForwardDirection();
					glm::vec3 right = cameraComponent.GetRightDirection();

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