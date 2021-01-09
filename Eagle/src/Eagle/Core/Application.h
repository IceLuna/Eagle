#pragma once

#include "Eagle/Core/Core.h"
#include "Eagle/Core/Window.h"
#include "Eagle/Events/ApplicationEvent.h"
#include "Eagle/Core/Layer.h"
#include "Eagle/Core/LayerStack.h"

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
		inline bool IsMinimized() const { return m_Minimized; }

		void PushLayer(Layer* layer);
		void PopLayer(Layer* layer);
		void PushLayout(Layer* layer);
		void PopLayout(Layer* layer);

		void SetShouldClose(bool close);

	protected:
		bool OnWindowClose(WindowCloseEvent& e);
		bool OnWindowResize(WindowResizeEvent& e);

	protected:
		Ref<Window> m_Window;
		ImGuiLayer* m_ImGuiLayer;
		LayerStack m_LayerStack;
		WindowProps m_WindowProps;
		bool m_Running = true;
		bool m_Minimized = false;

		static Application* s_Instance;
	};

	//To be defined in CLIENT
	Application* CreateApplication();
}

