#pragma once

#include "Eagle.h"

namespace Eagle
{
	class EagleEditor : public Application
	{
	public:
		EagleEditor();

	protected:
		virtual void OnEvent(Event& e) override;
		virtual bool OnWindowResize(WindowResizeEvent& e) override;
	};
}
