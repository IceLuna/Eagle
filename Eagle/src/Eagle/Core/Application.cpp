#include "egpch.h"
#include "Application.h"
#include "Log.h"
#include "Eagle/Core/Timestep.h"
#include "Eagle/Core/ThreadPool.h"
#include "Eagle/Debug/CPUTimings.h"
#include "Eagle/Renderer/RenderManager.h"
#include "Eagle/Script/ScriptEngine.h"
#include "Eagle/Physics/PhysicsEngine.h"
#include "Eagle/Audio/AudioEngine.h"

#include "Eagle/UI/Font.h"

#include "Platform/Vulkan/VulkanSwapchain.h"

#include <GLFW/glfw3.h>

namespace Eagle
{
	extern std::mutex g_ImGuiMutex;

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

		m_Threads.reserve(4);
		m_CPUTimingsInUse.reserve(4);
		m_CPUTimingsByName.reserve(4);
		m_Threads[std::this_thread::get_id()] = "Main Thread";

		RendererContext::SetAPI(RendererAPIType::Vulkan);
		m_RendererContext = RendererContext::Create();
		m_Window = Window::Create(m_WindowProps);
		m_Window->SetEventCallback(EG_BIND_FN(OnEvent));

		RenderManager::Init();
		m_ImGuiLayer = ImGuiLayer::Create();
		PushLayout(m_ImGuiLayer);

		PhysicsEngine::Init();
		AudioEngine::Init();
		ScriptEngine::Init("Eagle-Scripts.dll");
	}

	Application::~Application()
	{
		RenderManager::Finish();
		m_ImGuiLayer.reset();
		m_LayerStack.clear();
		m_Window.reset();
		ScriptEngine::Shutdown();
		AudioEngine::Shutdown();
		PhysicsEngine::Shutdown();
		FontLibrary::Clear();
		RenderManager::Shutdown();
	}

	static std::mutex s_Mutex;

	void Application::Run()
	{
		float m_LastFrameTime = (float)glfwGetTime();
		while (m_Running)
		{
#ifdef EG_CPU_TIMINGS
			{
				std::scoped_lock lock(s_Mutex);
				for (auto& it : m_CPUTimings)
					it.second.clear();

				for (auto& timingsByNameIt : m_CPUTimingsByName)
				{
					auto& timingsByName = timingsByNameIt.second;
					auto& timingsInUse = m_CPUTimingsInUse.find(timingsByNameIt.first)->second;
					auto& timings = m_CPUTimings[m_Threads[timingsByNameIt.first]];
					for (auto it = timingsByName.begin(); it != timingsByName.end();)
					{
						auto inUseIt = timingsInUse.find(it->first);
						if (inUseIt != timingsInUse.end())
						{
							timings.push_back({ it->first, it->second });
							++it;
						}
						else
						{
							it = timingsByName.erase(it);
						}
					}
					std::sort(timings.begin(), timings.end(), s_CustomCPUTimingsLess);
				}
				for (auto& it : m_CPUTimingsInUse)
					it.second.clear();
			}
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
			
			for (auto& func : m_NextFrameFuncs)
				func();
			m_NextFrameFuncs.clear();

			if (!m_Minimized)
			{
				RenderManager::BeginFrame();

				for (auto& layer : m_LayerStack)
					layer->OnUpdate(timestep);

				{
					std::scoped_lock lock(g_ImGuiMutex);
					m_ImGuiLayer->BeginFrame();
					for (auto& layer : m_LayerStack)
						layer->OnImGuiRender();
					m_ImGuiLayer->EndFrame();
					m_ImGuiLayer->UpdatePlatform();
				}

				RenderManager::EndFrame();
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

	void Application::AddThread(const ThreadPool& threadPool)
	{
		const auto& threads = threadPool->get_threads();
		const uint32_t threadsCount = threadPool->get_thread_count();
		for (uint32_t i = 0; i < threadsCount; ++i)
			m_Threads.emplace(threads[i].get_id(), threadPool.GetName());
	}

	void Application::RemoveThread(const ThreadPool& threadPool)
	{
		const auto& threads = threadPool->get_threads();
		const uint32_t threadsCount = threadPool->get_thread_count();
		for (uint32_t i = 0; i < threadsCount; ++i)
		{
			auto it = m_Threads.find(threads[i].get_id());
			if (it != m_Threads.end())
				m_Threads.erase(it);
		}
	}

	void Application::AddCPUTiming(std::string_view name, float timing)
	{
		std::scoped_lock lock(s_Mutex);

		auto id = std::this_thread::get_id();
		m_CPUTimingsInUse[id].emplace(name);
		auto& timingsByName = m_CPUTimingsByName[id];
		timingsByName[name] = timing;
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

		return false;
	}
}