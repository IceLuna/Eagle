#include "EagleEditorApp.h"
#include "EditorLayer.h"
#include "ProjectLayer.h"

#include <Eagle/Core/Core.h>
#include <Eagle/Core/EntryPoint.h>

namespace Eagle
{
	EagleEditor::EagleEditor(const ApplicationProperties& props) : Application(props)
	{
		const bool bHasProject = !Project::GetProjectPath().empty();
		Ref<Layer> layer;
		if (bHasProject)
			layer = MakeRef<EditorLayer>();
		else
			layer = MakeRef<ProjectLayer>();

		PushLayer(layer);
	}

	Application* CreateApplication(int argc, char** argv)
	{
		ApplicationProperties props;
		props.WindowProps.Title = "Eagle Editor";
		props.WindowProps.Width = 1600;
		props.WindowProps.Height = 900;
		props.WindowProps.VSync = true;
		props.WindowProps.Fullscreen = false;

		props.bGame = false;
		props.argc = argc;
		props.argv = argv;

		return new EagleEditor(props);
	}
}
