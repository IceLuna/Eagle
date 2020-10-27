#pragma once

#include "Event.h"

#include <sstream>

namespace Eagle
{
	using uint32_t = unsigned int;

	class EAGLE_API WindowResizeEvent : public Event
	{
	public:
		EVENT_CLASS_CATEGORY(EventCategoryApplication)
		EVENT_CLASS_TYPE(WindowResize)

		WindowResizeEvent(uint32_t width, uint32_t height)
			: m_Width(width), m_Height(height) {}

		uint32_t GetWidth() const { return m_Width; }
		uint32_t GetHeight() const { return m_Height; }

		std::string ToString() const override
		{
			std::stringstream ss;
			ss << "WindowResizeEvent: " << m_Width << ", " << m_Height;
			return ss.str();
		}

	private:
		uint32_t m_Width, m_Height;
	};

	class EAGLE_API WindowCloseEvent : public Event
	{
	public:
		EVENT_CLASS_CATEGORY(EventCategoryApplication)
		EVENT_CLASS_TYPE(WindowClose)

		WindowCloseEvent() = default;

		std::string ToString() const override
		{
			return "WindowCloseEvent";
		}
	};

	class EAGLE_API AppTickEvent : public Event
	{
	public:
		EVENT_CLASS_CATEGORY(EventCategoryApplication)
		EVENT_CLASS_TYPE(AppTick)

		AppTickEvent() = default;
	};

	class EAGLE_API AppUpdateEvent : public Event
	{
	public:
		EVENT_CLASS_CATEGORY(EventCategoryApplication)
		EVENT_CLASS_TYPE(AppUpdate)

		AppUpdateEvent() = default;
	};

	class EAGLE_API AppRenderEvent : public Event
	{
	public:
		EVENT_CLASS_CATEGORY(EventCategoryApplication)
		EVENT_CLASS_TYPE(AppRender)

		AppRenderEvent() = default;
	};
}
