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
		virtual void SetVSync(bool enable) override;
		virtual void SetFocus(bool focus) override;
		virtual void SetWindowSize(int width, int height) override;
		virtual void SetWindowPos(int x, int y) override;

		virtual bool IsVSync() const override { return m_Data.VSync; }
		virtual glm::vec2 GetWindowSize() const override;
		virtual glm::vec2 GetWindowPos() const override;

	public:
		struct WindowData
		{
			std::string Title;
			uint32_t Width, Height;
			bool VSync;

			EventCallbackFn EventCallback;
		};

	private:
		virtual void Init(const WindowProps& props);
		virtual void Shutdown();

		virtual void SetupGLFWCallbacks() const;

	private:
		GLFWwindow* m_Window;
		Ref<GraphicsContext> m_Context;
		WindowData m_Data;
	};
}
