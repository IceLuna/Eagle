#pragma once

#include "Eagle/Core/ScriptableEntity.h"

namespace Eagle
{
	class MouseScrolledEvent;

	class CameraController : public ScriptableEntity
	{
	protected:

		virtual void OnUpdate(Timestep ts) override;

		virtual void OnEvent(Event& e) override;

		bool OnMouseScrolled(MouseScrolledEvent& e);

	protected:
		float m_MouseX = 0.f;
		float m_MouseY = 0.f;

		float m_MoveSpeed = 1.75f; //0.225f;
		float m_MouseRotationSpeed = 2.f; //0.225f;
		float m_ScrollSpeed = 0.25f;
	};
}