#pragma once

namespace Eagle
{
	// Tells script debuggers (Visual Studio extension for debugging) how to attach to this process.
	// Each editor and each debuggable game (built with the "Script debugging" option) runs Mono's debugger agent on its own free port, and writes it to a small YAML file:
	//     Editors: `<Project>/Cache/Scripts/Debugger.yaml`, while the project is open.
	//     Games: `GetTempFilePath()`, while the game runs.
	//
	// The process keeps the file locked while it's published. The OS releases the lock when the process ends, even if it crashes,
	// so a reader can tell a live file from a stale one. If the reader can take the lock itself, no process owns the file.
	// Debuggers must never "test" the port by connecting to it because the agent stops listening after a connection without a debugger handshake.
	// On Windows: exclusive `LockFileEx` lock on `LockLength` byte(s) at `LockOffset`. The range is past the end of the file, so the content stays readable while it's locked.
	class ScriptDebuggerInfo
	{
	public:
		static constexpr uint64_t LockOffset = 1073741824; // 1 GB
		static constexpr uint32_t LockLength = 1;
		static constexpr uint16_t FirstPort = 2550;
		static constexpr uint16_t PortCount = 100;

		static Path GetTempFilePath();

		// Returns the first port in [firstPort, firstPort + count) that isn't used by another process, or 0 if there's none
		static uint16_t FindFreePort(uint16_t firstPort, uint16_t count);

		// Writes the file and locks it until `Revoke()` is called. Returns false if the file couldn't be written,
		// or if another process owns it (for example, the same project is open in another editor).
		static bool Publish(const Path& filepath, const std::string& projectName, uint16_t port, bool bGame);

		// Unlocks and deletes the file written by `Publish()`
		static void Revoke();
	};
}
