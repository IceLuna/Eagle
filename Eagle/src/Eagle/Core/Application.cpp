#include "egpch.h"
#include "Application.h"
#include "Log.h"
#include "Project.h"
#include "Eagle/Core/Timestep.h"
#include "Eagle/Core/ThreadPool.h"
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

		m_Game = props.bGame;
		m_CorePath = props.argv[0];
		m_CorePath = m_CorePath.parent_path();
		std::filesystem::current_path(m_CorePath);

		m_Threads.reserve(4);
		m_CPUTimingsInUse.reserve(4);
		m_Threads[std::this_thread::get_id()] = "Main Thread";

		RendererContext::SetAPI(RendererAPIType::Vulkan);
		m_RendererContext = RendererContext::Create();
		m_Window = Window::Create(m_WindowProps);
		m_Window->SetEventCallback(EG_BIND_FN(OnEvent));

		if (m_Game)
		{
			const Path shaderPackPath = "Data/ShaderPack.egspack";
			ScopedDataBuffer compressedData = FileSystem::Read(shaderPackPath);
			if (!compressedData)
			{
				EG_CORE_CRITICAL("Failed to load the shader pack: {}", shaderPackPath);
				exit(-1);
			}

			const size_t origSize = compressedData.Read<size_t>();
			DataBuffer compressedDataWithOffset((uint8_t*)compressedData.Data() + sizeof(size_t), compressedData.Size() - sizeof(size_t));

			ScopedDataBuffer data = Compressor::Decompress(compressedDataWithOffset, origSize);

			YAML::Node baseNode;
			Utils::ReadYAML(data, &baseNode);
			ShaderManager::InitGame(baseNode["Shaders"]);
		}
		else
		{
			ShaderManager::Init();
		}
		RenderManager::Init();
		TextureCompressor::Init();

		PhysicsEngine::Init();
		AudioEngine::Init();
		ScriptEngine::Init(m_CorePath / "Eagle-Scripts.dll");

		if (m_Game)
		{
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

		ProcessCmdCommands(props.argc, props.argv);
		ProcessNextFrameFuncs();
	}

	Application::~Application()
	{
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

	void Application::ProcessCmdCommands(int argc, char** argv)
	{
		argparse::ArgumentParser program("Eagle Engine");
		program.add_argument("--project").help("Path to a .egproj file");
		bool bParsedCmd = true;

		try
		{
			program.parse_args(argc, argv);
		}
		catch (const std::exception& err)
		{
			EG_CORE_ERROR("Error occured while parsing CMD arguments: {}", err.what());
			EG_CORE_ERROR("\t{}", program.help().str());
			bParsedCmd = false;
		}

		if (bParsedCmd)
		{
			if (program.is_used("--project"))
			{
				Path projectPath = program.get("--project");
				if (std::filesystem::exists(projectPath))
					Project::Open(projectPath);
			}
		}
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
		Get().CallNextFrame([bOpened, corePath = Get().m_CorePath]()
		{
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
				Project::OnProjectOpenProcessed();
			}
			else
			{
				std::filesystem::current_path(corePath);
			}
		});
	}

	void Application::Run()
	{
		double lastFrameTime = glfwGetTime();
		while (m_Running)
		{
#ifdef EG_CPU_TIMINGS
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
			m_Time = glfwGetTime();
			const double currentFrameTime = m_Time;
			m_Timestep = float(currentFrameTime - lastFrameTime);
			lastFrameTime = currentFrameTime;

#ifndef EG_RELEASE
			//If timestep is too big that probably means that we were debugging. In that case, reset timestep to 60fps value
			if (m_Timestep > 1.f)
				m_Timestep = 0.016f;
#endif

			ProcessNextFrameFuncs();

			if (!m_Minimized)
			{
				RenderManager::BeginFrame();

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
			m_Window->ProcessEvents();
		}
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
			(*it).Timing = timingData.Timing;
			(*it).Children= timingData.Children;
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