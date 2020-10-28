#pragma once

#include "Core.h"
#include "Window.h"
#include "Eagle/Events/ApplicationEvent.h"
#include "Eagle/Layer.h"
#include "Eagle/LayerStack.h"

namespace Eagle
{
	class EAGLE_API Application
	{
	public:
		Application();
		virtual ~Application();

		void Run();

		void OnEvent(Event& e);

		inline Window& GetWindow() { return *m_Window; }
		static inline Application& Get() { return *s_Instance; }

		void PushLayer(Layer* layer);
		void PopLayer(Layer* layer);
		void PushLayout(Layer* layer);
		void PopLayout(Layer* layer);

	private:
		bool OnWindowClose(WindowCloseEvent& e);

	private:
		std::unique_ptr<Window> m_Window;
		LayerStack m_LayerStack;
		bool m_Running = true;

		static Application* s_Instance;
	};

	//To be defined in CLIENT
	Application* CreateApplication();
}

