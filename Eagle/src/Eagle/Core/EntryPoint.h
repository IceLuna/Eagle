#pragma once

#ifdef EG_PLATFORM_WINDOWS

extern std::unique_ptr<Eagle::Application> Eagle::CreateApplication(int argc, char** argv);

int main(int argc, char** argv)
{
	std::unique_ptr<Eagle::Application> app = Eagle::CreateApplication(argc, argv);

	EG_CORE_INFO("Running Application!");
	app->Run();

	EG_CORE_INFO("Shutting down Application!");
	app.reset();

	return 0;
}

#endif