#pragma once

#include "Event.h"

#include <sstream>

namespace Eagle
{
	class WindowResizeEvent : public Event
	{
	public:
		EVENT_CLASS_CATEGORY(EventCategory::Application)
		EVENT_CLASS_TYPE(WindowResize, uint,uint)

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

		std::array<void*, 2> GetData() override { return { &m_Width, &m_Height }; }

	private:
		uint32_t m_Width, m_Height;
	};

	class WindowCloseEvent : public Event
	{
	public:
		EVENT_CLASS_CATEGORY(EventCategory::Application)
		EVENT_CLASS_TYPE(WindowClose)

		WindowCloseEvent() = default;
	};

	class WindowFocusedEvent : public Event
	{
	public:
		EVENT_CLASS_CATEGORY(EventCategory::Application)
		EVENT_CLASS_TYPE(WindowFocused, bool);

		WindowFocusedEvent(bool bFocused)
			: bFocused(bFocused) {}

		bool IsFocused() const { return bFocused; }

		std::string ToString() const override
		{
			return bFocused ? "WindowFocusedEvent: true" : "WindowFocusedEvent: false";
		}

		std::array<void*, 2> GetData() override { return { &bFocused, nullptr }; }

	private:
		bool bFocused;
	};
}
