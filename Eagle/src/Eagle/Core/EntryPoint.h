#pragma once

#ifdef EG_PLATFORM_WINDOWS

extern Eagle::Application* Eagle::CreateApplication();

int main(int argc, char** argv)
{
	Eagle::Log::Init();

	EG_CORE_INFO("Creating Application!");
	Eagle::Application* app = Eagle::CreateApplication();

	EG_CORE_INFO("Running Application!");
	app->Run();

	EG_CORE_INFO("Shutting down Application!");
	delete app;

	return 0;
}

#endif