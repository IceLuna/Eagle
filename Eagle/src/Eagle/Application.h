#pragma once

#include "Core.h"
#include "Window.h"
#include "Eagle/Events/ApplicationEvent.h"
#include "Eagle/Layer.h"
#include "Eagle/LayerStack.h"

#include "Eagle/ImGui/ImGuiLayer.h"

namespace Eagle
{
	class EAGLE_API Application
	{
	public:
		Application();
		Application(const Application&) = delete;
		virtual ~Application();

		void Run();

		void OnEvent(Event& e);

		static inline Application& Get() { return *s_Instance; }
		inline Window& GetWindow() { return *m_Window; }

		void PushLayer(Layer* layer);
		void PopLayer(Layer* layer);
		void PushLayout(Layer* layer);
		void PopLayout(Layer* layer);

	protected:
		bool OnWindowClose(WindowCloseEvent& e);

	protected:
		std::unique_ptr<Window> m_Window;
		ImGuiLayer* m_ImGuiLayer;
		LayerStack m_LayerStack;
		bool m_Running = true;

		static Application* s_Instance;
	};

	//To be defined in CLIENT
	Application* CreateApplication();
}

