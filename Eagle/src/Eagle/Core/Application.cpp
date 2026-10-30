#include "egpch.h"
#include "Application.h"
#include "Log.h"
#include "Project.h"
#include "Eagle/Core/Timestep.h"
#include "Eagle/Core/ThreadPool.h"
#include "Eagle/Core/AsyncTask.h"
#include "Eagle/Core/AsyncSceneLoader.h"
#include "Eagle/Debug/CPUTimings.h"
#include "Eagle/Renderer/RenderManager.h"
#include "Eagle/Script/ScriptEngine.h"
#include "Eagle/Physics/PhysicsEngine.h"
#include "Eagle/Audio/AudioEngine.h"
#include "Eagle/Asset/AssetManager.h"
#include "Eagle/Renderer/VidWrappers/Shader.h"
#include "Eagle/Renderer/TextureCompressor.h"
#include "Eagle/Utils/ThumbnailCache.h"
#include "Eagle/Utils/PlatformUtils.h"
#include "Eagle/Utils/Compressor.h"
#include "Eagle/Utils/SerializerUtils.h"

#include "Platform/Vulkan/VulkanSwapchain.h"

#include <GLFW/glfw3.h>
#include <argparse/argparse.hpp>

namespace Eagle
{
	static void SetSplashStatus(std::string_view status)
	{
		SplashScreen::SetStatus(status);
		SplashScreen::SetProgress(-1.f);
	}

	static std::mutex s_TimingsMutex;

#ifdef EG_CPU_TIMINGS
	struct {
		bool operator()(const CPUTiming::Data& a, const CPUTiming::Data& b) const
		{
			return a.Timing > b.Timing;
		}
	} s_CustomCPUTimingsLess;
#endif

	Application* Application::s_Instance = nullptr;

	Application::Application(const ApplicationProperties& props)
		: m_WindowProps(props.WindowProps)
	{
		EG_CORE_ASSERT(!s_Instance, "Application already exists!");
		s_Instance = this;
		MainThread::Init();

		m_Game = props.bGame;
		m_CorePath = props.argv[0];
		m_CorePath = m_CorePath.parent_path();
		std::filesystem::current_path(m_CorePath);

		m_Threads.reserve(4);
		m_CPUTimingsInUse.reserve(4);
		m_Threads[std::this_thread::get_id()] = "Main Thread";

		ParseCmdArguments(props.argc, props.argv);
		ShowStartupSplashScreen(props);

		SetSplashStatus("Initializing the renderer");
		RendererContext::SetAPI(RendererAPIType::Vulkan);
		m_RendererContext = RendererContext::Create();
		m_Window = Window::Create(m_WindowProps);
		m_Window->SetEventCallback(EG_BIND_FN(OnEvent));

		SetSplashStatus("Loading shaders");
		if (m_Game)
		{
			const Path shaderPackPath = Project::GetShaderPackRelativePath();
			ScopedDataBuffer compressedData = FileSystem::Read(shaderPackPath);
			if (!compressedData)
			{
				EG_CORE_CRITICAL("Failed to load the shader pack: {}", shaderPackPath);
				exit(-1);
			}

			ScopedDataBuffer data = Compressor::Decompress(compressedData);

			YAML::Node baseNode;
			Utils::ReadYAML(data, &baseNode);
			ShaderManager::InitGame(baseNode["Shaders"]);
		}
		else
		{
			ShaderManager::Init();
		}
		SetSplashStatus("Initializing the renderer");
		RenderManager::Init();
		TextureCompressor::Init();

		SetSplashStatus("Initializing physics");
		PhysicsEngine::Init();
		SetSplashStatus("Initializing audio");
		AudioEngine::Init();
		SetSplashStatus("Initializing scripting");
		ScriptEngine::Init(m_CorePath / "Eagle-Scripts.dll");

		if (m_Game)
		{
			SetSplashStatus("Loading game data");
			const Path dataPath = "Data/";
			for (auto& dirEntry : std::filesystem::recursive_directory_iterator(dataPath))
			{
				if (dirEntry.is_directory())
					continue;

				Path path = dirEntry.path();
				if (!Utils::HasExtension(path, AssetManager::GetAssetPackExtension()))
					continue;

				// It also initializes AssetManager & ShaderManager
				Project::OpenGameBuild(path);
				m_WindowProps.Title = Project::GetProjectInfo().Name;
				break;
			}
		}

		m_ImGuiLayer = ImGuiLayer::Create();
		PushLayer(m_ImGuiLayer);

		ProcessCmdCommands();
		ProcessNextFrameFuncs();
	}

