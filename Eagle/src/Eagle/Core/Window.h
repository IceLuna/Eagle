#pragma once

#include "Core.h"
#include "Eagle/Events/Event.h"

#include <string>
#include <filesystem>
#include <functional>
#include <glm/glm.hpp>

namespace Eagle
{
	struct WindowProps
	{
		std::string Title;
		uint32_t Width;
		uint32_t Height;
		bool Fullscreen;

		WindowProps(const std::string& title = "Eagle Engine", uint32_t width = 1280, uint32_t height = 720, bool fullscreen = false)
			: Title(title), Width(width), Height(height), Fullscreen(fullscreen) {}
	};

	//Window interface
	class Window
	{
	public:
		using EventCallbackFn = std::function<void(Event&)>;

		virtual ~Window() = default;

		virtual void OnUpdate() = 0;

		inline virtual uint32_t GetWidth()  const = 0;
		inline virtual uint32_t GetHeight() const = 0;

		inline virtual void* GetNativeWindow() const = 0;

		//Window attributes
		virtual void SetEventCallback(const EventCallbackFn& callback) = 0;
		virtual void SetVSync(bool enable) = 0;
		virtual void SetFocus(bool focus) = 0;
		virtual void SetWindowSize(int width, int height) = 0;
		virtual void SetWindowTitle(const std::string& title) = 0;
		virtual void SetWindowPos(int x, int y) = 0;
		virtual void SetWindowIcon(const std::filesystem::path& iconPath) = 0;

		virtual bool IsVSync() const = 0;
		virtual glm::vec2 GetWindowSize() const = 0;
		virtual glm::vec2 GetWindowPos() const = 0;
		virtual const std::string& GetWindowTitle() const = 0;

		static Ref<Window> Create(const WindowProps& props = WindowProps());

	public:
		static float s_HighDPIScaleFactor;
	};
}