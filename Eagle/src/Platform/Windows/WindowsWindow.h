#pragma once

#include "Eagle/Core/Window.h"
#include "Eagle/Renderer/RendererContext.h"
#include "Platform/Vulkan/VulkanSwapchain.h"

namespace Eagle
{
	class WindowsWindow : public Window
	{
	public:
		WindowsWindow(const WindowProperties& props);
		virtual ~WindowsWindow();

		void ProcessEvents() override;

		virtual void* GetNativeWindow() const override;

		inline void SetEventCallback(const EventCallbackFn& callback) { m_WindowData.EventCallback = callback; }
		virtual void SetVSync(bool enable) override;
		virtual void SetFocus(bool focus) override;
		virtual void SetWindowSize(int width, int height) override;
		virtual void SetWindowMaximized(bool bMaximize) override;
		virtual void SetWindowPos(int x, int y) override;
		virtual void SetWindowTitle(const std::string& title) override;
		virtual void SetWindowIcon(const Path& iconPath) override;
		virtual void SetFullscreen(bool bFullscreen) override;
		virtual void Minimize() override;
		virtual void Show(bool bCloaked) override;
		virtual void Uncloak() override;
		virtual void Hide() override;
		virtual bool IsVisible() const override { return m_bShown; }
		virtual WindowPlacement GetWindowPlacement() const override;
		virtual void SetWindowPlacement(const WindowPlacement& placement) override;
		virtual void SetTitleBarHitTestCallback(const WindowHitTestCallback& callback) override { m_WindowData.TitleBarHitTest = callback; }
		virtual void SetModalLoopCallback(const ModalLoopCallbackFn& callback) override { m_ModalLoopCallback = callback; }

		virtual glm::vec2 GetWindowSize() const override;
		virtual bool IsMaximized() const override;
		virtual glm::vec2 GetWindowPos() const override;
		virtual Ref<VulkanSwapchain>& GetSwapchain() override { return m_Swapchain; }

	private:
		virtual void Shutdown();
		virtual void SetupGLFWCallbacks() const;

		void ApplyFullscreen(bool bFullscreen);
		void SetCloaked(bool bCloaked);
		WindowPlacement& GetPendingPlacement();

		void CreateModalLoopHandler();
		void DestroyModalLoopHandler();
		void OnWindowMessageProcessed(const CWPRETSTRUCT& message);
		void RunModalLoopFrame();
		static LRESULT CALLBACK ModalLoopWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
		static LRESULT CALLBACK ModalLoopHookProc(int code, WPARAM wParam, LPARAM lParam);

		struct WindowData
		{
			WindowProperties* Props = nullptr;
			float* DPIScale = nullptr;
			EventCallbackFn EventCallback = nullptr;
			WindowHitTestCallback TitleBarHitTest = nullptr;
		};

	private:
		WindowData m_WindowData{};
		Ref<VulkanSwapchain> m_Swapchain;

		// Keep the app running during the OS's modal loops, see `CreateModalLoopHandler()`
		void* m_ModalLoopWindow = nullptr; // HWND. Hidden
		void* m_ModalLoopHook = nullptr;   // HHOOK
		ModalLoopCallbackFn m_ModalLoopCallback;
		uint32_t m_ModalLoopDepth = 0;
		bool m_bModalLoopFramePosted = false;
		bool m_bInModalLoopCallback = false;

		void* m_BigIcon = nullptr;   // HICON
		void* m_SmallIcon = nullptr; // HICON
		std::optional<WindowPlacement> m_PendingPlacement; // Applied when the window is shown, or when it leaves fullscreen
		bool m_bShown = false;
		bool m_bCloaked = false;
	};
}
