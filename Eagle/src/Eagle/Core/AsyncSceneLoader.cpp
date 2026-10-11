#include "egpch.h"
#include "AsyncSceneLoader.h"

#include "Eagle/Core/Application.h"
#include "Eagle/Core/AsyncTask.h"
#include "Eagle/Core/Scene.h"
#include "Eagle/Core/SceneSerializer.h"
#include "Eagle/Asset/AssetManager.h"
#include "Eagle/Renderer/RenderManager.h"
#include "Eagle/Script/ScriptEngine.h"
#include "Eagle/Utils/PlatformUtils.h"
#include "Eagle/Utils/SerializerUtils.h"
#include "Eagle/Utils/Timer.h"
#include "Eagle/Utils/Utils.h"

namespace Eagle
{
	struct LoaderData
	{
		std::mutex Mutex;
		std::condition_variable CV;
		std::thread Worker;
		Ref<SceneLoadRequest> Pending;
		Ref<SceneLoadRequest> InProgress;
		bool bStop = false;
		std::atomic<bool> bWorkerExited = false;

		// Main thread only
		Ref<SceneLoadRequest> Active; // The latest request from `Load` until it's activated or cancelled
		std::unordered_map<uint64_t, Ref<SceneLoadRequest>> Requests;
		uint64_t NextID = 1;
	};

	// Progress split. Usually, loading assets takes most of the time
	constexpr float s_AssetsProgressEnd = 0.85f;
	constexpr float s_EntitiesProgressEnd = 0.99f;

	// How long the main thread can spend loading main-thread-only assets per frame, so the current scene stays responsive
	constexpr uint64_t s_MainThreadBudgetMs = 8;

	static LoaderData s_Data;

	SceneLoadRequest::SceneLoadRequest(uint64_t id, const Ref<AssetScene>& sceneAsset, bool bActivateWhenReady, bool bReuseCurrentSceneRenderer, bool bRuntime)
		: m_SceneAsset(sceneAsset)
		, m_bActivationRequested(bActivateWhenReady)
		, m_ID(id)
		, m_bReuseCurrentSceneRenderer(bReuseCurrentSceneRenderer)
		, m_bRuntime(bRuntime)
	{}

	bool SceneLoadRequest::IsDone() const
	{
		const SceneLoadState state = GetState();
		return state == SceneLoadState::Activated || state == SceneLoadState::Failed || state == SceneLoadState::Cancelled;
	}

	void SceneLoadRequest::Activate()
	{
		EG_CORE_ASSERT(MainThread::IsMainThread());

		m_bActivationRequested = true;
		if (GetState() == SceneLoadState::Ready)
		{
			auto it = s_Data.Requests.find(m_ID);
			if (it != s_Data.Requests.end())
				AsyncSceneLoader::ScheduleActivation(it->second);
		}
	}

	void SceneLoadRequest::Cancel()
	{
		EG_CORE_ASSERT(MainThread::IsMainThread());

		if (IsDone())
			return;

		m_bCancelRequested = true;

		// If it's already loaded, the background thread is done with it. So finish it here
		SceneLoadState expected = SceneLoadState::Ready;
		if (m_State.compare_exchange_strong(expected, SceneLoadState::Cancelled))
			m_LoadedScene.reset();

		EG_CORE_INFO("Cancelled loading the scene: {}", m_SceneAsset->GetPath());
	}

	Ref<SceneLoadRequest> AsyncSceneLoader::Load(const Ref<AssetScene>& sceneAsset, bool bActivateWhenReady, bool bReuseCurrentSceneRenderer, bool bRuntime)
	{
		EG_CORE_ASSERT(MainThread::IsMainThread());
		if (!sceneAsset)
		{
			EG_CORE_ERROR("Failed to load a scene in background. The scene asset is null");
			return {};
		}

		// Only one scene is loaded at a time
		CancelAll(false);

		// Remove processed requests
		for (auto it = s_Data.Requests.begin(); it != s_Data.Requests.end(); )
		{
			if (it->second->IsDone())
				it = s_Data.Requests.erase(it);
			else
				++it;
		}

		Ref<SceneLoadRequest> request(new SceneLoadRequest(s_Data.NextID++, sceneAsset, bActivateWhenReady, bReuseCurrentSceneRenderer, bRuntime));
		s_Data.Requests.emplace(request->GetID(), request);
		s_Data.Active = request;

		{
			std::scoped_lock lock(s_Data.Mutex);
			if (s_Data.bStop)
			{
				EG_CORE_WARN("Can't load a scene in background. The engine is shutting down");
				request->m_State = SceneLoadState::Cancelled;
				return request;
			}

			s_Data.Pending = request;
			if (!s_Data.Worker.joinable())
				s_Data.Worker = std::thread(&AsyncSceneLoader::WorkerThreadFunc);
		}
		s_Data.CV.notify_one();

		return request;
	}

	Ref<SceneLoadRequest> AsyncSceneLoader::GetActiveRequest()
	{
		if (s_Data.Active && !s_Data.Active->IsDone())
			return s_Data.Active;
		return {};
	}

