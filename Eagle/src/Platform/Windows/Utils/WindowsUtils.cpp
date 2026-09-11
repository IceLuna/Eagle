#include "egpch.h"

#include "Eagle/Core/Application.h"
#include "Eagle/Utils/PlatformUtils.h"

#include <commdlg.h>
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <shellapi.h>
#include <ShlObj_core.h>

namespace Eagle
{
	static bool s_InitializedCOM = false;
	static IFileOpenDialog* s_FolderOpenDialog = nullptr;

	namespace FileDialog
	{
		Path OpenFile(const wchar_t* filter, const Path& initialDir)
		{
			constexpr DWORD bufferSize = 512;

			OPENFILENAMEW ofn;
			WCHAR szFile[bufferSize] = { 0 };
			ZeroMemory(&ofn, sizeof(OPENFILENAMEW));
			ofn.lStructSize = sizeof(OPENFILENAMEW);
			ofn.hwndOwner = (HWND)Application::Get().GetWindow().GetNativeWindow();
			ofn.lpstrFile = szFile;
			ofn.nMaxFile = bufferSize;
			ofn.lpstrFilter = filter;
			ofn.nFilterIndex = 1;
			ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

			std::wstring initialDirStr;
			if (!initialDir.empty())
			{
				initialDirStr = initialDir.wstring();
				ofn.lpstrInitialDir = initialDirStr.c_str();
			}

			if (GetOpenFileNameW(&ofn) == TRUE)
			{
				return Path(ofn.lpstrFile);
			}
			return Path();
		}

		std::vector<Path> OpenFileMultiselect(const wchar_t* filter, const Path& initialDir)
		{
			constexpr DWORD bufferSize = 65536;

			OPENFILENAMEW ofn;
			WCHAR szFile[bufferSize] = { 0 };
			ZeroMemory(&ofn, sizeof(OPENFILENAMEW));
			ofn.lStructSize = sizeof(OPENFILENAMEW);
			ofn.hwndOwner = (HWND)Application::Get().GetWindow().GetNativeWindow();
			ofn.lpstrFile = szFile;
			ofn.nMaxFile = bufferSize;
			ofn.lpstrFilter = filter;
			ofn.nFilterIndex = 1;
			ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR | OFN_ALLOWMULTISELECT;

			std::wstring initialDirStr;
			if (!initialDir.empty())
			{
				initialDirStr = initialDir.wstring();
				ofn.lpstrInitialDir = initialDirStr.c_str();
			}

			if (GetOpenFileNameW(&ofn) == TRUE)
			{
				std::wstring directory = szFile;
				wchar_t* p = szFile + directory.length() + 1;
				if (*p == 0)
				{
					// Just one file
					return { directory };
				}

				std::vector<Path> paths;
				paths.reserve(4);
				while (*p)
				{
					std::wstring filename = p;
					std::wstring fullPath = directory + L"\\" + filename;
					paths.emplace_back(fullPath);
					p += filename.length() + 1;
				}

				return paths;
			}
			return {};
		}

		Path SaveFile(const wchar_t* filter, const Path& initialDir)
		{
			OPENFILENAMEW ofn;
			WCHAR szFile[256] = { 0 };
			ZeroMemory(&ofn, sizeof(OPENFILENAMEW));
			ofn.lStructSize = sizeof(OPENFILENAMEW);
			ofn.hwndOwner = HWND(Application::Get().GetWindow().GetNativeWindow());
			ofn.lpstrFile = szFile;
			ofn.nMaxFile = sizeof(szFile);
			ofn.lpstrFilter = filter;
			ofn.nFilterIndex = 1;
			ofn.lpstrDefExt = std::wcschr(filter, L'\0') + 1;
			ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

			std::wstring initialDirStr;
			if (!initialDir.empty())
			{
				initialDirStr = initialDir.is_absolute() ? initialDir.wstring() : std::filesystem::absolute(initialDir).wstring();
				ofn.lpstrInitialDir = initialDirStr.c_str();
			}

			if (GetSaveFileNameW(&ofn) == TRUE)
			{
				return Path(ofn.lpstrFile);
			}
			return Path();
		}
	
		Path OpenFolder()
		{
			if (!s_InitializedCOM)
			{
				if (HRESULT result = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE); !SUCCEEDED(result))
				{
					EG_CORE_ERROR("Failed to init COM. Error code {}", result);
					return "";
				}
				s_InitializedCOM = true;
			}

			if (!s_FolderOpenDialog)
			{
				if (HRESULT result = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL, IID_IFileOpenDialog, reinterpret_cast<void**>(&s_FolderOpenDialog));
					!SUCCEEDED(result))
				{
					EG_CORE_ERROR("Failed to init file open dialog. Error code {}", result);
					return "";
				}
				s_FolderOpenDialog->SetOptions(FOS_PICKFOLDERS | FOS_PATHMUSTEXIST);
			}

