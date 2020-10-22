#include "Application.h"

#include "Events/MouseEvent.h"
#include "Log.h"

namespace Eagle
{
	Application::Application()
	{
	}
	Application::~Application()
	{
	}
	void Application::Run()
	{
		MouseMovedEvent event(15.f, 20.f);

		if (event.IsInCategory(Eagle::EventCategory::EventCategoryInput))
		{
			EG_TRACE(event);
		}
		else
		{
			EG_CRITICAL("No!");
		}

		while(true);
	}
}