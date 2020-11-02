#pragma once

#include "Eagle/Layer.h"

#include "Eagle/Events/ApplicationEvent.h"
#include "Eagle/Events/KeyEvent.h"
#include "Eagle/Events/MouseEvent.h"

namespace Eagle
{
	class EAGLE_API ImGuiLayer : public Layer
	{
	public:
		ImGuiLayer(const std::string& name = "ImGuiLayer");
		~ImGuiLayer();

		void OnAttach() override;
		void OnDetach() override;
		void OnUpdate() override;
		void OnEvent(Event& e) override;

	private:
		bool OnMousePressedEvent(MouseButtonPressedEvent& e);
		bool OnMouseReleasedEvent(MouseButtonReleasedEvent& e);
		bool OnMouseMovedEvent(MouseMovedEvent& e);
		bool OnMouseScrolledEvent(MouseScrolledEvent& e);

		bool OnKeyPressedEvent(KeyPressedEvent& e);
		bool OnKeyReleasedEvent(KeyReleasedEvent& e);
		bool OnKeyTypedEvent(KeyTypedEvent& e);

		bool OnWindowResizedEvent(WindowResizeEvent& e);
	private:
		float m_Time = 0.f;
	};
}