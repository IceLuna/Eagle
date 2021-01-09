#include "SandBoxApp.h"

#include <Eagle/Core/EntryPoint.h>

#include "Sandbox2D.h"

Sandbox::Sandbox()
{
	PushLayer(new Sandbox2D());
	m_Window->SetVSync(true);
}


Eagle::Application* Eagle::CreateApplication()
{
	return new Sandbox;
}