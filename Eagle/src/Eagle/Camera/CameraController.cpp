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

				if (Input::IsMouseButtonPressed(Mouse::ButtonRight))
				{
					float offsetX = m_MouseX - Input::GetMouseX();
					float offsetY = Input::GetMouseY() - m_MouseY;

					Translation.x += offsetX * ts * m_MouseMoveSpeed;
					Translation.y += offsetY * ts * m_MouseMoveSpeed;
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