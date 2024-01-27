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
		Path OpenFile(const wchar_t* filter)
		{
			OPENFILENAMEW ofn;
			WCHAR szFile[256] = { 0 };
			WCHAR currentDir[256] = { 0 };
			ZeroMemory(&ofn, sizeof(OPENFILENAME));
			ofn.lStructSize = sizeof(OPENFILENAME);
			ofn.hwndOwner = (HWND)Application::Get().GetWindow().GetNativeWindow();
			ofn.lpstrFile = szFile;
			ofn.nMaxFile = sizeof(szFile);
			ofn.lpstrFilter = filter;
			ofn.nFilterIndex = 1;
			ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

			if (GetCurrentDirectoryW(256, currentDir))
				ofn.lpstrInitialDir = currentDir;

			if (GetOpenFileNameW(&ofn) == TRUE)
			{
				return Path(ofn.lpstrFile);
			}
			return Path();
		}

		Path SaveFile(const wchar_t* filter)
		{
			OPENFILENAMEW ofn;
			WCHAR szFile[256] = { 0 };
			WCHAR currentDir[256] = { 0 };
			ZeroMemory(&ofn, sizeof(OPENFILENAME));
			ofn.lStructSize = sizeof(OPENFILENAME);
			ofn.hwndOwner = HWND(Application::Get().GetWindow().GetNativeWindow());
			ofn.lpstrFile = szFile;
			ofn.nMaxFile = sizeof(szFile);
			ofn.lpstrFilter = filter;
			ofn.nFilterIndex = 1;
			ofn.lpstrDefExt = std::wcschr(filter, L'\0') + 1;
			ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

			if (GetCurrentDirectoryW(256, currentDir))
				ofn.lpstrInitialDir = currentDir;

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
			if (!std::filesystem::exists(path))
				std::filesystem::create_directories(path.parent_path());

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
	
		DataBuffer Read(const Path& path)
		{
			if (std::filesystem::exists(path) == false)
			{
				EG_CORE_ERROR("Couldn't read a file, it doesn't exist: {}", path);
				return {};
			}
			std::ifstream stream(path, std::ios::binary | std::ios::ate);

			std::streampos end = stream.tellg();
			stream.seekg(0, std::ios::beg);
			size_t size = end - stream.tellg();
			EG_CORE_ASSERT(size != 0, "Empty file");

			DataBuffer buffer;
			buffer.Allocate(size);
			stream.read((char*)buffer.Data, buffer.Size);

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
			const std::string cmd = "call " + exePath.u8string() + ' ' + args;
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
