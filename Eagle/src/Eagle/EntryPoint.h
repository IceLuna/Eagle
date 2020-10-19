#pragma once

#ifdef EG_PLATFORM_WINDOWS

extern Eagle::Application* Eagle::CreateApplication();

int main(int argc, char** argv)
{
	Eagle::Application* app = Eagle::CreateApplication();
	app->Run();
	delete app;
}

#endif