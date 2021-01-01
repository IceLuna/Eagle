#pragma once

#include "Core.h"
#include "Events/Event.h"

#include <string>
#include <functional>

namespace Eagle
{
	struct WindowProps
	{
		std::string Title;
		uint32_t Width;
		uint32_t Height;

		WindowProps(const std::string& title = "Eagle Engine", uint32_t width = 1280, uint32_t height = 720)
			: Title(title), Width(width), Height(height) {}
	};

	//Window interface
	class EAGLE_API Window
	{
	public:
		using EventCallbackFn = std::function<void(Event&)>;

		virtual ~Window() {}

		virtual void OnUpdate() = 0;

		inline virtual uint32_t GetWidth()  const = 0;
		inline virtual uint32_t GetHeight() const = 0;

		inline virtual void* GetNativeWindow() const = 0;

		//Window attributes
		virtual void SetEventCallback(const EventCallbackFn& callback) = 0;
		virtual void SetVSync(bool enable) = 0;
		virtual bool IsVSync() const = 0;

		static Ref<Window> Create(const WindowProps& props = WindowProps());
	};
}