	void Application::ShowStartupSplashScreen(const ApplicationProperties& props)
	{
		m_bSplashScreenEnabled = props.bSplashScreen;
		m_SplashScreenDesc = props.SplashScreen;
		for (Path* path : { &m_SplashScreenDesc.ImagePath, &m_SplashScreenDesc.IconPath, &m_WindowProps.IconPath })
		{
			if (!path->empty() && path->is_relative())
				*path = m_CorePath / *path;
		}

		SplashScreenDesc desc = m_SplashScreenDesc;
		if (m_Game)
		{
			SplashScreenDesc gameSplash;
			if (Project::LoadGameBuildSplashScreen(&gameSplash))
			{
				desc = gameSplash;
				if (m_WindowProps.IconPath.empty())
					m_WindowProps.IconPath = gameSplash.IconPath;
			}
			else if (desc.Title.empty() && props.argv && props.argv[0])
			{
				// There are no settings for some reason.
				// And since the project isn't loaded yet, use the executable name
				desc.Title = Utils::AsString(Path(props.argv[0]).stem());
			}
		}
		else if (!m_CmdProjectPath.empty())
		{
			desc.Subtitle = Project::ReadProjectName(m_CmdProjectPath);
		}

		if (m_bSplashScreenEnabled)
			SplashScreen::Show(desc);
	}

	void Application::ShowSplashScreen(const SplashScreenDesc& desc)
	{
		if (m_bSplashScreenEnabled)
		{
			SplashScreenDesc splashDesc = desc;
			if (!splashDesc.NearWindow && m_Window->IsVisible())
				splashDesc.NearWindow = m_Window->GetNativeWindow();

			// Shown first, so that the focus goes to it from the window
			SplashScreen::Show(splashDesc);
			SetSplashStatus({});
		}

		m_Window->Hide();
		m_RevealState = WindowRevealState::WaitingForWork;
	}

	void Application::BeginWindowReveal()
	{
		// Showing the window applies its position & fullscreen state, which can resize it.
		// So it's done before the frame is built, so that the frame is rendered at the final size
		m_Window->Show(true);
		m_RevealFrameNumber = RenderManager::GetFrameNumber_CPU() + 1; // The frame that's about to be built
		m_RevealState = WindowRevealState::Revealing;
	}

	void Application::UpdateWindowReveal()
	{
		if (m_RevealState != WindowRevealState::Revealing)
			return;

		// The first frame after the window was shown might be rendered for its old size, the next one is presented at the final size
		constexpr uint64_t framesToPresent = 2;

		// To avoid keeping the window invisible forever, just in case the frames aren't presented (for example, the swapchain is invalid)
		constexpr uint64_t maxFramesToWait = 8;

		const bool bPresented = RenderManager::GetLastPresentedFrameNumber() >= m_RevealFrameNumber + framesToPresent - 1;
		const bool bGiveUp = RenderManager::GetFrameNumber_CPU() >= m_RevealFrameNumber + maxFramesToWait;
		if (!bPresented && !bGiveUp && !m_Minimized)
			return;

		m_Window->Uncloak();
		SplashScreen::Hide();
		m_RevealState = WindowRevealState::Revealed;
	}

	Application::~Application()
	{
		SplashScreen::Hide();
		AsyncSceneLoader::Shutdown();
		AsyncTaskManager::Shutdown();
		ProcessNextFrameFuncs();

		if (Project::IsOpened())
			Project::Close();
		ThumbnailCache::Release();
		RenderManager::Finish();
		AssetManager::Reset();
		m_ImGuiLayer.reset();
		m_LayerStack.clear();
		m_Window.reset();
		ScriptEngine::Shutdown();
		AudioEngine::Shutdown();
		PhysicsEngine::Shutdown();
		TextureCompressor::Shutdown();
		RenderManager::Shutdown();
		s_Instance = nullptr;
	}

	void Application::ParseCmdArguments(int argc, char** argv)
	{
		argparse::ArgumentParser program("Eagle Engine");
		program.add_argument("--project").help("Path to a .egproj file");

		try
		{
			program.parse_args(argc, argv);
		}
		catch (const std::exception& err)
		{
			EG_CORE_ERROR("Error occured while parsing CMD arguments: {}", err.what());
			EG_CORE_ERROR("\t{}", program.help().str());
			return;
		}

		if (!m_Game && program.is_used("--project"))
		{
			Path projectPath = program.get("--project");
			if (std::filesystem::exists(projectPath))
				m_CmdProjectPath = std::filesystem::absolute(projectPath); // The current path changes later
			else
				EG_CORE_ERROR("The project passed with `--project` doesn't exist: {}", projectPath);
		}
	}

	void Application::ProcessCmdCommands()
	{
		if (!m_CmdProjectPath.empty())
			Project::Open(m_CmdProjectPath);
	}

	void Application::ProcessNextFrameFuncs()
	{
		bProcessingNextFrameFuncs = true;
		for (auto& func : m_NextFrameFuncs)
			func();
		m_NextFrameFuncs.clear();
		bProcessingNextFrameFuncs = false;
	}

