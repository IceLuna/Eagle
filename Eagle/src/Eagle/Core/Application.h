#pragma once

#include "Eagle/Core/Core.h"
#include "Eagle/Core/Window.h"
#include "Eagle/Events/ApplicationEvent.h"
#include "Eagle/Core/Layer.h"
#include "Eagle/Core/LayerStack.h"
#include "Eagle/Core/SplashScreen.h"

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

	struct ApplicationProperties
	{
		WindowProperties WindowProps;
		bool bGame = false;

		// The splash screen is shown while the application starts up, until it renders its first frames. Relative paths are relative to the executable's folder.
		// A game build uses the splash screen it was built with (the project's settings, see `Project::LoadGameBuildSplashScreen()`), these are ignored then
		SplashScreenDesc SplashScreen;
		bool bSplashScreen = true;

		int argc = 0;
		char** argv = nullptr;
	};

	class Application
	{
	public:
		Application(const ApplicationProperties& props);
		Application(const Application&) = delete;
		virtual ~Application();

		void Run();
		virtual void OnEvent(Event& e);

		double GetTime() const { return m_Time; }
		Timestep GetTimestep() const { return m_Timestep; }

		// A project was either opened, or closed
		// @bOpened. If false, it means that a project was closed
		static void OnProjectChanged(bool bOpened);
		static const Path& GetCorePath() { return Get().m_CorePath; }
		static Application& Get() { return *s_Instance; }
		inline Window& GetWindow() { return *m_Window; }
		inline bool IsMinimized() const { return m_Minimized; }
		void SetShouldClose(bool close);

		// Returns true if it's a game build
		bool IsGame() const { return m_Game; }

		void ShowSplashScreen(const SplashScreenDesc& desc);
		const SplashScreenDesc& GetSplashScreenDesc() const { return m_SplashScreenDesc; }

		void PushLayer(const Ref<Layer>& layer);
		bool PopLayer(const Ref<Layer>& layer);

		void CallNextFrame(const std::function<void()>& func);
		void CallNextFrame(std::function<void()>&& func);

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

		const WindowProperties& GetWindowProps() const { return m_WindowProps; }

	protected:
		virtual bool OnWindowClose(WindowCloseEvent& e);
		virtual bool OnWindowResize(WindowResizeEvent& e);

		void ParseCmdArguments(int argc, char** argv);
		void ProcessCmdCommands();
		void ProcessNextFrameFuncs();

	private:
		// `bProcessEvents` is false for the frames that are run from within `Window::ProcessEvents()`, see `Window::SetModalLoopCallback()`
		void RunFrame(bool bProcessEvents);
		void UpdateTime();

		void ShowStartupSplashScreen(const ApplicationProperties& props);
		void BeginWindowReveal();
		void UpdateWindowReveal();

	protected:
		Ref<Window> m_Window;
		Ref<RendererContext> m_RendererContext;
		WindowProperties m_WindowProps;
		Ref<ImGuiLayer> m_ImGuiLayer;
		LayerStack m_LayerStack;

		std::unordered_map<std::thread::id, std::string_view> m_Threads;

		std::unordered_map<std::thread::id, std::unordered_set<std::string_view>> m_CPUTimingsInUse;
		std::unordered_map<std::thread::id, std::set<CPUTiming::Data>> m_CPUTimings;

		std::vector<std::function<void()>> m_NextFrameFuncs;
		bool bProcessingNextFrameFuncs = false; // Used to prevent pushes to `m_NextFrameFuncs` from inside `m_NextFrameFuncs`. Instead, they're called immediately

		enum class WindowRevealState
		{
			Revealed,
			WaitingForWork,
			WaitingForFrame,
			Revealing,
		};
		WindowRevealState m_RevealState = WindowRevealState::WaitingForWork;
		uint64_t m_RevealFrameNumber = 0;

		SplashScreenDesc m_SplashScreenDesc;
		bool m_bSplashScreenEnabled = true;
		Path m_CmdProjectPath; // `--project` argument

		Path m_CorePath;

		Timestep m_Timestep = 0.f;
		double m_Time = 0.f;
		double m_LastFrameTime = 0.0;

		bool m_Running = true;
		bool m_Minimized = false;
		bool m_Game = false;

		static Application* s_Instance;
	};

	//To be defined in CLIENT
	std::unique_ptr<Application> CreateApplication(int argc, char** argv);
}

