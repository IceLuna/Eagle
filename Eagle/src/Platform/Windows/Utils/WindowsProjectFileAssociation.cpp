#include "egpch.h"

#include "Eagle/Core/Project.h"
#include "Eagle/Utils/PlatformUtils.h"
#include "Eagle/Utils/Utils.h"

#include <shlobj.h>
#include <shlwapi.h>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")

// How it works:
// Windows opens a file with the "open" command of its ProgID: HKEY_CLASSES_ROOT\.egproj (default value) = ProgID,
// and HKEY_CLASSES_ROOT\<ProgID>\shell\open\command (default value) = the command line, where %1 is the file.
// HKEY_CLASSES_ROOT is a merged view of HKEY_LOCAL_MACHINE\Software\Classes (all users, requires admin rights to write)
// and HKEY_CURRENT_USER\Software\Classes (the current user), and the latter wins.
// So the editor registers itself for the current user only, which doesn't require admin rights, and overrides the old registration if there's one.
//
// If the user picks a program with "Open with > Always use this app", Windows stores it in
// HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Explorer\FileExts\.egproj\UserChoice, which wins over both.
// It's protected by a hash, so that only the user can change it. AssocQueryString takes it into account.

namespace Eagle::ProjectFileAssociation
{
	namespace
	{
		std::wstring GetExtension()
		{
			const std::string extension = Project::GetExtension();
			return std::wstring(extension.begin(), extension.end());
		}

		// Returns an empty string if there's no association or it couldn't be read
		std::wstring QueryAssociation(ASSOCSTR what)
		{
			// ASSOCF_INIT_IGNOREUNKNOWN - fail instead of returning the "Open with" dialog if nothing is registered
			constexpr ASSOCF flags = ASSOCF_INIT_IGNOREUNKNOWN | ASSOCF_NOTRUNCATE;
			const std::wstring extension = GetExtension();

			DWORD length = 0;
			if (AssocQueryStringW(flags, what, extension.c_str(), L"open", nullptr, &length) != S_FALSE || length == 0)
				return {};

			std::wstring result(length, L'\0');
			if (FAILED(AssocQueryStringW(flags, what, extension.c_str(), L"open", result.data(), &length)))
				return {};

			result.resize(wcsnlen(result.c_str(), result.size()));
			return result;
		}

		// The program of a command line. For example, `"C:\Eagle\Eagle-Editor.exe" "--project=%1"` -> `C:\Eagle\Eagle-Editor.exe`
		Path GetProgram(const std::wstring& command)
		{
			const size_t begin = command.find_first_not_of(L" \t");
			if (begin == std::wstring::npos)
				return {};

			if (command[begin] == L'"')
			{
				const size_t end = command.find(L'"', begin + 1);
				return end == std::wstring::npos ? Path() : Path(command.substr(begin + 1, end - begin - 1));
			}

			// Not quoted. The path might contain spaces, so it ends after ".exe" if there is one
			std::wstring lowercase = command;
			std::transform(lowercase.begin(), lowercase.end(), lowercase.begin(), ::towlower);
			const size_t exe = lowercase.find(L".exe", begin);
			if (exe != std::wstring::npos)
				return Path(command.substr(begin, exe + 4 - begin));

			const size_t end = command.find_first_of(L" \t", begin);
			return Path(command.substr(begin, end == std::wstring::npos ? std::wstring::npos : end - begin));
		}

		// Sets a string value of HKEY_CURRENT_USER\Software\Classes\<subKey>, creating the key if needed.
		// @valueName. nullptr - the default value
		bool SetClassesValue(const std::wstring& subKey, const wchar_t* valueName, const std::wstring& value)
		{
			const std::wstring keyPath = L"Software\\Classes\\" + subKey;

			HKEY key = nullptr;
			LSTATUS status = RegCreateKeyExW(HKEY_CURRENT_USER, keyPath.c_str(), 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr, &key, nullptr);
			if (status != ERROR_SUCCESS)
			{
				EG_CORE_ERROR("[ProjectFileAssociation] Failed to create registry key HKEY_CURRENT_USER\\{}. Error: {}", Utils::AsString(Path(keyPath)), status);
				return false;
			}

			const DWORD size = DWORD((value.size() + 1) * sizeof(wchar_t));
			status = RegSetValueExW(key, valueName, 0, REG_SZ, (const BYTE*)value.c_str(), size);
			RegCloseKey(key);

			if (status != ERROR_SUCCESS)
			{
				EG_CORE_ERROR("[ProjectFileAssociation] Failed to write registry key HKEY_CURRENT_USER\\{}. Error: {}", Utils::AsString(Path(keyPath)), status);
				return false;
			}
			return true;
		}
	}

	Info Get()
	{
		Info info;

		const std::wstring command = QueryAssociation(ASSOCSTR_COMMAND);
		info.Program = GetProgram(command);
		if (info.Program.empty())
		{
			info.Status = AssociationStatus::NotRegistered;
			return info;
		}

		std::error_code error;
		if (info.Program.is_absolute() && !std::filesystem::exists(info.Program, error))
		{
			info.Status = AssociationStatus::ProgramMissing;
			return info;
		}

		const Path executable = Utils::GetExecutablePath();
		const bool bThisProgram = !executable.empty() && std::filesystem::equivalent(info.Program, executable, error);
		info.Status = bThisProgram ? AssociationStatus::ThisProgram : AssociationStatus::OtherProgram;
		return info;
	}

	bool Register()
	{
		const Path executable = Utils::GetExecutablePath();
		if (executable.empty())
		{
			EG_CORE_ERROR("[ProjectFileAssociation] Failed to get the path of the executable");
			return false;
		}

		constexpr const wchar_t* progID = L"eagleeditor";
		constexpr const wchar_t* typeName = L"Eagle Project";

		const std::wstring extension = GetExtension();
		const std::wstring progIDStr = progID;
		const std::wstring command = L"\"" + executable.wstring() + L"\" \"--project=%1\""; // The same as Win-SetupFileAssociation.bat

		// `OpenWithProgids` - so that it's listed in "Open with" too
		const bool bRegistered =
			SetClassesValue(progIDStr, nullptr, typeName) &&
			SetClassesValue(progIDStr + L"\\shell\\open\\command", nullptr, command) &&
			SetClassesValue(extension, nullptr, progIDStr) &&
			SetClassesValue(extension + L"\\OpenWithProgids", progIDStr.c_str(), L"");

		// Let Explorer know, so that it doesn't use the old association that it cached
		SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);

		if (bRegistered)
			EG_CORE_INFO("[ProjectFileAssociation] Registered '{}' to open {} files", Utils::AsString(executable), Project::GetExtension());

		return bRegistered;
	}
}
