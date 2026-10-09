#include "egpch.h"
#include "AsyncTask.h"

#include "Eagle/UI/UI.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <future>

namespace Eagle
{
	using Clock = std::chrono::steady_clock;

	struct AsyncTaskState
	{
		AsyncTaskDesc Desc;
		AsyncTaskManager::TaskFunc Func;
		AsyncTaskManager::FinishedFunc OnFinished;

		mutable std::mutex TextMutex;
		std::string Status;
		std::string Detail;
		std::string ExtraDetail;

		std::atomic<float> Progress = 0.f;
		std::atomic<bool> bCancelRequested = false;
		std::atomic<bool> bCancelable = true;
		std::atomic<bool> bStarted = false;
		Clock::time_point StartTime;
	};

	namespace
	{
		struct ManagerData
		{
			std::mutex Mutex;
			std::condition_variable CV;
			std::deque<Ref<AsyncTaskState>> Queue;
			Ref<AsyncTaskState> Running;
			std::thread Worker;
			std::atomic<bool> bWorkerExited = false;
			bool bStop = false;

			std::mutex MainThreadMutex;
			std::vector<std::function<void()>> MainThreadFuncs;
			std::atomic<std::thread::id> MainThreadID;

			bool bModalPopupOpen = false; // Main thread only
		};

		ManagerData& GetData()
		{
			static ManagerData s_Data;
			return s_Data;
		}

		thread_local AsyncTaskContext* t_CurrentContext = nullptr;

		constexpr const char* s_ModalPopupID = "###EagleAsyncTaskModal";

		bool IsMainThread()
		{
			return std::this_thread::get_id() == GetData().MainThreadID.load();
		}

		void PushMainThreadFunc(std::function<void()> func)
		{
			auto& data = GetData();
			std::scoped_lock lock(data.MainThreadMutex);
			data.MainThreadFuncs.push_back(std::move(func));
		}

		void WorkerThreadFunc()
		{
			auto& data = GetData();
			while (true)
			{
				Ref<AsyncTaskState> task;
				{
					std::unique_lock lock(data.Mutex);
					data.CV.wait(lock, [&data]() { return data.bStop || !data.Queue.empty(); });

					if (data.Queue.empty())
						break;

					task = std::move(data.Queue.front());
					data.Queue.pop_front();
					data.Running = task;
				}

				AsyncTaskResult result = AsyncTaskResult::Cancelled;
				if (!task->bCancelRequested)
				{
					task->StartTime = Clock::now();
					task->bStarted = true;
					EG_CORE_INFO("Started a background task: {}", task->Desc.Name);

					try
					{
						AsyncTaskContext context(*task);
						result = task->Func(context);
					}
					catch (const std::exception& e)
					{
						EG_CORE_ERROR("Background task `{}` failed with an exception: {}", task->Desc.Name, e.what());
						result = AsyncTaskResult::Failed;
					}
					catch (...)
					{
						EG_CORE_ERROR("Background task `{}` failed with an unknown exception", task->Desc.Name);
						result = AsyncTaskResult::Failed;
					}

					const double seconds = std::chrono::duration<double>(Clock::now() - task->StartTime).count();
					EG_CORE_INFO("Background task `{}` finished in {:.2f}s. Result: {}", task->Desc.Name, seconds, Utils::GetEnumName(result));
				}

				// Executed on the main thread after everything the task has queued with `RunOnMainThread`.
				// The worker waits for it, so that the next task sees the results of this one (for example, the assets registered by an import)
				std::promise<void> finished;
				std::future<void> finishedFuture = finished.get_future();
				PushMainThreadFunc([task, result, &finished]()
				{
					if (task->OnFinished)
					{
						try
						{
							task->OnFinished(result);
						}
						catch (const std::exception& e)
						{
							EG_CORE_ERROR("`onFinished` of the background task `{}` threw an exception: {}", task->Desc.Name, e.what());
						}
						catch (...)
						{
							EG_CORE_ERROR("`onFinished` of the background task `{}` threw an unknown exception", task->Desc.Name);
						}
					}

					{
						auto& data = GetData();
						std::scoped_lock lock(data.Mutex);
						if (data.Running == task)
							data.Running.reset();
					}

					finished.set_value();
				});
				finishedFuture.wait();
			}

			data.bWorkerExited = true;
		}

