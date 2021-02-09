#include "egpch.h"

#include "Eagle/Core/Application.h"
#include "Eagle/Utils/PlatformUtils.h"

#include <commdlg.h>
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

namespace Eagle
{
	std::string FileDialog::OpenFile(const char* filter)
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

	std::string FileDialog::SaveFile(const char* filter)
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
