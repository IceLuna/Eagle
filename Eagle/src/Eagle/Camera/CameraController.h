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

		float m_MoveSpeed = 10.f;
		float m_MouseRotationSpeed = 0.032f;

		uint64_t m_NumberOfFramesMoving = 0;
	};
}