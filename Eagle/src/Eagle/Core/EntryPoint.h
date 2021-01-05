#pragma once

#ifdef EG_PLATFORM_WINDOWS

extern Eagle::Application* Eagle::CreateApplication();

int main(int argc, char** argv)
{
	Eagle::Log::Init();

	EG_PROFILE_BEGIN_SESSION("Startup", "EagleProfile-Startup.json");
	Eagle::Application* app = Eagle::CreateApplication();
	EG_PROFILE_END_SESSION();

	EG_PROFILE_BEGIN_SESSION("Runtime", "EagleProfile-Runtime.json");
	app->Run();
	EG_PROFILE_END_SESSION();

	EG_PROFILE_BEGIN_SESSION("Shutdown", "EagleProfile-Shutdown.json");
	delete app;
	EG_PROFILE_END_SESSION();

	return 0;
}

#endif