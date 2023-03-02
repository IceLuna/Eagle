#include "egpch.h"
#include "Application.h"
#include "Log.h"
#include "Eagle/Core/Timestep.h"
#include "Eagle/Debug/CPUTimings.h"
#include "Eagle/Renderer/Renderer.h"
#include "Eagle/Renderer/Renderer2D.h"
#include "Eagle/Script/ScriptEngine.h"
#include "Eagle/Physics/PhysicsEngine.h"
#include "Eagle/Audio/AudioEngine.h"

#include "Platform/Vulkan/VulkanSwapchain.h"

#include <GLFW/glfw3.h>

namespace Eagle
{
#ifdef EG_CPU_TIMINGS
	struct {
		bool operator()(const CPUTimingData& a, const CPUTimingData& b) const
		{
			return a.Timing > b.Timing;
		}
	} s_CustomCPUTimingsLess;
#endif

	Application* Application::s_Instance = nullptr;

	Application::Application(const std::string& name)
		: m_WindowProps(name, 1600, 900, true, false)
	{
		EG_CORE_ASSERT(!s_Instance, "Application already exists!");
		s_Instance = this;

		RendererContext::SetAPI(RendererAPIType::Vulkan);
		m_RendererContext = RendererContext::Create();
		m_Window = Window::Create(m_WindowProps);
		m_Window->SetEventCallback(EG_BIND_FN(OnEvent));

		Renderer::Init();
		m_ImGuiLayer = ImGuiLayer::Create();
		PushLayout(m_ImGuiLayer);

		PhysicsEngine::Init();
		AudioEngine::Init();
		ScriptEngine::Init("Eagle-Scripts.dll");
	}

	Application::~Application()
	{
		m_ImGuiLayer.reset();
		m_LayerStack.clear();
		m_Window.reset();
		ScriptEngine::Shutdown();
		AudioEngine::Shutdown();
		PhysicsEngine::Shutdown();

		Renderer::Shutdown();
	}

	void Application::Run()
	{
		float m_LastFrameTime = (float)glfwGetTime();
		while (m_Running)
		{
#ifdef EG_CPU_TIMINGS
			m_CPUTimings.clear(); //m_CPUTimingsInUse
			for (auto it = m_CPUTimingsByName.begin(); it != m_CPUTimingsByName.end();)
			{
				auto inUseIt = m_CPUTimingsInUse.find(it->first);
				if (inUseIt != m_CPUTimingsInUse.end())
				{
					m_CPUTimings.push_back({ it->first, it->second });
					++it;
				}
				else
				{
					it = m_CPUTimingsByName.erase(it);
				}
			}
			std::sort(m_CPUTimings.begin(), m_CPUTimings.end(), s_CustomCPUTimingsLess);
			m_CPUTimingsInUse.clear();
#endif
			EG_CPU_TIMING_SCOPED("Whole frame");
			const float currentFrameTime = (float)glfwGetTime();
			Timestep timestep = currentFrameTime - m_LastFrameTime;
			m_LastFrameTime = currentFrameTime;

			#ifndef EG_DIST
			//If timestep is too big that probably means that we were debugging. In that case, reset timestep to 60fps value
			if (timestep > 1.f)
				timestep = 0.016f;
			#endif

			if (!m_Minimized)
			{
				Renderer::BeginFrame();
				m_ImGuiLayer->NextFrame();

				for (auto& layer : m_LayerStack)
					layer->OnUpdate(timestep);

				for (auto& layer : m_LayerStack)
					layer->OnImGuiRender();

				Renderer::EndFrame();
			}
			AudioEngine::Update(timestep);
			m_Window->ProcessEvents();
		}
	}

	void Application::OnEvent(Event& e)
	{
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<WindowCloseEvent>(EG_BIND_FN(Application::OnWindowClose));
		dispatcher.Dispatch<WindowResizeEvent>(EG_BIND_FN(Application::OnWindowResize));

		for (auto it = m_LayerStack.rbegin(); it != m_LayerStack.rend(); ++it)
		{
			if (e.Handled)
				break;

			(*it)->OnEvent(e);
		}
	}

	void Application::SetShouldClose(bool close)
	{
		m_Running = !close;
	}

	void Application::PushLayer(const Ref<Layer>& layer)
	{
		m_LayerStack.PushLayer(layer);
		layer->OnAttach();
	}

	bool Application::PopLayer(const Ref<Layer>& layer)
	{
		if (m_LayerStack.PopLayer(layer))
		{
			layer->OnDetach();
			return true;
		}
		return false;
	}

	void Application::PushLayout(const Ref<Layer>& layer)
	{
		m_LayerStack.PushLayout(layer);
		layer->OnAttach();
	}

	bool Application::PopLayout(const Ref<Layer>& layer)
	{
		if (m_LayerStack.PopLayout(layer))
		{
			layer->OnDetach();
			return true;
		}
		return false;
	}

	void Application::AddCPUTiming(std::string_view name, float timing)
	{
		m_CPUTimingsInUse.emplace(name);
		m_CPUTimingsByName[name] = timing;
	}

	bool Application::OnWindowClose(WindowCloseEvent& e)
	{
		SetShouldClose(true);
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