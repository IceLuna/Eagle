#pragma once

#include "Event.h"
#include "Eagle/Input/KeyCodes.h"

namespace Eagle
{
	class EAGLE_API KeyEvent : public Event
	{
	public:
		EVENT_CLASS_CATEGORY(EventCategoryInput | EventCategoryKeyboard)

		Key::KeyCode GetKeyCode() const { return m_KeyCode; }

	protected:
		KeyEvent(Key::KeyCode keyCode) : m_KeyCode(keyCode) {}
		
		Key::KeyCode m_KeyCode;
	};

	class EAGLE_API KeyPressedEvent : public KeyEvent
	{
	public:
		EVENT_CLASS_TYPE(KeyPressed)

		KeyPressedEvent(Key::KeyCode keyCode, uint16_t repeatCount)
			: KeyEvent(keyCode), m_RepeatCount(repeatCount) {}

		uint16_t GetRepeatCount() const { return m_RepeatCount; }

		std::string ToString() const override
		{
			std::stringstream ss;
			ss << "KeyPressedEvent: " << m_KeyCode << " (" << m_RepeatCount << " repeats)";
			return ss.str();
		}

	private:
		uint16_t m_RepeatCount;
	};

	class EAGLE_API KeyReleasedEvent : public KeyEvent
	{
	public:
		EVENT_CLASS_TYPE(KeyReleased)

		KeyReleasedEvent(Key::KeyCode keyCode)
			: KeyEvent(keyCode) {}

		std::string ToString() const override
		{
			std::stringstream ss;
			ss << "KeyReleasedEvent: " << m_KeyCode;
			return ss.str();
		}
	};

	class EAGLE_API KeyTypedEvent : public KeyEvent
	{
	public:
		EVENT_CLASS_TYPE(KeyTyped)

		KeyTypedEvent(Key::KeyCode keyCode)
			: KeyEvent(keyCode) {}

		std::string ToString() const override
		{
			std::stringstream ss;
			ss << "KeyTypedEvent: " << m_KeyCode;
			return ss.str();
		}
	};
}

