#include "EagleEditorApp.h"
#include "EditorLayer.h"

#include <Eagle/Core/Core.h>
#include <Eagle/Core/EntryPoint.h>

namespace Eagle
{
	EagleEditor::EagleEditor() : Application("Eagle Editor")
	{
		PushLayer(new EditorLayer());
	}

	void EagleEditor::OnEvent(Event& e)
	{
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<WindowCloseEvent>(EG_BIND_FN(EagleEditor::OnWindowClose));
		dispatcher.Dispatch<WindowResizeEvent>(EG_BIND_FN(EagleEditor::OnWindowResize));

		for (auto it = m_LayerStack.rbegin(); it != m_LayerStack.rend(); ++it)
		{
			if (e.Handled)
				break;

			(*it)->OnEvent(e);
		}
	}

	bool EagleEditor::OnWindowResize(WindowResizeEvent& e)
	{
		const uint32_t width = e.GetWidth();
		const uint32_t height = e.GetHeight();

		if (width == 0 || height == 0)
		{
			m_Minimized = true;
		}
		else
		{
			m_Minimized = false;
		}

		//Disabled so EditorLayer can control vieportsize
		//Renderer::WindowResized(width, height);

		return false;
	}

	Application* CreateApplication()
	{
		return new EagleEditor();
	}
}