		Ref<AsyncTaskState> GetDisplayedTask(size_t* outQueuedCount)
		{
			auto& data = GetData();
			std::scoped_lock lock(data.Mutex);

			if (data.Running)
			{
				*outQueuedCount = data.Queue.size();
				return data.Running;
			}
			if (!data.Queue.empty())
			{
				*outQueuedCount = data.Queue.size() - 1;
				return data.Queue.front();
			}

			*outQueuedCount = 0;
			return {};
		}
	}

	AsyncTaskContext::AsyncTaskContext(AsyncTaskState& state)
		: m_State(state)
	{
		m_Ranges.reserve(8);
		m_Ranges.push_back({ 0.f, 1.f });

		m_PrevContext = t_CurrentContext;
		t_CurrentContext = this;
	}

	AsyncTaskContext::~AsyncTaskContext()
	{
		t_CurrentContext = m_PrevContext;
	}

	void AsyncTaskContext::SetStatus(std::string status)
	{
		std::scoped_lock lock(m_State.TextMutex);
		m_State.Status = std::move(status);
	}

	void AsyncTaskContext::SetDetail(std::string detail)
	{
		std::scoped_lock lock(m_State.TextMutex);
		m_State.Detail = std::move(detail);
		m_State.ExtraDetail.clear();
	}

	void AsyncTaskContext::SetExtraDetail(std::string detail)
	{
		std::scoped_lock lock(m_State.TextMutex);
		m_State.ExtraDetail = std::move(detail);
	}

	void AsyncTaskContext::SetProgress(float progress)
	{
		progress = glm::clamp(progress, 0.f, 1.f);

		const ProgressRange& range = m_Ranges.back();
		m_State.Progress = range.Begin + (range.End - range.Begin) * progress;
	}

	bool AsyncTaskContext::IsCancelRequested() const
	{
		return m_State.bCancelRequested;
	}

	void AsyncTaskContext::SetCancelable(bool bCancelable)
	{
		m_State.bCancelable = bCancelable;
	}

	void AsyncTaskContext::RunOnMainThread(std::function<void()> func)
	{
		if (func)
			PushMainThreadFunc(std::move(func));
	}

	void AsyncTaskContext::RunOnMainThreadAndWait(const std::function<void()>& func)
	{
		if (!func)
			return;

		if (IsMainThread())
		{
			func();
			return;
		}

		std::promise<void> done;
		std::future<void> doneFuture = done.get_future();
		PushMainThreadFunc([&func, &done]()
		{
			try
			{
				func();
				done.set_value();
			}
			catch (...)
			{
				done.set_exception(std::current_exception());
			}
		});
		doneFuture.get(); // Rethrows if `func` threw
	}

	AsyncTaskContext* AsyncTaskContext::GetCurrent()
	{
		return t_CurrentContext;
	}

	bool AsyncTaskContext::IsCurrentCancelRequested()
	{
		const AsyncTaskContext* context = GetCurrent();
		return context && context->IsCancelRequested();
	}

	void AsyncTaskContext::PushRange(float begin, float end)
	{
		begin = glm::clamp(begin, 0.f, 1.f);
		end = glm::clamp(end, begin, 1.f);

		const ProgressRange& parent = m_Ranges.back();
		const float parentSize = parent.End - parent.Begin;

		ProgressRange range;
		range.Begin = parent.Begin + parentSize * begin;
		range.End = parent.Begin + parentSize * end;
		m_Ranges.push_back(range);

		SetProgress(0.f);
	}

	void AsyncTaskContext::PopRange()
	{
		SetProgress(1.f);
		if (m_Ranges.size() > 1)
			m_Ranges.pop_back();
	}

	AsyncTaskProgressScope::AsyncTaskProgressScope(float begin, float end)
		: m_Context(AsyncTaskContext::GetCurrent())
	{
		if (m_Context)
			m_Context->PushRange(begin, end);
	}

	AsyncTaskProgressScope::~AsyncTaskProgressScope()
	{
		if (m_Context)
			m_Context->PopRange();
	}

	void AsyncTaskManager::Submit(const AsyncTaskDesc& desc, TaskFunc func, FinishedFunc onFinished)
	{
		EG_CORE_ASSERT(func);

		auto task = MakeRef<AsyncTaskState>();
		task->Desc = desc;
		task->Func = std::move(func);
		task->OnFinished = std::move(onFinished);
		task->bCancelable = desc.bCancelable;

		auto& data = GetData();
		{
			std::scoped_lock lock(data.Mutex);
			if (data.bStop)
			{
				EG_CORE_WARN("Can't start a background task `{}`. The app is shutting down", desc.Name);
				if (task->OnFinished)
					PushMainThreadFunc([task]() { task->OnFinished(AsyncTaskResult::Cancelled); });
				return;
			}

			data.Queue.push_back(task);
			if (!data.Worker.joinable())
				data.Worker = std::thread(WorkerThreadFunc);
		}
		data.CV.notify_one();
	}

