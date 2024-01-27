#pragma once

#include "Core.h"
#include "Eagle/Events/Event.h"
#include "Eagle/Renderer/RendererContext.h"

#include <functional>

struct GLFWwindow;

namespace Eagle
{
	struct WindowProperties
	{
		std::string Title = "Untitled";
		uint32_t Width = 1280;
		uint32_t Height = 720;
		bool VSync = true;
		bool Fullscreen = false;

		WindowProperties() = default;
		WindowProperties(const std::string& title, uint32_t width, uint32_t height, bool bVSync, bool bFullscreen)
			: Title(title), Width(width), Height(height), VSync(bVSync), Fullscreen(bFullscreen) {}
	};

	class VulkanSwapchain;

	//Window interface
	class Window
	{
	public:
		using EventCallbackFn = std::function<void(Event&)>;

		Window(const WindowProperties& props) : m_Props(props) {}
		virtual ~Window() = default;

		virtual void ProcessEvents() = 0;

		uint32_t GetWidth() const { return m_Props.Width; }
		uint32_t GetHeight() const { return m_Props.Height; }
		bool IsVSync() const { return m_Props.VSync; }

		virtual void* GetNativeWindow() const = 0;
		GLFWwindow* GetGLFWWindow() const { return m_Window; }

		//Window attributes
		virtual void SetEventCallback(const EventCallbackFn& callback) = 0;
		virtual void SetVSync(bool enable) = 0;
		virtual void SetFocus(bool focus) = 0;
		virtual void SetWindowSize(int width, int height) = 0;
		virtual void SetWindowMaximized(bool bMaximize) = 0;
		virtual void SetWindowTitle(const std::string& title) = 0;
		virtual void SetWindowPos(int x, int y) = 0;
		virtual void SetWindowIcon(const Path& iconPath) = 0;
		virtual void SetFullscreen(bool bFullscreen) = 0;

		virtual glm::vec2 GetWindowSize() const = 0;
		virtual bool IsMaximized() const = 0;
		virtual glm::vec2 GetWindowPos() const = 0;
		const std::string& GetWindowTitle() const { return m_Props.Title; }
		virtual Ref<VulkanSwapchain>& GetSwapchain() = 0;
		bool IsFullscreen() const { return m_Props.Fullscreen; }

		static Ref<Window> Create(const WindowProperties& props);

	public:
		static float s_HighDPIScaleFactor;

	protected:
		WindowProperties m_Props;
		GLFWwindow* m_Window;
	};
}