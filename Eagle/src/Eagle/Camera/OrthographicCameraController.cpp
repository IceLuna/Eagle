#include "egpch.h"
#include "OrthographicCameraController.h"

#include "Eagle/Input/Input.h"

namespace Eagle
{
	OrthographicCameraController::OrthographicCameraController(float aspectRatio)
	:	m_AspectRatio(aspectRatio)
	,	m_Camera(-m_AspectRatio * m_ZoomLevel, m_AspectRatio * m_ZoomLevel, -m_ZoomLevel, m_ZoomLevel)
	{
		
	}

	void OrthographicCameraController::OnUpdate(Timestep ts)
	{
		if (Input::IsMouseButtonPressed(Mouse::ButtonRight))
		{
			float offsetX = m_MouseX - Input::GetMouseX();
			float offsetY = Input::GetMouseY() - m_MouseY;

			glm::vec3 cameraNewPos = m_Camera.GetPosition();
			cameraNewPos.x += offsetX * ts * m_MouseMoveSpeed;
			cameraNewPos.y += offsetY * ts * m_MouseMoveSpeed;

			m_Camera.SetPosition(cameraNewPos);
		}

		m_MouseX = Input::GetMouseX();
		m_MouseY = Input::GetMouseY();
	}

	void OrthographicCameraController::OnEvent(Event& e)
	{
		EventDispatcher dispatcher(e);

		dispatcher.Dispatch<MouseScrolledEvent>(EG_BIND_FN(OrthographicCameraController::OnMouseScrolled));
		dispatcher.Dispatch<WindowResizeEvent>(EG_BIND_FN(OrthographicCameraController::OnWindowResizedEvent));
	}

	void OrthographicCameraController::SetAspectRatio(float aspectRatio)
	{
		m_AspectRatio = aspectRatio;
		m_Camera.SetProjection(-m_AspectRatio * m_ZoomLevel, m_AspectRatio* m_ZoomLevel, -m_ZoomLevel, m_ZoomLevel);
	}

	bool OrthographicCameraController::OnMouseScrolled(MouseScrolledEvent& e)
	{
		m_ZoomLevel -= e.GetYOffset() * m_ScrollSpeed;
		m_ZoomLevel = std::max(m_ZoomLevel, 0.25f);

		m_Camera.SetProjection(-m_AspectRatio * m_ZoomLevel, m_AspectRatio * m_ZoomLevel, -m_ZoomLevel, m_ZoomLevel);
		return false;
	}

	bool OrthographicCameraController::OnWindowResizedEvent(WindowResizeEvent& e)
	{
		m_AspectRatio = (float)e.GetWidth() / (float)e.GetHeight();

		m_Camera.SetProjection(-m_AspectRatio * m_ZoomLevel, m_AspectRatio * m_ZoomLevel, -m_ZoomLevel, m_ZoomLevel);
		return false;
	}
}