#include "GameApp.h"
#include "GameLayer.h"

#include <Eagle/Core/Core.h>
#include <Eagle/Core/EntryPoint.h>

namespace Eagle
{
	GameApp::GameApp(const ApplicationProperties& props) : Application(props)
	{
		PushLayer(MakeRef<GameLayer>());
	}

	Application* CreateApplication(int argc, char** argv)
	{
		ApplicationProperties props;
		props.WindowProps.Title = "Game name";
		props.WindowProps.Width = 1600;
		props.WindowProps.Height = 900;
		props.WindowProps.VSync = true;
		props.WindowProps.Fullscreen = false;

		props.bGame = true;
		props.argc = argc;
		props.argv = argv;

		return new GameApp(props);
	}
}
