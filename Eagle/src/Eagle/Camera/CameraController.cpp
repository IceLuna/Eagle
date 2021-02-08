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
				glm::vec3& Translation = cameraComponent.Transform.Translation;
				glm::vec3& Rotation = cameraComponent.Transform.Rotation;

				if (Input::IsMouseButtonPressed(Mouse::ButtonRight))
				{
					float offsetX = m_MouseX - Input::GetMouseX();
					float offsetY = Input::GetMouseY() - m_MouseY;

					Rotation.y += glm::radians(offsetX * ts * m_MouseRotationSpeed);
					Rotation.x -= glm::radians(offsetY * ts * m_MouseRotationSpeed);

					glm::mat4 viewMatrix = cameraComponent.GetViewMatrix();
					glm::vec3 forward = glm::normalize(glm::vec3(viewMatrix[2]));
					glm::vec3 right = glm::normalize(glm::vec3(viewMatrix[0]));
					glm::vec3 up = glm::normalize(glm::vec3(viewMatrix[1]));
					forward.z *= -1.f;

					if (Input::IsKeyPressed(Key::W))
					{
						Translation += forward * m_MoveSpeed;
					}
					if (Input::IsKeyPressed(Key::S))
					{
						Translation -= forward * m_MoveSpeed;
					}
					if (Input::IsKeyPressed(Key::Q))
					{
						Translation -= up * m_MoveSpeed;
					}
					if (Input::IsKeyPressed(Key::E))
					{
						Translation += up * m_MoveSpeed;
					}
					if (Input::IsKeyPressed(Key::A))
					{
						Translation -= right * m_MoveSpeed;
					}
					if (Input::IsKeyPressed(Key::D))
					{
						Translation += right * m_MoveSpeed;
					}
				}

				m_MouseX = Input::GetMouseX();
				m_MouseY = Input::GetMouseY();
			}
		}
	}

	void CameraController::OnEvent(Event& e)
	{
		EventDispatcher dispatcher(e);

		dispatcher.Dispatch<MouseScrolledEvent>(EG_BIND_FN(CameraController::OnMouseScrolled));
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