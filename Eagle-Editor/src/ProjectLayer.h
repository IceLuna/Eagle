#pragma once

#include "Eagle.h"

namespace Eagle
{
	class ProjectLayer : public Layer
	{
	public:
		ProjectLayer() : Layer("Project Layer") {}

		void OnAttach() override;
		void OnDetach() override;
		void OnUpdate(Timestep ts) override;
		void OnEvent(Event& e) override;
		void OnImGuiRender() override;

	private:
		void OpenEditor();

	private:
		bool bWasVSync = false;
	};
}