	void AsyncTaskManager::Update()
	{
		auto& data = GetData();
		data.MainThreadID = std::this_thread::get_id();

		std::vector<std::function<void()>> funcs;
		{
			std::scoped_lock lock(data.MainThreadMutex);
			funcs.swap(data.MainThreadFuncs);
		}

		for (auto& func : funcs)
		{
			try
			{
				func();
			}
			catch (const std::exception& e)
			{
				EG_CORE_ERROR("A main-thread function of a background task threw an exception: {}", e.what());
			}
			catch (...)
			{
				EG_CORE_ERROR("A main-thread function of a background task threw an unknown exception");
			}
		}
	}

	bool AsyncTaskManager::IsBusy()
	{
		auto& data = GetData();
		std::scoped_lock lock(data.Mutex);
		return data.Running || !data.Queue.empty();
	}

	bool AsyncTaskManager::IsModalTaskActive()
	{
		size_t unused = 0;
		const Ref<AsyncTaskState> task = GetDisplayedTask(&unused);
		return task && task->Desc.bModal;
	}

	std::string AsyncTaskManager::GetActiveTaskName()
	{
		size_t unused = 0;
		const Ref<AsyncTaskState> task = GetDisplayedTask(&unused);
		return task ? task->Desc.Name : std::string{};
	}

	void AsyncTaskManager::CancelAll()
	{
		auto& data = GetData();
		std::scoped_lock lock(data.Mutex);

		for (auto& task : data.Queue)
			task->bCancelRequested = true;
		if (data.Running)
			data.Running->bCancelRequested = true;
	}

