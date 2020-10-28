#pragma once

#include "Eagle/Layer.h"

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
		float m_Time = 0.f;
	};
}