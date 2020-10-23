#pragma once

#include "Event.h"
#include "Eagle/Core/MouseCodes.h"

namespace Eagle
{
	class EAGLE_API MouseEvent : public Event
	{
	public:
		EVENT_CLASS_CATEGORY(EventCategoryInput | EventCategoryMouse)
	protected:
		MouseEvent() = default;
	};

	class EAGLE_API MouseMovedEvent : public MouseEvent
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

	class EAGLE_API MouseScrolledEvent : public MouseEvent
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

	class EAGLE_API MouseButtonEvent : public MouseEvent
	{
	public:
		EVENT_CLASS_CATEGORY(EventCategoryInput | EventCategoryMouse | EventCategoryMouseButton)
	
		MouseCode GetMouseCode() const { return m_MouseCode; }

	protected:
		MouseButtonEvent(MouseCode mouseCode)
			: m_MouseCode(mouseCode) {}

		MouseCode m_MouseCode;
	};

	class EAGLE_API MouseButtonPressedEvent : public MouseButtonEvent
	{
	public:
		EVENT_CLASS_TYPE(MouseButtonPressed)

		MouseButtonPressedEvent(MouseCode mouseCode)
			: MouseButtonEvent(mouseCode) {}

		std::string ToString() const override
		{
			std::stringstream ss;
			ss << "MouseButtonPressedEvent: " << m_MouseCode;
			return ss.str();
		}
	};

	class EAGLE_API MouseButtonReleasedEvent : public MouseButtonEvent
	{
	public:
		EVENT_CLASS_TYPE(MouseButtonReleased)

		MouseButtonReleasedEvent(MouseCode mouseCode)
			: MouseButtonEvent(mouseCode) {}

		std::string ToString() const override
		{
			std::stringstream ss;
			ss << "MouseButtonReleasedEvent: " << m_MouseCode;
			return ss.str();
		}
	};
}
