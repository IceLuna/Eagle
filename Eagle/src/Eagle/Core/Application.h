#pragma once

#include "Eagle/Core/Core.h"
#include "Eagle/Core/Window.h"
#include "Eagle/Events/ApplicationEvent.h"
#include "Eagle/Core/Layer.h"
#include "Eagle/Core/LayerStack.h"

#include "Eagle/ImGui/ImGuiLayer.h"

int main(int argc, char** argv);

namespace Eagle
{
	class Application
	{
	public:
		Application();
		Application(const Application&);
		virtual ~Application();

		static inline Application& Get() { return *s_Instance; }
		inline Window& GetWindow() { return *m_Window; }
		inline bool IsMinimized() const { return m_Minimized; }

		void PushLayer(Layer* layer);
		bool PopLayer(Layer* layer);
		void PushLayout(Layer* layer);
		bool PopLayout(Layer* layer);

	protected:
		bool OnWindowClose(WindowCloseEvent& e);
		bool OnWindowResize(WindowResizeEvent& e);

		void Run();
		void OnEvent(Event& e);

		friend int ::main(int argc, char** argv);

	protected:
		Ref<Window> m_Window;
		ImGuiLayer* m_ImGuiLayer;
		LayerStack m_LayerStack;
		bool m_Running = true;
		bool m_Minimized = false;

		static Application* s_Instance;
	};

	//To be defined in CLIENT
	Application* CreateApplication();
}

