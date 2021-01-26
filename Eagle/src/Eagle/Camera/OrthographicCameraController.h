#pragma once

#include "OrthographicCamera.h"
#include "Eagle/Core/Timestep.h"

#include "Eagle/Events/Event.h"
#include "Eagle/Events/ApplicationEvent.h"
#include "Eagle/Events/MouseEvent.h"

namespace Eagle
{
	class OrthographicCameraController : public CameraController
	{
	public:
		OrthographicCameraController(float aspectRatio);

		void OnUpdate(Timestep ts) override;
		void OnEvent(Event& e) override;

		void SetAspectRatio(float aspectRatio) override;

		const OrthographicCamera& GetCamera() const override { return m_Camera; }

	private:
		bool OnMouseScrolled(MouseScrolledEvent& e);
		bool OnWindowResizedEvent(WindowResizeEvent& e);

	private:
		float m_AspectRatio;
		float m_ZoomLevel = 1.f;
		OrthographicCamera m_Camera;

		float m_MouseX = 0.f;
		float m_MouseY = 0.f;

		float m_MouseMoveSpeed = 0.225f;
		float m_ScrollSpeed = 0.25f;
	};
}