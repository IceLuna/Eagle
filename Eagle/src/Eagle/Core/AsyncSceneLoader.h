#pragma once

namespace Eagle
{
	class AssetScene;
	class Scene;

	enum class SceneLoadState
	{
		Queued,    // Waiting for the background thread to start loading it
		Loading,
		Ready,     // Loaded, waiting for `Activate()` (only happens if it was requested with `bActivateWhenReady == false`)
		Activated, // Opened (or it's about to be opened at the beginning of the next frame)
		Failed,
		Cancelled,
	};

	class SceneLoadRequest
	{
	public:
		SceneLoadRequest(const SceneLoadRequest&) = delete;
		SceneLoadRequest& operator=(const SceneLoadRequest&) = delete;

		// Progress is in [0; 1] range
		float GetProgress() const { return m_Progress; }
		SceneLoadState GetState() const { return m_State; }

		bool IsReady() const { return GetState() == SceneLoadState::Ready; }

		// True if it's finished (activated, failed or cancelled)
		bool IsDone() const;

		const Ref<AssetScene>& GetSceneAsset() const { return m_SceneAsset; }

		uint64_t GetID() const { return m_ID; }

		// Main thread only. Opens the loaded scene at the beginning of the next frame.
		// If it's still loading, it'll be opened as soon as it's loaded
		void Activate();

		// Main thread only. Stops loading it or drops it if it's already loaded but not activated
		void Cancel();

	private:
		SceneLoadRequest(uint64_t id, const Ref<AssetScene>& sceneAsset, bool bActivateWhenReady, bool bReuseCurrentSceneRenderer, bool bRuntime);

	private:
		Ref<AssetScene> m_SceneAsset;
		Ref<Scene> m_LoadedScene;

		std::atomic<float> m_Progress = 0.f;
		std::atomic<SceneLoadState> m_State = SceneLoadState::Queued;

		std::atomic<bool> m_bCancelRequested = false;
		std::atomic<bool> m_bActivationRequested = false;

		uint64_t m_ID = 0;
		bool m_bActivationScheduled = false;
		bool m_bReuseCurrentSceneRenderer = true;
		bool m_bRuntime = false;

		friend class AsyncSceneLoader;
	};

	// Loads scenes on a background thread. Use it through `Scene::OpenSceneAsync`.
	// How it works:
	//	1. For game builds, the assets that the scene uses are found and loaded one by one.
	//	2. The scene and its entities are created. The scene doesn't have a scene renderer yet, so the current scene isn't affected.
	//	3. At the beginning of a frame, the main thread gives it a scene renderer and opens it.
	// C# scripts are instantiated when the scene is opened (OnRuntimeStart), not during loading.
	class AsyncSceneLoader
	{
	public:
		// Main thread only. Cancels the previous load
		static Ref<SceneLoadRequest> Load(const Ref<AssetScene>& sceneAsset, bool bActivateWhenReady, bool bReuseCurrentSceneRenderer, bool bRuntime);

		// Main thread only
		static Ref<SceneLoadRequest> GetActiveRequest();

		// Main thread only
		static Ref<SceneLoadRequest> FindRequest(uint64_t id);

		static bool IsBusy();

		// Main thread only
		// @bWait. Set to true to wait for the background thread to stop
		static void CancelAll(bool bWait);

		static void Shutdown();

	private:
		static void ScheduleActivation(const Ref<SceneLoadRequest>& request);
		static void OpenLoadedScene(const Ref<SceneLoadRequest>& request);
		static void LoadScene(const Ref<SceneLoadRequest>& request);
		static void WorkerThreadFunc();

		friend class SceneLoadRequest;
	};
}
