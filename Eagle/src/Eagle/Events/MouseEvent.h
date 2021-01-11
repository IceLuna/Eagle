#pragma once

#include "Event.h"
#include "Eagle/Input/MouseButtonCodes.h"

namespace Eagle
{
	class MouseEvent : public Event
	{
	public:
		EVENT_CLASS_CATEGORY(EventCategoryInput | EventCategoryMouse)
	protected:
		MouseEvent() = default;
	};

	class MouseMovedEvent : public MouseEvent
	{
	public:
		EVENT_CLASS_TYPE(MouseMoved)

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

	private:
		float m_MouseX, m_MouseY;
	};

	class MouseScrolledEvent : public MouseEvent
	{
	public:
		EVENT_CLASS_TYPE(MouseScrolled)

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

	private:
		float m_XOffset, m_YOffset;
	};

	class MouseButtonEvent : public MouseEvent
	{
	public:
		EVENT_CLASS_CATEGORY(EventCategoryInput | EventCategoryMouse | EventCategoryMouseButton)
	
		Mouse::MouseButton GetMouseCode() const { return m_MouseCode; }

	protected:
		MouseButtonEvent(Mouse::MouseButton mouseCode)
			: m_MouseCode(mouseCode) {}

		Mouse::MouseButton m_MouseCode;
	};

	class MouseButtonPressedEvent : public MouseButtonEvent
	{
	public:
		EVENT_CLASS_TYPE(MouseButtonPressed)

		MouseButtonPressedEvent(Mouse::MouseButton mouseCode)
			: MouseButtonEvent(mouseCode) {}

		std::string ToString() const override
		{
			std::stringstream ss;
			ss << "MouseButtonPressedEvent: " << m_MouseCode;
			return ss.str();
		}
	};

	class MouseButtonReleasedEvent : public MouseButtonEvent
	{
	public:
		EVENT_CLASS_TYPE(MouseButtonReleased)

		MouseButtonReleasedEvent(Mouse::MouseButton mouseCode)
			: MouseButtonEvent(mouseCode) {}

		std::string ToString() const override
		{
			std::stringstream ss;
			ss << "MouseButtonReleasedEvent: " << m_MouseCode;
			return ss.str();
		}
	};
}
