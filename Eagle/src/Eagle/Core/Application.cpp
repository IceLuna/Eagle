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

		m_WindowProps.Width = 1920;
		m_WindowProps.Height = 1080;
		m_WindowProps.Fullscreen = true;

		m_Window = Window::Create(m_WindowProps);
		m_Window->SetEventCallback(EG_BIND_FN(Application::OnEvent));

		Renderer::Init();

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

			if (!m_Minimized)
			{
				for (Layer* layer : m_LayerStack)
				{
					layer->OnUpdate(timestep);
				}
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
		dispatcher.Dispatch<WindowResizeEvent>(EG_BIND_FN(Application::OnWindowResize));

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

	void Application::SetShouldClose(bool close)
	{
		m_Running = !close;
	}

	bool Application::OnWindowClose(WindowCloseEvent& e)
	{
		m_Running = false;
		return true;
	}

	bool Application::OnWindowResize(WindowResizeEvent& e)
	{
		const uint32_t width = e.GetWidth();
		const uint32_t height = e.GetHeight();

		if (width == 0 || height == 0)
		{
			m_Minimized = true;
		}
		else
		{
			m_Minimized = false;
		}

		Renderer::WindowResized(width, height);

		return false;
	}
}