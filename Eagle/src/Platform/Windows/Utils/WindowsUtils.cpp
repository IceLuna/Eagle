#include "egpch.h"

#include "Eagle/Core/Application.h"
#include "Eagle/Utils/PlatformUtils.h"

#include <commdlg.h>
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

namespace Eagle
{
	namespace FileDialog
	{
		std::filesystem::path OpenFile(const wchar_t* filter)
		{
			OPENFILENAMEW ofn;
			WCHAR szFile[256] = { 0 };
			WCHAR currentDir[256] = { 0 };
			ZeroMemory(&ofn, sizeof(OPENFILENAME));
			ofn.lStructSize = sizeof(OPENFILENAME);
			ofn.hwndOwner = glfwGetWin32Window((GLFWwindow*)Application::Get().GetWindow().GetNativeWindow());
			ofn.lpstrFile = szFile;
			ofn.nMaxFile = sizeof(szFile);
			ofn.lpstrFilter = filter;
			ofn.nFilterIndex = 1;
			ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

			if (GetCurrentDirectoryW(256, currentDir))
				ofn.lpstrInitialDir = currentDir;

			if (GetOpenFileNameW(&ofn) == TRUE)
			{
				return std::filesystem::path(ofn.lpstrFile);
			}
			return std::filesystem::path();
		}

		std::filesystem::path SaveFile(const wchar_t* filter)
		{
			OPENFILENAMEW ofn;
			WCHAR szFile[256] = { 0 };
			WCHAR currentDir[256] = { 0 };
			ZeroMemory(&ofn, sizeof(OPENFILENAME));
			ofn.lStructSize = sizeof(OPENFILENAME);
			ofn.hwndOwner = glfwGetWin32Window((GLFWwindow*)Application::Get().GetWindow().GetNativeWindow());
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
				return std::filesystem::path(ofn.lpstrFile);
			}
			return std::filesystem::path();
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