	void Application::OnProjectChanged(bool bOpened)
	{
		Get().CallNextFrame([bOpened, bGame = Get().m_Game, corePath = Get().m_CorePath, imguiLayer = Get().m_ImGuiLayer]()
		{
			AsyncSceneLoader::CancelAll(true);

			ThumbnailCache::Release();
			RenderManager::Reset();
			ScriptEngine::Reset();
			Log::ClearLogHistory();
			AssetManager::Reset();

			if (bOpened)
			{
				std::filesystem::current_path(Project::GetProjectPath());
				AssetManager::Init();
				ThumbnailCache::Init();
				if (!imguiLayer->SetIniFilepath(Project::GetConfigPath() / "imgui.ini"))
				{
					if (!bGame)
					{
						EG_CORE_WARN("Copying the default imgui layout to the project and trying again...");
						std::filesystem::copy(Application::GetCorePath() / "default_imgui_layout.ini", Project::GetConfigPath() / "imgui.ini");
						if (imguiLayer->SetIniFilepath(Project::GetConfigPath() / "imgui.ini"))
						{
							EG_CORE_INFO("Successfully restored ImGui ini file");
						}
					}
				}
				Project::OnProjectOpenProcessed();
			}
			else
			{
				std::filesystem::current_path(corePath);
				imguiLayer->SetIniFilepath(corePath / "imgui.ini");
			}
		});
	}

	void Application::Run()
	{
		m_LastFrameTime = glfwGetTime();

		// While the user moves or resizes a window, the OS doesn't let `ProcessEvents()` return until the mouse is released.
		// So frames are run from within it, otherwise the content of the windows would freeze (and resized windows would show stretched or cut off content)
		m_Window->SetModalLoopCallback([this]()
		{
			if (m_Running)
				RunFrame(false);
		});

		while (m_Running)
			RunFrame(true);

		m_Window->SetModalLoopCallback({});
	}

	void Application::RunFrame(bool bProcessEvents)
	{
#ifdef EG_CPU_TIMINGS
		// A frame run from within `ProcessEvents()` is nested in the outer one, so the outer frame's timings are still in use
		if (bProcessEvents)
		{
			std::scoped_lock lock(s_TimingsMutex);

			for (auto& [threadID, timings] : m_CPUTimings)
			{
				auto& timingsInUse = m_CPUTimingsInUse.find(threadID)->second;

				for (auto it = timings.begin(); it != timings.end();)
				{
					std::string_view name = (*it).Name;
					auto inUseIt = timingsInUse.find(name);
					if (inUseIt != timingsInUse.end())
					{
						++it;
					}
					else
					{
						it = timings.erase(it);
					}
				}
			}
			for (auto& it : m_CPUTimingsInUse)
				it.second.clear();
		}
#endif
		EG_CPU_TIMING_SCOPED("Whole frame");
		UpdateTime();

		ProcessNextFrameFuncs();

		MainThread::ExecutePending();

		// The work the hidden window was waiting for is done. The next frame is the first one to show
		// And it's done because we went past `ProcessNextFrameFuncs`, which loads the project because `Application::OnProjectChanged()` pushes the work there
		if (m_RevealState == WindowRevealState::WaitingForWork)
		{
			m_RevealState = WindowRevealState::WaitingForFrame;
			SetSplashStatus("Finishing up");
		}

		// The events are processed right before the frame is built (after `RenderManager::BeginFrame` waited for the renderer),
		// so that the frame reacts to the latest input
		const bool bBeganFrame = !m_Minimized;
		if (bBeganFrame)
			RenderManager::BeginFrame();

		if (bProcessEvents)
		{
			const uint64_t frameNumber = RenderManager::GetFrameNumber_CPU();
			m_Window->ProcessEvents();

			// Frames were run from within `ProcessEvents()` (the user moved or resized a window), and they submitted the frame `BeginFrame` was waiting for.
			// So this frame waits for the renderer again, as if it just started
			if (RenderManager::GetFrameNumber_CPU() != frameNumber)
			{
				UpdateTime();
				if (bBeganFrame && !m_Minimized)
					RenderManager::BeginFrame();
			}
		}

		// The events might have minimized the window. If they restored it, the frame starts next iteration, since `BeginFrame` wasn't called
		if (bBeganFrame && !m_Minimized)
		{
			if (m_RevealState == WindowRevealState::WaitingForFrame)
				BeginWindowReveal();

			ThumbnailCache::NextFrame();

			for (auto& layer : m_LayerStack)
				layer->OnUpdate(m_Timestep);

			// ImGui
			{
				m_ImGuiLayer->BeginFrame();
				for (auto& layer : m_LayerStack)
					layer->OnImGuiRender();
				m_ImGuiLayer->EndFrame();
				m_ImGuiLayer->UpdatePlatform();
			}

			RenderManager::EndFrame();
		}
		AudioEngine::Update(m_Timestep);
		UpdateWindowReveal();
	}

