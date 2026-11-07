#include "egpch.h"
#include "Eagle/Script/ScriptDebuggerInfo.h"
#include "Eagle/Utils/Utils.h"
#include "Eagle/Utils/PlatformUtils.h"

#include <winsock2.h>
#include <yaml-cpp/yaml.h>

#pragma comment(lib, "Ws2_32.lib")

namespace Eagle
{
	static HANDLE s_File = INVALID_HANDLE_VALUE;
	static Path s_Filepath;

	static Path GetTempFolder()
	{
		wchar_t localAppData[MAX_PATH];
		const DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, MAX_PATH);
		if (length == 0 || length >= MAX_PATH)
			return {};

		return Path(localAppData) / "Eagle" / "Debugger" / "Games";
	}

	Path ScriptDebuggerInfo::GetTempFilePath()
	{
		const Path folder = GetTempFolder();
		if (folder.empty())
			return {};

		return folder / ("Game-" + std::to_string(GetCurrentProcessId()) + ".yaml");
	}

	uint16_t ScriptDebuggerInfo::FindFreePort(uint16_t firstPort, uint16_t count)
	{
		WSADATA wsaData;
		if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		{
			EG_CORE_ERROR("[ScriptDebuggerInfo] WSAStartup failed: {}", WSAGetLastError());
			return 0;
		}

		uint16_t result = 0;
		for (uint32_t i = 0; i < count && result == 0; ++i)
		{
			const uint16_t port = uint16_t(firstPort + i);

			SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			if (sock == INVALID_SOCKET)
				break;

			// Exclusive, so that it also fails if another process bound the port with SO_REUSEADDR (Mono's debugger agent does)
			BOOL bExclusive = TRUE;
			setsockopt(sock, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (const char*)&bExclusive, sizeof(bExclusive));

			sockaddr_in addr{};
			addr.sin_family = AF_INET;
			addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
			addr.sin_port = htons(port);
			if (bind(sock, (const sockaddr*)&addr, sizeof(addr)) == 0)
				result = port;

			closesocket(sock);
		}

		WSACleanup();
		return result;
	}

	bool ScriptDebuggerInfo::Publish(const Path& filepath, const std::string& projectName, uint16_t port, bool bGame)
	{
		Revoke();
		if (filepath.empty())
			return false;

		std::error_code error;
		std::filesystem::create_directories(filepath.parent_path(), error);

		HANDLE file = CreateFileW(filepath.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (file == INVALID_HANDLE_VALUE)
		{
			EG_CORE_ERROR("[ScriptDebuggerInfo] Couldn't create {}. Error: {}", filepath, GetLastError());
			return false;
		}

		OVERLAPPED overlapped{};
		overlapped.Offset = DWORD(LockOffset & 0xFFFFFFFF);
		overlapped.OffsetHigh = DWORD(LockOffset >> 32);
		if (!LockFileEx(file, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, 0, LockLength, 0, &overlapped))
		{
			CloseHandle(file);
			EG_CORE_WARN("[ScriptDebuggerInfo] {} is locked by another process (is this project already open in another Eagle Editor?). "
				"Script debuggers will attach to that process", filepath);
			return false;
		}

		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Version" << YAML::Value << 1;
		out << YAML::Key << "Kind" << YAML::Value << (bGame ? "Game" : "Editor");
		out << YAML::Key << "Project" << YAML::Value << projectName;
		out << YAML::Key << "Port" << YAML::Value << port;
		out << YAML::Key << "ProcessId" << YAML::Value << uint32_t(GetCurrentProcessId());
		out << YAML::Key << "Executable" << YAML::Value << Utils::AsString(Path(Utils::GetExecutablePath().generic_wstring()));
		out << YAML::EndMap;

		// Written through the locked handle rather than opening the file again
		const DWORD size = DWORD(out.size());
		DWORD written = 0;
		const bool bWritten = SetFilePointer(file, 0, nullptr, FILE_BEGIN) != INVALID_SET_FILE_POINTER
			&& SetEndOfFile(file)
			&& WriteFile(file, out.c_str(), size, &written, nullptr)
			&& written == size;
		if (!bWritten)
		{
			EG_CORE_ERROR("[ScriptDebuggerInfo] Couldn't write {}. Error: {}", filepath, GetLastError());
			CloseHandle(file);
			DeleteFileW(filepath.c_str());
			return false;
		}
		FlushFileBuffers(file);

		s_File = file;
		s_Filepath = filepath;
		return true;
	}

	void ScriptDebuggerInfo::Revoke()
	{
		if (s_File == INVALID_HANDLE_VALUE)
			return;

		CloseHandle(s_File); // Releases the lock
		s_File = INVALID_HANDLE_VALUE;

		DeleteFileW(s_Filepath.c_str());
		s_Filepath.clear();
	}
}
