#pragma once

#ifdef EG_PLATFORM_WINDOWS

extern Eagle::Application* Eagle::CreateApplication();

int main(int argc, char** argv)
{
	Eagle::Log::Init();
	EG_CORE_WARN("Initialized Log!");
	EG_INFO("Hello, Eagle-user!");

	Eagle::Application* app = Eagle::CreateApplication();
	app->Run();
	delete app;
}

#endif