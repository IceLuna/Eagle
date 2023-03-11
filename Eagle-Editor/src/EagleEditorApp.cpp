#include "EagleEditorApp.h"
#include "EditorLayer.h"

#include <Eagle/Core/Core.h>
#include <Eagle/Core/EntryPoint.h>

namespace Eagle
{
	EagleEditor::EagleEditor() : Application("Eagle Editor")
	{
		PushLayer(MakeRef<EditorLayer>());
	}

	Application* CreateApplication()
	{
		return new EagleEditor();
	}
}
