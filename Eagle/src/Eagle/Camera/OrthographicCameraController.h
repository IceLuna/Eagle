#pragma once

#include "OrthographicCamera.h"
#include "Eagle/Core/Timestep.h"

#include "Eagle/Events/Event.h"
#include "Eagle/Events/ApplicationEvent.h"
#include "Eagle/Events/MouseEvent.h"

namespace Eagle
{
	class OrthographicCameraController
	{
	public:
		OrthographicCameraController(float aspectRatio, bool shouldRotate = false);

		void OnUpdate(Timestep ts);
		void OnEvent(Event& e);

		OrthographicCamera& GetCamera() { return m_Camera; }
		const OrthographicCamera& GetCamera() const { return m_Camera; }

		void SetScrollEnabled(bool enabled);

	private:
		bool OnMouseScrolled(MouseScrolledEvent& e);
		bool OnWindowResizedEvent(WindowResizeEvent& e);

	private:
		float m_AspectRatio;
		float m_ZoomLevel = 1.f;
		OrthographicCamera m_Camera;

		float m_MouseX = 0.f;
		float m_MouseY = 0.f;

		float m_MouseMoveSpeed = 0.25f;
		float m_ScrollSpeed = 0.25f;

		bool m_ShouldRotate;
		bool m_ScrollEnabled;

	};
}