	void Application::UpdateTime()
	{
		m_Time = glfwGetTime();
		m_Timestep = float(m_Time - m_LastFrameTime);
		m_LastFrameTime = m_Time;

#ifndef EG_RELEASE
		//If timestep is too big that probably means that we were debugging. In that case, reset timestep to 60fps value
		if (m_Timestep > 1.f)
			m_Timestep = 0.016f;
#endif
	}

	void Application::OnEvent(Event& e)
	{
		Event::Dispatch<WindowResizeEvent>(e, EG_BIND_FN(Application::OnWindowResize));

		for (auto it = m_LayerStack.rbegin(); it != m_LayerStack.rend(); ++it)
		{
			if (e.Handled)
				break;

			(*it)->OnEvent(e);
		}

		// Let layers decide if we should quit. If layers didn't handle that event, we simply close the engine
		if (!e.Handled)
			Event::Dispatch<WindowCloseEvent>(e, EG_BIND_FN(Application::OnWindowClose));
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

	void Application::CallNextFrame(const std::function<void()>& func)
	{
		if (bProcessingNextFrameFuncs)
			func();
		else
			m_NextFrameFuncs.push_back(func);
	}

	void Application::CallNextFrame(std::function<void()>&& func)
	{
		if (bProcessingNextFrameFuncs)
			func();
		else
			m_NextFrameFuncs.emplace_back(std::move(func));
	}

	void Application::AddThread(const ThreadPool& threadPool)
	{
		const auto& threads = threadPool->get_threads();
		const uint32_t threadsCount = (uint32_t)threadPool->get_thread_count();
		for (uint32_t i = 0; i < threadsCount; ++i)
			m_Threads.emplace(threads[i].get_id(), threadPool.GetName());
	}

	void Application::RemoveThread(const ThreadPool& threadPool)
	{
		const auto& threads = threadPool->get_threads();
		const uint32_t threadsCount = (uint32_t)threadPool->get_thread_count();
		for (uint32_t i = 0; i < threadsCount; ++i)
		{
			auto it = m_Threads.find(threads[i].get_id());
			if (it != m_Threads.end())
				m_Threads.erase(it);
		}
	}

	void Application::AddCPUTiming(const CPUTiming* timing)
	{
		std::scoped_lock lock(s_TimingsMutex);

		auto id = std::this_thread::get_id();
		auto name = timing->GetName();
		m_CPUTimingsInUse[id].emplace(name);

		const auto& timingData = timing->GetData();
		auto& timingsByName = m_CPUTimings[id];

		auto it = timingsByName.find(timingData);
		if (it != timingsByName.end()) // Update timings data
		{
			const float oldTiming = (*it).Timing;
			const float newTiming = timingData.Timing;
			const uint64_t samples = (*it).SamplesCount + 1;

			if (samples > 500)
			{
				// Reset
				(*it).Timing = timingData.Timing;
				(*it).Children = timingData.Children;
				(*it).SamplesCount = 1;
			}
			else
			{
				// Running average
				(*it).Timing = oldTiming + ((newTiming - oldTiming) / (float)samples);
				(*it).Children = timingData.Children;
				(*it).SamplesCount++;
			}
		}
		else
		{
			timingsByName.emplace(timingData);
		}
	}

	static CPUTiming::Data ProcessCPUTiming(const CPUTiming::Data& timing)
	{
		CPUTiming::Data data;
		data.Name = timing.Name;
		data.Timing = timing.Timing;

		const auto& children = timing.Children;
		for (auto& child : children)
		{
			auto& childData = data.Children.emplace_back();
			childData.Name = child.Name;
			childData.Timing = child.Timing;
		}

		const size_t childsCount = children.size();
		for (size_t i = 0; i < childsCount; ++i)
		{
			auto& child = children[i];
			for (auto& childsChild : child.Children)
				data.Children[i].Children.push_back(ProcessCPUTiming(childsChild));
			std::sort(data.Children[i].Children.begin(), data.Children[i].Children.end(), s_CustomCPUTimingsLess);
		}
		std::sort(data.Children.begin(), data.Children.end(), s_CustomCPUTimingsLess);

		return data;
	}

	CPUTimingsContainer Application::GetCPUTimings() const
	{
		CPUTimingsContainer result;
		{
			std::scoped_lock lock(s_TimingsMutex);
			for (auto& [threadID, timings] : m_CPUTimings)
			{
				for (auto& timing : timings)
					result[threadID].push_back(ProcessCPUTiming(timing));
			}
		}
		return result;
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

		EG_CORE_TRACE("Window was resized: {}x{}", width, height);

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