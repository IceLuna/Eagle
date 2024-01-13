#include "EagleEditorApp.h"
#include "EditorLayer.h"
#include "ProjectLayer.h"

#include <Eagle/Core/Core.h>
#include <Eagle/Core/EntryPoint.h>

namespace Eagle
{
	EagleEditor::EagleEditor(int argc, char** argv) : Application("Eagle Editor", argc, argv)
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
		return new EagleEditor(argc, argv);
	}
}