	void AsyncTaskManager::Shutdown()
	{
		auto& data = GetData();

		const std::string runningTask = GetActiveTaskName();
		if (!runningTask.empty())
			EG_CORE_WARN("Cancelling background tasks. Waiting for `{}` to stop...", runningTask);

		CancelAll();
		{
			std::scoped_lock lock(data.Mutex);
			data.bStop = true;
		}
		data.CV.notify_all();

		if (data.Worker.joinable())
		{
			// The worker might be waiting for the main thread (`RunOnMainThreadAndWait`, `onFinished`), so keep executing main-thread functions
			while (!data.bWorkerExited)
			{
				Update();
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
			data.Worker.join();
		}
		Update();
	}

	// ----------------- UI -----------------
	static std::string FormatDuration(double seconds)
	{
		const uint64_t total = uint64_t(std::max(0.0, seconds));
		const uint64_t hours = total / 3600;
		const uint64_t minutes = (total / 60) % 60;
		const uint64_t secs = total % 60;

		char buffer[32];
		if (hours > 0)
			snprintf(buffer, sizeof(buffer), "%llu:%02llu:%02llu", (unsigned long long)hours, (unsigned long long)minutes, (unsigned long long)secs);
		else
			snprintf(buffer, sizeof(buffer), "%llu:%02llu", (unsigned long long)minutes, (unsigned long long)secs);
		return buffer;
	}

	// Draws a single line of text. If it doesn't fit, its beginning is replaced with "...", since the end of a path is usually more important
	static void TextTruncatedFront(const std::string& text, float maxWidth)
	{
		if (ImGui::CalcTextSize(text.c_str()).x <= maxWidth)
		{
			ImGui::TextUnformatted(text.c_str());
			return;
		}

		const char* ellipsis = "...";
		const float ellipsisWidth = ImGui::CalcTextSize(ellipsis).x;
		const char* begin = text.c_str();
		const char* end = begin + text.size();
		while (begin < end && ImGui::CalcTextSize(begin, end).x + ellipsisWidth > maxWidth)
		{
			++begin;
			while (begin < end && (uint8_t(*begin) & 0xC0) == 0x80) // Don't cut UTF-8 sequences in half
				++begin;
		}

		const std::string result = std::string(ellipsis) + std::string(begin, end);
		ImGui::TextUnformatted(result.c_str());
	}

	static void DrawTaskBody(AsyncTaskState& task, size_t queuedCount, float width)
	{
		std::string status;
		std::string detail;
		std::string extraDetail;
		{
			std::scoped_lock lock(task.TextMutex);
			status = task.Status;
			detail = task.Detail;
			extraDetail = task.ExtraDetail;
		}
		detail = detail + extraDetail;

		const bool bStarted = task.bStarted;
		if (!bStarted)
			status = "Waiting for other tasks to finish...";
		else if (status.empty())
			status = "Working...";

		TextTruncatedFront(status, width);

		const float progress = task.Progress;
		char overlay[16];
		snprintf(overlay, sizeof(overlay), "%d%%", int(progress * 100.f));
		ImGui::ProgressBar(progress, ImVec2(width, 0.f), overlay);

		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
		TextTruncatedFront(detail.empty() ? std::string(" ") : detail, width);

		const double elapsed = bStarted ? std::chrono::duration<double>(Clock::now() - task.StartTime).count() : 0.0;
		std::string footer = "Elapsed: " + FormatDuration(elapsed);
		if (queuedCount > 0)
			footer += "   |   " + std::to_string(queuedCount) + " more queued";
		ImGui::TextUnformatted(footer.c_str());
		ImGui::PopStyleColor();

		if (task.Desc.bCancelable)
		{
			const bool bCancelRequested = task.bCancelRequested;
			const bool bCanCancel = task.bCancelable && !bCancelRequested;
			const char* label = bCancelRequested ? "Cancelling..." : "Cancel";
			const float buttonWidth = ImGui::CalcTextSize("Cancelling...").x + ImGui::GetStyle().FramePadding.x * 2.f;

			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + width - buttonWidth);
			if (!bCanCancel)
				UI::PushItemDisabled();
			if (ImGui::Button(label, ImVec2(buttonWidth, 0.f)) && bCanCancel)
			{
				task.bCancelRequested = true;
				EG_CORE_WARN("Cancellation requested: {}", task.Desc.Name);
			}
			if (!bCanCancel)
			{
				UI::PopItemDisabled();
				if (!bCancelRequested && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
					ImGui::SetTooltip("This step can't be interrupted");
			}
		}
	}

	void AsyncTaskManager::OnImGuiRender()
	{
		auto& data = GetData();

		size_t queuedCount = 0;
		Ref<AsyncTaskState> task = GetDisplayedTask(&queuedCount);
		const bool bModal = task && task->Desc.bModal;

		const ImGuiStyle& style = ImGui::GetStyle();
		const float contentWidth = ImGui::GetFontSize() * 26.f;
		const float windowWidth = contentWidth + style.WindowPadding.x * 2.f;
		constexpr ImGuiWindowFlags modalFlags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse;

		if (bModal)
		{
			// The visible title is the task name. The ID after `###` stays the same, so the popup isn't reopened when the name changes
			const std::string popupName = task->Desc.Name + s_ModalPopupID;
			if (!ImGui::IsPopupOpen(popupName.c_str()))
				ImGui::OpenPopup(popupName.c_str());

			const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
			ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
			ImGui::SetNextWindowSize(ImVec2(windowWidth, 0.f), ImGuiCond_Always);
			if (ImGui::BeginPopupModal(popupName.c_str(), nullptr, modalFlags))
			{
				DrawTaskBody(*task, queuedCount, contentWidth);
				ImGui::EndPopup();
			}
			data.bModalPopupOpen = true;
		}
		else if (data.bModalPopupOpen)
		{
			// The modal task has finished. Close its popup without submitting it again, so that an empty popup doesn't flash for a frame
			data.bModalPopupOpen = false;
			ImGuiContext& g = *GImGui;
			const ImGuiID popupID = ImGui::GetID(s_ModalPopupID);
			for (int i = 0; i < g.OpenPopupStack.Size; ++i)
			{
				if (g.OpenPopupStack[i].PopupId == popupID)
				{
					ImGui::ClosePopupToLevel(i, true);
					break;
				}
			}
		}

		if (!task || bModal)
			return;

		// Non-modal task. Draw a small overlay in the bottom-right corner of the main window
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		const float margin = ImGui::GetFontSize();
		ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x - margin, viewport->WorkPos.y + viewport->WorkSize.y - margin), ImGuiCond_Always, ImVec2(1.f, 1.f));
		ImGui::SetNextWindowViewport(viewport->ID);
		ImGui::SetNextWindowSize(ImVec2(windowWidth, 0.f), ImGuiCond_Always);

		constexpr ImGuiWindowFlags overlayFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;
		if (ImGui::Begin("##EagleAsyncTaskOverlay", nullptr, overlayFlags))
		{
			ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow()); // Keep it above the dockspace & floating windows
			TextTruncatedFront(task->Desc.Name, contentWidth);
			ImGui::Separator();
			DrawTaskBody(*task, queuedCount, contentWidth);
		}
		ImGui::End();
	}
}
