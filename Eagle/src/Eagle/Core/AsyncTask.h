#pragma once

#include <atomic>
#include <functional>
#include <string>
#include <vector>

namespace Eagle
{
	enum class AsyncTaskResult
	{
		Succeeded,
		Failed,
		Cancelled
	};

	struct AsyncTaskDesc
	{
		// Shown as the title of the progress UI. For example, "Building the game"
		std::string Name;

		// If false, the Cancel button is never shown
		bool bCancelable = true;

		// Modal tasks block the editor UI with a popup while it runs.
		// Used it when the user shouldn't touch anything until the task is done. For example, during the game builds.
		bool bModal = false;
	};

	struct AsyncTaskState;

	// Given to a task's function. Used to report progress and to check whether the user requested a cancellation.
	// It must only be used on the thread that runs the task (`IsCancelRequested` is the exception, it can be called from anywhere).
	class AsyncTaskContext
	{
	public:
		explicit AsyncTaskContext(AsyncTaskState& state);
		~AsyncTaskContext();

		AsyncTaskContext(const AsyncTaskContext&) = delete;
		AsyncTaskContext& operator=(const AsyncTaskContext&) = delete;

		void SetStatus(std::string status);
		void SetDetail(std::string detail); // Note: clears `ExtraDetail`
		void SetExtraDetail(std::string detail); // Appends to the end of the "detail"
		void SetProgress(float progress); // Progress in [0; 1] within the current progress range
		void SetProgress(size_t done, size_t total) { SetProgress(total == 0 ? 1.f : float(double(done) / double(total))); }

		bool IsCancelRequested() const;
		void SetCancelable(bool bCancelable);

		// Queues `func` to be executed on the main thread. Doesn't wait for it
		void RunOnMainThread(std::function<void()> func);

		// Executes `func` on the main thread and waits for it to finish.
		// Use it to read state that can only be safely accessed from the main thread
		void RunOnMainThreadAndWait(const std::function<void()>& func);

		// Returns the context of the task that's running on the calling thread. Null if the calling thread isn't running a task.
		// Lets deeply nested code (AssetImporter, for example) report progress and check for cancellation without passing the context around
		static AsyncTaskContext* GetCurrent();

		// Just a helper for `GetCurrent() && GetCurrent()->IsCancelRequested()`
		static bool IsCurrentCancelRequested();

	private:
		struct ProgressRange
		{
			float Begin = 0.f;
			float End = 1.f;
		};

		void PushRange(float begin, float end);
		void PopRange();

	private:
		AsyncTaskState& m_State;
		std::vector<ProgressRange> m_Ranges; // Only accessed by the worker thread
		AsyncTaskContext* m_PrevContext = nullptr;

		friend class AsyncTaskProgressScope;
	};

	// Maps the [0; 1] progress that's reported inside the scope to [begin; end] of the enclosing scope.
	// Scopes can be nested. For example, when importing 4 files, each file gets a quarter of the bar,
	// and a mesh import inside it can give a part of its quarter to the material import, and so on.
	// Does nothing if the calling thread isn't running a task
	class AsyncTaskProgressScope
	{
	public:
		AsyncTaskProgressScope(float begin, float end);
		~AsyncTaskProgressScope();

		AsyncTaskProgressScope(const AsyncTaskProgressScope&) = delete;
		AsyncTaskProgressScope& operator=(const AsyncTaskProgressScope&) = delete;

	private:
		AsyncTaskContext* m_Context = nullptr;
	};

	// Runs long operations (such as game builds and asset imports) on a background thread so the app doesn't freeze,
	// and draws a progress bar with a `Cancel` button for them.
	// The next task only starts after the previous task's `onFinished` callback was executed on the main thread,
	// so a task always sees the results of the previous ones.
	class AsyncTaskManager
	{
	public:
		using TaskFunc = std::function<AsyncTaskResult(AsyncTaskContext&)>;
		using FinishedFunc = std::function<void(AsyncTaskResult)>;

		// @func. Executed on the background thread.
		// @onFinished. Executed on the main thread once the task is done. Also executed (with `Cancelled`) if the task was cancelled before it started
		static void Submit(const AsyncTaskDesc& desc, TaskFunc func, FinishedFunc onFinished = {});

		// Main thread only. Should be called once per frame.
		// Executes the functions that tasks queued with `RunOnMainThread` and their `onFinished` callbacks
		static void Update();

		// Main thread only. Draws the progress UI (a modal popup for modal tasks, an overlay in the corner for others)
		static void OnImGuiRender();

		static bool IsBusy();
		static bool IsModalTaskActive();
		static std::string GetActiveTaskName();

		static void CancelAll();

		static void Shutdown();
	};
}
