#pragma once

namespace Eagle
{
	enum class FileChangeAction
	{
		Added,
		Removed,
		Modified,
		RenamedOldName,
		RenamedNewName
	};

	struct FileChangeEvent
	{
		Path Filepath; // Absolute path of the file that changed
		FileChangeAction Action = FileChangeAction::Modified;
	};

	// Watches a directory and reports changes made to it by anything outside of the editor.
	// The implementation is platform specific (see `WindowsFileWatcher`) and does the actual watching
	// on its own thread, so none of the functions below block.
	// Everything here is safe to call from the main thread while the watcher thread is running.
	class FileWatcher
	{
	public:
		virtual ~FileWatcher() = default;

		FileWatcher(const FileWatcher&) = delete;
		FileWatcher& operator=(const FileWatcher&) = delete;

		// Creates a watcher for `directory` (which must exist and should be an absolute path).
		// Returns null if the directory can't be watched, e.g. it doesn't exist, it's on a filesystem that
		// doesn't support notifications, or the process doesn't have the required permissions.
		// Callers are expected to fall back to polling in that case
		static Ref<FileWatcher> Create(const Path& directory, bool bRecursive = false);

		// True if anything changed since the last `PopEvents`/`ClearEvents`.
		// This is all most callers need: it's a cheap atomic load that can be polled every frame
		virtual bool HasChanges() const = 0;

		// Moves every accumulated event into `outEvents` (appending to it) and resets `HasChanges`.
		// Note that a single file write usually produces several events, and that events can be dropped
		// if the OS buffer overflows - in which case `HasChanges` is still set, so it's never silently missed
		virtual void PopEvents(std::vector<FileChangeEvent>& outEvents) = 0;

		// Drops every accumulated event and resets `HasChanges`
		virtual void ClearEvents() = 0;

		virtual const Path& GetDirectory() const = 0;
		virtual bool IsRecursive() const = 0;

	protected:
		FileWatcher() = default;
	};
}