	Ref<SceneLoadRequest> AsyncSceneLoader::FindRequest(uint64_t id)
	{
		auto it = s_Data.Requests.find(id);
		return it != s_Data.Requests.end() ? it->second : nullptr;
	}

	bool AsyncSceneLoader::IsBusy()
	{
		std::scoped_lock lock(s_Data.Mutex);
		return s_Data.Pending || s_Data.InProgress;
	}

	void AsyncSceneLoader::CancelAll(bool bWait)
	{
		EG_CORE_ASSERT(MainThread::IsMainThread());

		Ref<SceneLoadRequest> pending;
		Ref<SceneLoadRequest> inProgress;
		{
			std::scoped_lock lock(s_Data.Mutex);
			pending = std::move(s_Data.Pending);
			inProgress = s_Data.InProgress;
		}

		if (pending)
		{
			pending->m_bCancelRequested = true;
			pending->m_State = SceneLoadState::Cancelled;
		}

		if (inProgress)
			inProgress->m_bCancelRequested = true;

		if (s_Data.Active)
		{
			s_Data.Active->Cancel();
			s_Data.Active.reset();
		}

		if (bWait)
		{
			// The background thread might be waiting for the main thread (main-thread-only assets), so keep executing main-thread functions
			while (IsBusy())
			{
				MainThread::ExecutePending();
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
		}
	}

	void AsyncSceneLoader::Shutdown()
	{
		CancelAll(false);

		{
			std::scoped_lock lock(s_Data.Mutex);
			s_Data.bStop = true;
		}
		s_Data.CV.notify_all();

		if (s_Data.Worker.joinable())
		{
			while (!s_Data.bWorkerExited)
			{
				MainThread::ExecutePending();
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
			s_Data.Worker.join();
		}

		s_Data.Requests.clear();
		s_Data.Active.reset();
	}

	void AsyncSceneLoader::ScheduleActivation(const Ref<SceneLoadRequest>& request)
	{
		if (request->m_bActivationScheduled)
			return;

		request->m_bActivationScheduled = true;
		Application::Get().CallNextFrame([request]()
		{
			OpenLoadedScene(request);
		});
	}

	void AsyncSceneLoader::OpenLoadedScene(const Ref<SceneLoadRequest>& request)
	{
		if (request->m_bCancelRequested || request->GetState() != SceneLoadState::Ready)
		{
			if (request->GetState() == SceneLoadState::Ready)
				request->m_State = SceneLoadState::Cancelled;
			request->m_LoadedScene.reset();
			return;
		}

		Ref<Scene> scene = std::move(request->m_LoadedScene);
		RenderManager::Wait();

		const Ref<Scene>& currentScene = Scene::GetCurrentScene();
		Ref<SceneRenderer> sceneRenderer = (request->m_bReuseCurrentSceneRenderer && currentScene) ? currentScene->GetSceneRenderer() : nullptr;
		if (!sceneRenderer)
			sceneRenderer = MakeRef<SceneRenderer>(glm::uvec2{ scene->m_ViewportWidth, scene->m_ViewportHeight });
		scene->SetSceneRenderer(sceneRenderer);

		AssetManager::ResetRuntimeAsset();

		request->m_State = SceneLoadState::Activated;
		if (s_Data.Active == request)
			s_Data.Active.reset();

		EG_CORE_INFO("Opening the scene loaded in background: {}", request->GetSceneAsset()->GetPath());
		Scene::OnSceneOpened(scene);
	}

	void AsyncSceneLoader::WorkerThreadFunc()
	{
		while (true)
		{
			Ref<SceneLoadRequest> request;
			{
				std::unique_lock lock(s_Data.Mutex);
				s_Data.CV.wait(lock, []() { return s_Data.bStop || s_Data.Pending; });

				if (s_Data.bStop && !s_Data.Pending)
					break;

				request = std::move(s_Data.Pending);
				s_Data.InProgress = request;
			}

			try
			{
				LoadScene(request);
			}
			catch (const std::exception& e)
			{
				EG_CORE_ERROR("Failed to load the scene {}: {}", request->GetSceneAsset()->GetPath(), e.what());
				// Once it's `Ready`, it belongs to the main thread. Before that, only this thread changes the state
				if (request->GetState() != SceneLoadState::Ready)
					request->m_State = SceneLoadState::Failed;
			}
			catch (...)
			{
				EG_CORE_ERROR("Failed to load the scene {}: unknown exception", request->GetSceneAsset()->GetPath());
				if (request->GetState() != SceneLoadState::Ready)
					request->m_State = SceneLoadState::Failed;
			}

			{
				std::scoped_lock lock(s_Data.Mutex);
				s_Data.InProgress.reset();
			}
		}

		s_Data.bWorkerExited = true;
	}

	void AsyncSceneLoader::LoadScene(const Ref<SceneLoadRequest>& request)
	{
		SceneLoadRequest& req = *request;
		auto isCancelled = [&req]() { return req.m_bCancelRequested.load(); };
		auto setProgress = [&req](float progress) { req.m_Progress = glm::clamp(progress, 0.f, 1.f); };
		auto finishCancelled = [&req]()
		{
			req.m_State = SceneLoadState::Cancelled;
			EG_CORE_INFO("Stopped loading the scene: {}", req.GetSceneAsset()->GetPath());
		};

		if (isCancelled())
			return finishCancelled();

		req.m_State = SceneLoadState::Loading;
		const Path path = req.GetSceneAsset()->GetPath();
		const bool bGame = Application::Get().IsGame();
		EG_CORE_INFO("Loading the scene in background: {}", path);
		Timer timer;

		// 1. Read the scene
		YAML::Node baseNode;
		if (bGame)
		{
			Ref<ScopedDataBuffer> sceneData;
			if (!AssetManager::GetRuntimeAssetData(path, &sceneData))
			{
				EG_CORE_ERROR("Failed to load the scene. The asset is not found: {}", path);
				req.m_State = SceneLoadState::Failed;
				return;
			}
			Utils::ReadYAML(sceneData->GetDataBuffer(), &baseNode);

			// Same as `Scene::OpenScene`, the new scene doesn't share loaded assets with the previous one.
			MainThread::SubmitAndWait([]() { AssetManager::ResetGameAssets(); });
		}
		else if (std::filesystem::exists(path)) // If it doesn't exist, an empty scene is opened (same as `Scene::OpenScene`)
		{
			const ScopedDataBuffer sceneData = FileSystem::Read(path);
			if (sceneData)
				Utils::ReadYAML(sceneData, &baseNode);
		}

		// 2. Load the assets (the editor has all assets loaded)
		if (bGame)
		{
			const auto dependencies = AssetManager::GatherAssetPackDependencies(baseNode);

			size_t totalSize = 0;
			for (const auto& dependency : dependencies)
				totalSize += dependency.DataSize;
			totalSize = std::max(totalSize, size_t(1));

			size_t loadedSize = 0;
			auto updateAssetsProgress = [&]() { setProgress(s_AssetsProgressEnd * float(double(loadedSize) / double(totalSize))); };

			std::vector<const AssetManager::AssetPackDependency*> mainThreadAssets;
			for (const auto& dependency : dependencies)
			{
				if (isCancelled())
					return finishCancelled();

				if (dependency.bRequiresMainThread)
				{
					mainThreadAssets.push_back(&dependency);
					continue;
				}

				// Load the asset
				Ref<Asset> asset;
				AssetManager::Get(dependency.ID, &asset);

				loadedSize += dependency.DataSize;
				updateAssetsProgress();
			}

			// Some assets can only be created on the main thread. Load them in small batches,
			// so that the main thread can keep updating the current scene in between
			size_t index = 0;
			while (index < mainThreadAssets.size())
			{
				if (isCancelled())
					return finishCancelled();

				MainThread::SubmitAndWait([&]()
				{
					Timer budget;
					do
					{
						const auto* dependency = mainThreadAssets[index++];
						Ref<Asset> asset;
						AssetManager::Get(dependency->ID, &asset);

						loadedSize += dependency->DataSize;
					} while (index < mainThreadAssets.size() && budget.GetMilliseconds() < s_MainThreadBudgetMs && !isCancelled());
				});
				updateAssetsProgress();
			}

			EG_CORE_INFO("Loaded {} assets for the scene in {}s: {}", dependencies.size(), timer.GetSeconds(), path);
		}

		if (isCancelled())
			return finishCancelled();

		// 3. Create the scene and its entities. The scene doesn't have a scene renderer, so the current scene's renderer isn't touched
		// Note: when this function is modified, don't forget to update `Scene::OpenScene`
		Ref<Scene> scene = MakeRef<Scene>(Utils::AsString(path), req.m_bRuntime);
		scene->SetSkybox(SkySettings{});
		scene->SetSkybox(nullptr);
		scene->SetSkyboxIntensity(1.f);

		if (baseNode)
		{
			const float progressBegin = bGame ? s_AssetsProgressEnd : 0.f;
			const bool bDeserialized = SceneSerializer::Deserialize(scene, baseNode, [&](size_t done, size_t total)
			{
				setProgress(progressBegin + (s_EntitiesProgressEnd - progressBegin) * float(double(done) / double(std::max(total, size_t(1)))));
				return !isCancelled();
			});

			if (!bDeserialized)
			{
				if (isCancelled())
					return finishCancelled();

				EG_CORE_ERROR("Failed to load the scene. Invalid asset: {}", path);
				req.m_State = SceneLoadState::Failed;
				return;
			}
		}

		if (isCancelled())
			return finishCancelled();

		req.m_LoadedScene = std::move(scene);
		setProgress(1.f);
		req.m_State = SceneLoadState::Ready;
		EG_CORE_INFO("Loaded the scene in background in {}s: {}", timer.GetSeconds(), path);

		MainThread::Submit([request]()
		{
			if (request->m_bCancelRequested)
			{
				SceneLoadState expected = SceneLoadState::Ready;
				if (request->m_State.compare_exchange_strong(expected, SceneLoadState::Cancelled))
					request->m_LoadedScene.reset();
				return;
			}

			if (request->m_bActivationRequested)
				ScheduleActivation(request);
		});
	}
}
