#pragma once

#ifdef EG_PLATFORM_WINDOWS

extern std::unique_ptr<Eagle::Application> Eagle::CreateApplication(int argc, char** argv);

int main(int argc, char** argv)
{
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