#pragma once

#include "Eagle/Core/Window.h"
#include "Eagle/Renderer/GraphicsContext.h"

struct GLFWwindow;

namespace Eagle
{
	class WindowsWindow : public Window
	{
	public:
		WindowsWindow(const WindowProps& props);
		virtual ~WindowsWindow();

		void OnUpdate() override;

		inline uint32_t GetWidth() const override { return m_Data.Width; }
		inline uint32_t GetHeight() const override { return m_Data.Height; }

		inline virtual void* GetNativeWindow() const override { return m_Window; }

		//Window attributes
		inline void SetEventCallback(const EventCallbackFn& callback) { m_Data.EventCallback = callback; }
		void SetVSync(bool enable) override;
		void SetFocus(bool focus) override;
		bool IsVSync() const override { return m_Data.VSync; }

	private:
		virtual void Init(const WindowProps& props);
		virtual void Shutdown();

		virtual void SetupGLFWCallbacks() const;

	private:
		GLFWwindow* m_Window;
		Ref<GraphicsContext> m_Context;

		struct WindowData
		{
			std::string Title;
			uint32_t Width, Height;
			bool VSync;

			EventCallbackFn EventCallback;
		};

		WindowData m_Data;
	};
}