			Path resultPath;
			HRESULT result = s_FolderOpenDialog->Show(NULL);
			if (SUCCEEDED(result))
			{
				IShellItem* pItem;
				result = s_FolderOpenDialog->GetResult(&pItem);
				if (SUCCEEDED(result))
				{
					PWSTR pszFilePath;
					result = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);

					if (SUCCEEDED(result))
						resultPath = pszFilePath;
					pItem->Release();
				}
			}
			return resultPath;
		}
	}

	namespace FileSystem
	{
		bool Write(const Path& path, const DataBuffer& buffer)
		{
			std::error_code error;
			if (!std::filesystem::exists(path))
				std::filesystem::create_directories(path.parent_path(), error);

			std::ofstream stream(path, std::ios::binary | std::ios::trunc);

			if (!stream)
			{
				stream.close();
				return false;
			}

			stream.write((const char*)buffer.Data, buffer.Size);
			stream.close();
			return true;
		}
	
		ScopedDataBuffer Read(const Path& path)
		{
			if (std::filesystem::exists(path) == false)
			{
				EG_CORE_ERROR("Couldn't read a file, it doesn't exist: {}", path);
				return {};
			}
			std::ifstream stream(path, std::ios::binary | std::ios::ate);
			if (!stream)
			{
				EG_CORE_ERROR("Couldn't open a file: {}", path);
				return {};
			}

			const size_t size = (size_t)stream.tellg();
			if (size == 0)
			{
				EG_CORE_WARN("Empty file read, returning null: {}", path);
				return {};
			}
			stream.seekg(0, std::ios::beg);

			ScopedDataBuffer buffer(size);
			if (!stream.read((char*)buffer.Data(), buffer.Size()))
			{
				EG_CORE_ERROR("Failed to read a file: {}", path);
				return {};
			}

			return buffer;
		}
	
		std::string ReadText(const Path& path)
		{
			if (std::filesystem::exists(path) == false)
			{
				EG_CORE_ERROR("Couldn't read a file, it doesn't exist: {}", path);
				return {};
			}
			std::ifstream stream(path, std::ios::ate);
			if (!stream)
			{
				EG_CORE_ERROR("Failed to open a file file: {}", path);
				return {};
			}
			std::streampos end = stream.tellg();
			stream.seekg(0, std::ios::beg);
			size_t size = end - stream.tellg();

			std::string result, temp;
			result.reserve(size);

			while (std::getline(stream, temp))
			{
				result += temp;
				if (!stream.eof())
					result += '\n';
			}

			return result;
		}

		Path GetFullPath(const Path& path)
		{
			TCHAR  buffer[256] = TEXT("");
			TCHAR** lppPart = { NULL };

			GetFullPathName(path.c_str(), 256, buffer, lppPart);
			return Path(buffer);
		}
	}

	namespace Utils
	{
		void OpenInExplorer(const Path& path)
		{
			ShellExecute(NULL, L"open", path.wstring().c_str(), NULL, NULL, SW_SHOWDEFAULT);
		}

		void ShowInExplorer(const Path& path)
		{
			if (!s_InitializedCOM)
			{
				if (HRESULT result = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE); !SUCCEEDED(result))
				{
					EG_CORE_ERROR("Failed to init COM. Error code {}", result);
					return;
				}
				s_InitializedCOM = true;
			}

			std::thread thread([path]() {
				std::wstring pathString = std::filesystem::absolute(path).wstring();
				LPITEMIDLIST pidl = ILCreateFromPath(pathString.c_str());
				if (pidl)
				{
					SHOpenFolderAndSelectItems(pidl, 0, 0, 0); //OFASI_OPENDESKTOP
					ILFree(pidl);
				}
			});
			thread.detach();
		}
	
		void OpenLink(const Path& path)
		{
			ShellExecute(0, 0, path.wstring().c_str(), 0, 0, SW_SHOW);
		}

		bool WereScriptsRebuild()
		{
			static HANDLE eagleEvent = CreateEventA(NULL, false, false, "Eagle-Editor");
			if (eagleEvent == 0)
				return false;

			return WaitForSingleObject(eagleEvent, 0) == WAIT_OBJECT_0;
		}

		bool IsSSE2Supported()
		{
			return IsProcessorFeaturePresent(PF_XMMI64_INSTRUCTIONS_AVAILABLE);
		}

		int Execute(const Path& exePath, const std::string& args)
		{
			const std::string cmd = "call " + Utils::AsString(exePath) + ' ' + args;
			return system(cmd.c_str());
		}
	}
	
	namespace Dialog
	{
		bool YesNoQuestion(const std::string& title, const std::string& message)
		{
			std::wstring wideTitle= std::wstring(title.begin(), title.end());
			std::wstring wideMessage = std::wstring(message.begin(), message.end());
			int answer = MessageBox(NULL, wideMessage.c_str(), wideTitle.c_str(), MB_YESNO | MB_ICONQUESTION);
			return answer == IDYES;
		}
	}
}
