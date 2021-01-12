#include "EagleEditorApp.h"
#include "EditorLayer.h"

#include <Eagle/Core/EntryPoint.h>
#include <Eagle/Core/Core.h>

namespace Eagle
{
	EagleEditor::EagleEditor() : Application("Eagle Editor")
	{
		PushLayer(new EditorLayer());
		m_Window->SetVSync(true);
	}

	Application* CreateApplication()
	{
		return new EagleEditor();
	}
}
