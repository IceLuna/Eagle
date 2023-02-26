#pragma once

#include "Eagle/Core/Window.h"
#include "Eagle/Renderer/RendererContext.h"
#include "Platform/Vulkan/VulkanSwapchain.h"

struct GLFWwindow;

namespace Eagle
{
	class WindowsWindow : public Window
	{
	public:
		WindowsWindow(const WindowProps& props);
		virtual ~WindowsWindow();

		void ProcessEvents() override;

		inline virtual void* GetNativeWindow() const override { return m_Window; }

		//Window attributes
		inline void SetEventCallback(const EventCallbackFn& callback) { m_WindowData.EventCallback = callback; }
		virtual void SetVSync(bool enable) override;
		virtual void SetFocus(bool focus) override;
		virtual void SetWindowSize(int width, int height) override;
		virtual void SetWindowMaximized(bool bMaximize) override;
		virtual void SetWindowPos(int x, int y) override;
		virtual void SetWindowTitle(const std::string& title) override;
		virtual void SetWindowIcon(const Path& iconPath) override;

		virtual glm::vec2 GetWindowSize() const override;
		virtual bool IsMaximized() const override;
		virtual glm::vec2 GetWindowPos() const override;
		virtual Ref<VulkanSwapchain>& GetSwapchain() override { return m_Swapchain; }

	private:
		virtual void Shutdown();
		virtual void SetupGLFWCallbacks() const;

		struct WindowData
		{
			WindowProps* Props;
			EventCallbackFn EventCallback;
		};

	private:
		WindowData m_WindowData;
		GLFWwindow* m_Window;
		Ref<VulkanSwapchain> m_Swapchain;
	};
}
