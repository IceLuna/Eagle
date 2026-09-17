#pragma once

#include "Eagle/Utils/FileWatcher.h"

namespace Eagle
{
	// `ReadDirectoryChangesW` based watcher.
	// The read is issued asynchronously on a worker thread which blocks until either the OS reports changes
	// or the watcher is destroyed, so it costs no CPU while nothing happens.
	// Note: Windows handles are stored as `void*` on purpose, so that <Windows.h> doesn't leak into the engine
	class WindowsFileWatcher : public FileWatcher
	{
	public:
		WindowsFileWatcher(const Path& directory, bool bRecursive);
		virtual ~WindowsFileWatcher();

		bool HasChanges() const override { return m_bHasChanges.load(std::memory_order_acquire); }
		void PopEvents(std::vector<FileChangeEvent>& outEvents) override;
		void ClearEvents() override;

		const Path& GetDirectory() const override { return m_Directory; }
		bool IsRecursive() const override { return m_bRecursive; }

		// Used by `FileWatcher::Create` to discard a watcher that failed to start
		bool IsValid() const { return m_bValid; }

	private:
		void ThreadFunc();

	private:
		// If more than that many events pile up without anyone popping them, they're dropped.
		// `HasChanges` stays set, so a caller that only cares about "something changed" is unaffected
		static constexpr size_t s_MaxPendingEvents = 1024;

		Path m_Directory;

		std::vector<FileChangeEvent> m_Events;
		mutable std::mutex m_EventsMutex;

		std::thread m_Thread;

		void* m_DirHandle = nullptr;  // HANDLE of the watched directory
		void* m_StopEvent = nullptr;  // HANDLE of the event used to stop the worker thread

		std::atomic<bool> m_bHasChanges = false;
		bool m_bRecursive = false;
		bool m_bValid = false;
	};
}
