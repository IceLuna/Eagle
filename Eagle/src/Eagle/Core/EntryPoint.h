#pragma once

#ifdef EG_PLATFORM_WINDOWS

extern Eagle::Application* Eagle::CreateApplication();

int main(int argc, char** argv)
{
	Eagle::Log::Init();
	EG_CORE_INFO("Initialized Log!");
	EG_CORE_INFO("Hello, Eagle-user!");

	Eagle::Application* app = Eagle::CreateApplication();
	app->Run();
	delete app;

	return 0;
}

#endif