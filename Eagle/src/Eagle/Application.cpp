#include "egpch.h"

#include "Application.h"

#include "Log.h"
#include "Eagle/Core/Timestep.h"

#include "Eagle/Renderer/Renderer.h"

#include <GLFW/glfw3.h>

namespace Eagle
{
	Application* Application::s_Instance = nullptr;

	Application::Application()
	{
		EG_CORE_ASSERT(!s_Instance, "Application already exists!");
		s_Instance = this;

		m_Window = Scope<Window>(Window::Create());
		m_Window->SetEventCallback(EG_BIND_FN(Application::OnEvent));

		m_ImGuiLayer = new ImGuiLayer();
		PushLayout(m_ImGuiLayer);
	}

	Application::~Application()
	{
	}

	void Application::Run()
	{
		float m_LastFrameTime = (float)glfwGetTime();

		while (m_Running)
		{
			const float currentFrameTime = (float)glfwGetTime();
			const Timestep timestep = currentFrameTime - m_LastFrameTime;
			m_LastFrameTime = currentFrameTime;

			for (Layer* layer : m_LayerStack)
			{
				layer->OnUpdate(timestep);
			}

			m_ImGuiLayer->Begin();
			for (Layer* layer : m_LayerStack)
			{
				layer->OnImGuiRender();
			}
			m_ImGuiLayer->End();

			m_Window->OnUpdate();
		}
	}
	void Application::OnEvent(Event& e)
	{
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<WindowCloseEvent>(EG_BIND_FN(Application::OnWindowClose));

		for (auto it = m_LayerStack.rbegin(); it != m_LayerStack.rend(); ++it)
		{
			(*it)->OnEvent(e);
			if (e.Handled)
			{
				break;
			}
		}
	}
	void Application::PushLayer(Layer* layer)
	{
		m_LayerStack.PushLayer(layer);
	}
	void Application::PopLayer(Layer* layer)
	{
		m_LayerStack.PopLayer(layer);
	}
	void Application::PushLayout(Layer* layer)
	{
		m_LayerStack.PushLayout(layer);
	}
	void Application::PopLayout(Layer* layer)
	{
		m_LayerStack.PopLayout(layer);
	}
	bool Application::OnWindowClose(WindowCloseEvent& e)
	{
		m_Running = false;
		return true;
	}
}