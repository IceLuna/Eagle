#pragma once

#ifdef EG_PLATFORM_WINDOWS

extern Eagle::Application* Eagle::CreateApplication(int argc, char** argv);

int main(int argc, char** argv)
{
	Eagle::Application* app = Eagle::CreateApplication(argc, argv);

	EG_CORE_INFO("Running Application!");
	app->Run();

	EG_CORE_INFO("Shutting down Application!");
	delete app;
	EG_CORE_INFO("Successfully shut down the app!");

	return 0;
}

#endif