#include "egpch.h"

#include "Application.h"

#include "Events/MouseEvent.h"
#include "Log.h"

namespace Eagle
{
	Application::Application()
	{
		m_Window = std::unique_ptr<Window>(Window::Create());
	}

	Application::~Application()
	{
	}

	void Application::Run()
	{
		while (m_Running)
		{
			m_Window->OnUpdate();
		}
	}
}