#pragma once

#include "Eagle/Core/Core.h"
#include "Eagle/Core/Window.h"
#include "Eagle/Events/ApplicationEvent.h"
#include "Eagle/Core/Layer.h"
#include "Eagle/Core/LayerStack.h"

#include "Eagle/ImGui/ImGuiLayer.h"
#include "Eagle/Debug/CPUTimings.h"

#include <set>
#include <unordered_map>
#include <unordered_set>

int main(int argc, char** argv);

namespace Eagle
{
	class RendererContext;
	class ThreadPool;

	// Key - Thread id; value - timings
	using CPUTimingsContainer = std::unordered_map<std::thread::id, std::vector<CPUTiming::Data>>;

	class Application
	{
	public:
		Application(const std::string& name = "Eagle Application");
		Application(const Application&) = delete;
		virtual ~Application();

		static inline Application& Get() { return *s_Instance; }
		inline Window& GetWindow() { return *m_Window; }
		inline bool IsMinimized() const { return m_Minimized; }
		void SetShouldClose(bool close);

		void PushLayer(const Ref<Layer>& layer);
		bool PopLayer(const Ref<Layer>& layer);
		void PushLayout(const Ref<Layer>& layer);
		bool PopLayout(const Ref<Layer>& layer);

		void CallNextFrame(const std::function<void()>& func) { m_NextFrameFuncs.push_back(func); }

		Ref<ImGuiLayer>& GetImGuiLayer() { return m_ImGuiLayer; }

		Ref<RendererContext>& GetRenderContext() { return m_RendererContext; }
		const Ref<RendererContext>& GetRenderContext() const { return m_RendererContext; }

		void AddThread(const ThreadPool& threadPool);
		void RemoveThread(const ThreadPool& threadPool);
		std::string_view GetThreadName(std::thread::id threadID)
		{
			auto it = m_Threads.find(threadID);
			if (it != m_Threads.end())
				return it->second;
			return "";
		}

		void AddCPUTiming(const CPUTiming* timing);
		CPUTimingsContainer GetCPUTimings() const;

	protected:
		virtual bool OnWindowClose(WindowCloseEvent& e);
		virtual bool OnWindowResize(WindowResizeEvent& e);
		virtual void OnEvent(Event& e);

		void Run();

		friend int ::main(int argc, char** argv);

	protected:
		Ref<Window> m_Window;
		Ref<RendererContext> m_RendererContext;
		WindowProps m_WindowProps;
		Ref<ImGuiLayer> m_ImGuiLayer;
		LayerStack m_LayerStack;

		std::unordered_map<std::thread::id, std::string_view> m_Threads;

		std::unordered_map<std::thread::id, std::unordered_set<std::string_view>> m_CPUTimingsInUse;
		std::unordered_map<std::thread::id, std::set<CPUTiming::Data>> m_CPUTimings;

		std::vector<std::function<void()>> m_NextFrameFuncs;

		bool m_Running = true;
		bool m_Minimized = false;

		static Application* s_Instance;
	};

	//To be defined in CLIENT
	Application* CreateApplication();
}

