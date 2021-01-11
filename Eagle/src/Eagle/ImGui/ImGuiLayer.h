#pragma once

#include "Eagle/Core/Layer.h"

#include "Eagle/Events/ApplicationEvent.h"
#include "Eagle/Events/KeyEvent.h"
#include "Eagle/Events/MouseEvent.h"

namespace Eagle
{
	class ImGuiLayer : public Layer
	{
	public:
		ImGuiLayer(const std::string& name = "ImGuiLayer");
		~ImGuiLayer() = default;

		void OnAttach() override;
		void OnDetach() override;

		void OnEvent(Event& e) override;
		
		void Begin();
		void End();
	private:
		float m_Time = 0.f;
	};
}