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
		std::string OpenFile(const char* filter)
		{
			OPENFILENAMEA ofn;
			CHAR szFile[256] = { 0 };
			CHAR currentDir[256] = { 0 };
			ZeroMemory(&ofn, sizeof(OPENFILENAME));
			ofn.lStructSize = sizeof(OPENFILENAME);
			ofn.hwndOwner = glfwGetWin32Window((GLFWwindow*)Application::Get().GetWindow().GetNativeWindow());
			ofn.lpstrFile = szFile;
			ofn.nMaxFile = sizeof(szFile);
			ofn.lpstrFilter = filter;
			ofn.nFilterIndex = 1;
			ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

			if (GetCurrentDirectoryA(256, currentDir))
				ofn.lpstrInitialDir = currentDir;

			if (GetOpenFileNameA(&ofn) == TRUE)
			{
				return ofn.lpstrFile;
			}
			return std::string();
		}

		std::string SaveFile(const char* filter)
		{
			OPENFILENAMEA ofn;
			CHAR szFile[256] = { 0 };
			CHAR currentDir[256] = { 0 };
			ZeroMemory(&ofn, sizeof(OPENFILENAME));
			ofn.lStructSize = sizeof(OPENFILENAME);
			ofn.hwndOwner = glfwGetWin32Window((GLFWwindow*)Application::Get().GetWindow().GetNativeWindow());
			ofn.lpstrFile = szFile;
			ofn.nMaxFile = sizeof(szFile);
			ofn.lpstrFilter = filter;
			ofn.nFilterIndex = 1;
			ofn.lpstrDefExt = std::strchr(filter, '\0') + 1;
			ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

			if (GetCurrentDirectoryA(256, currentDir))
				ofn.lpstrInitialDir = currentDir;

			if (GetSaveFileNameA(&ofn) == TRUE)
			{
				return ofn.lpstrFile;
			}
			return std::string();
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
