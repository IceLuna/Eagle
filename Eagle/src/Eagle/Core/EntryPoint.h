#pragma once

#ifdef EG_PLATFORM_WINDOWS

extern std::unique_ptr<Eagle::Application> Eagle::CreateApplication(int argc, char** argv);

#ifdef EG_RELEASE
#define EG_WINMAIN
#endif

#ifdef EG_WINMAIN
#include <shellapi.h>
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
#else
int main(int argc, char** argv)
#endif
{
#ifdef EG_WINMAIN
    // Get command line as wide-char argv (includes exe path as argv[0])
    int argc = 0;
    LPWSTR* wargv = CommandLineToArgvW(GetCommandLineW(), &argc);

    // Convert wide strings to narrow (UTF-8) char* array
    std::vector<std::string> argStorage(argc);
    std::vector<char*> argvVec(argc);

    for (int i = 0; i < argc; i++)
    {
        int size = WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, nullptr, 0, nullptr, nullptr);
        argStorage[i].resize(size - 1); // size includes null terminator
        WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, argStorage[i].data(), size, nullptr, nullptr);
        argvVec[i] = argStorage[i].data();
    }
    char** argv = argvVec.data();
#endif

	Eagle::Log::Init();

	EG_CORE_INFO("Creating Application!");
	std::unique_ptr<Eagle::Application> app = Eagle::CreateApplication(argc, argv);

	EG_CORE_INFO("Running {}!", app->GetWindowProps().Title);
	app->Run();

	EG_CORE_INFO("Shutting down {}!", app->GetWindowProps().Title);
	app.reset();
	Eagle::Log::Destroy();

	return 0;
}

#endif
