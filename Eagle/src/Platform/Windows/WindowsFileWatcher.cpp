#include "egpch.h"
#include "WindowsFileWatcher.h"

#include <Windows.h>

namespace Eagle
{
	static constexpr DWORD s_NotifyFilter = FILE_NOTIFY_CHANGE_FILE_NAME
		| FILE_NOTIFY_CHANGE_DIR_NAME
		| FILE_NOTIFY_CHANGE_LAST_WRITE
		| FILE_NOTIFY_CHANGE_SIZE
		| FILE_NOTIFY_CHANGE_CREATION;

	// The OS writes `FILE_NOTIFY_INFORMATION` entries into this buffer. If it overflows (lots of changes at once,
	// e.g. copying a hundred files into the folder), the OS reports 0 bytes and the changes are lost,
	// which is handled by just flagging "something changed"
	static constexpr DWORD s_BufferSize = 64 * 1024;

	static FileChangeAction ToFileChangeAction(DWORD action)
	{
		switch (action)
		{
			case FILE_ACTION_ADDED: return FileChangeAction::Added;
			case FILE_ACTION_REMOVED: return FileChangeAction::Removed;
			case FILE_ACTION_RENAMED_OLD_NAME: return FileChangeAction::RenamedOldName;
			case FILE_ACTION_RENAMED_NEW_NAME: return FileChangeAction::RenamedNewName;
			case FILE_ACTION_MODIFIED:
			default: return FileChangeAction::Modified;
		}
	}

	Ref<FileWatcher> FileWatcher::Create(const Path& directory, bool bRecursive)
	{
		Ref<WindowsFileWatcher> watcher = MakeRef<WindowsFileWatcher>(directory, bRecursive);
		return watcher->IsValid() ? watcher : nullptr;
	}

	WindowsFileWatcher::WindowsFileWatcher(const Path& directory, bool bRecursive)
		: m_Directory(directory)
		, m_bRecursive(bRecursive)
	{
		const HANDLE dirHandle = CreateFileW(m_Directory.wstring().c_str(),
			FILE_LIST_DIRECTORY,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			NULL,
			OPEN_EXISTING,
			FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, // `BACKUP_SEMANTICS` is required to open a directory
			NULL);

		if (dirHandle == INVALID_HANDLE_VALUE)
		{
			EG_CORE_ERROR("Failed to start watching a directory: {}. Error: {}", m_Directory, GetLastError());
			return;
		}

		const HANDLE stopEvent = CreateEventW(NULL, TRUE, FALSE, NULL); // Manual reset, initially unset
		if (!stopEvent)
		{
			EG_CORE_ERROR("Failed to create a stop-event for the file watcher. Error: {}", GetLastError());
			CloseHandle(dirHandle);
			return;
		}

		m_DirHandle = dirHandle;
		m_StopEvent = stopEvent;
		m_bValid = true;

		m_Thread = std::thread([this]() { ThreadFunc(); });
	}

	WindowsFileWatcher::~WindowsFileWatcher()
	{
		if (m_StopEvent)
			SetEvent((HANDLE)m_StopEvent); // Wakes up the worker thread, which then cancels the pending read and returns

		if (m_Thread.joinable())
			m_Thread.join();

		if (m_StopEvent)
			CloseHandle((HANDLE)m_StopEvent);

		if (m_DirHandle)
			CloseHandle((HANDLE)m_DirHandle);

		m_StopEvent = nullptr;
		m_DirHandle = nullptr;
	}

	void WindowsFileWatcher::PopEvents(std::vector<FileChangeEvent>& outEvents)
	{
		std::scoped_lock lock(m_EventsMutex);

		outEvents.insert(outEvents.end(), std::make_move_iterator(m_Events.begin()), std::make_move_iterator(m_Events.end()));
		m_Events.clear();
		m_bHasChanges.store(false, std::memory_order_release);
	}

	void WindowsFileWatcher::ClearEvents()
	{
		std::scoped_lock lock(m_EventsMutex);

		m_Events.clear();
		m_bHasChanges.store(false, std::memory_order_release);
	}

	void WindowsFileWatcher::ThreadFunc()
	{
		const HANDLE dirHandle = (HANDLE)m_DirHandle;

		OVERLAPPED overlapped{};
		overlapped.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL); // Manual reset
		if (!overlapped.hEvent)
		{
			EG_CORE_ERROR("File watcher: failed to create an overlapped event. Error: {}", GetLastError());
			return;
		}

		// `FILE_NOTIFY_INFORMATION` requires DWORD alignment, which `new` guarantees
		std::vector<uint8_t> buffer(s_BufferSize);

		const HANDLE waitHandles[2] = { overlapped.hEvent, (HANDLE)m_StopEvent };

		while (true)
		{
			DWORD bytesReturned = 0;
			if (!ReadDirectoryChangesW(dirHandle, buffer.data(), DWORD(buffer.size()), m_bRecursive,
				s_NotifyFilter, &bytesReturned, &overlapped, NULL))
			{
				EG_CORE_ERROR("File watcher: ReadDirectoryChangesW failed for {}. Error: {}", m_Directory, GetLastError());
				break;
			}

			// Blocks until either the OS reports changes or the watcher is being destroyed
			const DWORD waitResult = WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE);
			if (waitResult != WAIT_OBJECT_0)
			{
				// Either the stop event was signaled or the wait failed. Cancel the pending read and wait for it
				// to actually complete, otherwise the OS would keep writing into a buffer that's about to die
				CancelIo(dirHandle);
				GetOverlappedResult(dirHandle, &overlapped, &bytesReturned, TRUE);
				break;
			}

			if (!GetOverlappedResult(dirHandle, &overlapped, &bytesReturned, FALSE))
			{
				EG_CORE_ERROR("File watcher: GetOverlappedResult failed for {}. Error: {}", m_Directory, GetLastError());
				break;
			}
			ResetEvent(overlapped.hEvent);

			if (bytesReturned == 0)
			{
				// The internal buffer overflowed, so the individual changes are lost.
				// Still flag that something changed so that the caller refreshes
				m_bHasChanges.store(true, std::memory_order_release);
				continue;
			}

			{
				std::scoped_lock lock(m_EventsMutex);

				const uint8_t* ptr = buffer.data();
				while (true)
				{
					const FILE_NOTIFY_INFORMATION& info = *(const FILE_NOTIFY_INFORMATION*)ptr;

					if (m_Events.size() < s_MaxPendingEvents)
					{
						// `FileName` is not null terminated and its length is in bytes, not in characters
						const std::wstring filename(info.FileName, info.FileNameLength / sizeof(WCHAR));

						FileChangeEvent& event = m_Events.emplace_back();
						event.Filepath = m_Directory / Path(filename);
						event.Action = ToFileChangeAction(info.Action);
					}

					if (info.NextEntryOffset == 0)
						break;
					ptr += info.NextEntryOffset;
				}
			}

			m_bHasChanges.store(true, std::memory_order_release);
		}

		CloseHandle(overlapped.hEvent);
	}
}
