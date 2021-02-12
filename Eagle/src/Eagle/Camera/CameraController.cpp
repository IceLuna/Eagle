#include "egpch.h"

#include "CameraController.h"
#include "Eagle/Events/MouseEvent.h"
#include "Eagle/Input/Input.h"

#include "Eagle/Components/Components.h"

namespace Eagle
{
	void CameraController::OnUpdate(Timestep ts)
	{
		if (m_Entity.HasComponent<CameraComponent>())
		{
			auto& cameraComponent = m_Entity.GetComponent<CameraComponent>();
			if (cameraComponent.Primary)
			{
				if (Input::IsMouseButtonPressed(Mouse::ButtonRight))
				{
					if (Input::IsCursorVisible())
						Input::SetShowCursor(false);

					auto& transform = cameraComponent.Transform;

					float offsetX = m_MouseX - Input::GetMouseX();
					float offsetY = Input::GetMouseY() - m_MouseY;

					glm::vec3& Translation = transform.Translation;
					glm::vec3& Rotation = transform.Rotation;

					Rotation.y += glm::radians(offsetX * ts * m_MouseRotationSpeed);
					Rotation.x -= glm::radians(offsetY * ts * m_MouseRotationSpeed);

					glm::vec3 forward = cameraComponent.GetForwardDirection();
					glm::vec3 right = cameraComponent.GetRightDirection();
					glm::vec3 up = cameraComponent.GetUpDirection();

					if (Input::IsKeyPressed(Key::W))
					{
						Translation += (forward * (m_MoveSpeed * ts));
					}
					if (Input::IsKeyPressed(Key::S))
					{
						Translation -= (forward * (m_MoveSpeed * ts));
					}
					if (Input::IsKeyPressed(Key::Q))
					{
						Translation -= (up * (m_MoveSpeed * ts));
					}
					if (Input::IsKeyPressed(Key::E))
					{
						Translation += (up * (m_MoveSpeed * ts));
					}
					if (Input::IsKeyPressed(Key::A))
					{
						Translation -= (right * (m_MoveSpeed * ts));
					}
					if (Input::IsKeyPressed(Key::D))
					{
						Translation += (right * (m_MoveSpeed * ts));
					}
				}
				else
				{
					if (Input::IsCursorVisible() == false)
						Input::SetShowCursor(true);
				}
			}

			m_MouseX = Input::GetMouseX();
			m_MouseY = Input::GetMouseY();
		}
	}

	void CameraController::OnEvent(Event& e)
	{
		EventDispatcher dispatcher(e);

		//dispatcher.Dispatch<MouseScrolledEvent>(EG_BIND_FN(CameraController::OnMouseScrolled));
	}

	bool CameraController::OnMouseScrolled(MouseScrolledEvent& e)
	{
		auto& cameraComponent = m_Entity.GetComponent<CameraComponent>();
		if (cameraComponent.Primary)
		{
			auto& camera = cameraComponent.Camera;

			float zoomLevel = e.GetYOffset() * m_ScrollSpeed;

			cameraComponent.Transform.Translation.z -= zoomLevel;
		}

		return true;
	}
}