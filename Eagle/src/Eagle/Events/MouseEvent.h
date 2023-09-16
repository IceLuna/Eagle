#pragma once

#include "Event.h"
#include "Eagle/Input/MouseButtonCodes.h"

namespace Eagle
{
	class MouseEvent : public Event
	{
	public:
		EVENT_CLASS_CATEGORY(EventCategory::Input | EventCategory::Mouse)
	protected:
		MouseEvent() = default;
	};

	class MouseMovedEvent : public MouseEvent
	{
	public:
		EVENT_CLASS_TYPE(MouseMoved, single,single)

		MouseMovedEvent(float x, float y)
			: m_MouseX(x), m_MouseY(y) {}

		float GetX() const { return m_MouseX; }
		float GetY() const { return m_MouseY; }

		std::string ToString() const override
		{
			std::stringstream ss;
			ss << "MouseMovedEvent: " << m_MouseX << ", " << m_MouseY;
			return ss.str();
		}

		std::array<void*, 2> GetData() override { return { &m_MouseX, &m_MouseY }; }

	private:
		float m_MouseX, m_MouseY;
	};

	class MouseScrolledEvent : public MouseEvent
	{
	public:
		EVENT_CLASS_TYPE(MouseScrolled, single,single)

		MouseScrolledEvent(float xOffset, float yOffset)
			: m_XOffset(xOffset), m_YOffset(yOffset) {}

		float GetXOffset() const { return m_XOffset; }
		float GetYOffset() const { return m_YOffset; }

		std::string ToString() const override
		{
			std::stringstream ss;
			ss << "MouseScrolledEvent: " << m_XOffset << ", " << m_YOffset;
			return ss.str();
		}

		std::array<void*, 2> GetData() override { return { &m_XOffset, &m_YOffset }; }

	private:
		float m_XOffset, m_YOffset;
	};

	class MouseButtonEvent : public MouseEvent
	{
	public:
		EVENT_CLASS_CATEGORY(EventCategory::Input | EventCategory::Mouse | EventCategory::MouseButton)
	
		Mouse GetMouseCode() const { return m_MouseCode; }

	protected:
		MouseButtonEvent(Mouse mouseCode)
			: m_MouseCode(mouseCode) {}

		Mouse m_MouseCode;
	};

	class MouseButtonPressedEvent : public MouseButtonEvent
	{
	public:
		EVENT_CLASS_TYPE(MouseButtonPressed, Eagle.MouseButton)

		MouseButtonPressedEvent(Mouse mouseCode)
			: MouseButtonEvent(mouseCode) {}

		std::string ToString() const override
		{
			return std::string("MouseButtonPressedEvent: ") + Utils::GetEnumName(m_MouseCode);
		}

		std::array<void*, 2> GetData() override { return { &m_MouseCode, nullptr }; }
	};

	class MouseButtonReleasedEvent : public MouseButtonEvent
	{
	public:
		EVENT_CLASS_TYPE(MouseButtonReleased, Eagle.MouseButton)

		MouseButtonReleasedEvent(Mouse mouseCode)
			: MouseButtonEvent(mouseCode) {}

		std::string ToString() const override
		{
			return std::string("MouseButtonReleasedEvent: ") + Utils::GetEnumName(m_MouseCode);
		}

		std::array<void*, 2> GetData() override { return { &m_MouseCode, nullptr }; }
	};
}
