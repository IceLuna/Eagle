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
		EVENT_CLASS_TYPE(WindowClose, bool)

		WindowCloseEvent(bool bQuitGame)
			: bQuitGame(bQuitGame) {}

		bool IsQuitGame() const { return bQuitGame; }

		std::string ToString() const override
		{
			return bQuitGame ? "WindowCloseEvent. Quit Game: true" : "WindowCloseEvent. Quit Game: false";
		}

		std::array<void*, 2> GetData() override { return { &bQuitGame, nullptr }; }

	private:
		bool bQuitGame; // `True` if it was requested from C# scripts
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

	class WindowContentScaleEvent : public Event
	{
	public:
		EVENT_CLASS_CATEGORY(EventCategory::Application)
		EVENT_CLASS_TYPE(WindowContentScale, single,single)

		WindowContentScaleEvent(float xscale, float yscale)
			: m_ScaleX(xscale), m_ScaleY(yscale) {
		}

		float GetScaleX() const { return m_ScaleX; }
		float GetScaleY() const { return m_ScaleY; }

		std::string ToString() const override
		{
			std::stringstream ss;
			ss << "WindowContentScaleEvent: " << m_ScaleX << ", " << m_ScaleY;
			return ss.str();
		}

		std::array<void*, 2> GetData() override { return { &m_ScaleX, &m_ScaleY }; }

	private:
		float m_ScaleX, m_ScaleY;
	};
}
