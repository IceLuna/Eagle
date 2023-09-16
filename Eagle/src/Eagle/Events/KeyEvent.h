#pragma once

#include "Event.h"
#include "Eagle/Input/KeyCodes.h"

namespace Eagle
{
	class KeyEvent : public Event
	{
	public:
		EVENT_CLASS_CATEGORY(EventCategory::Input | EventCategory::Keyboard)

		Key GetKey() const { return m_KeyCode; }

	protected:
		KeyEvent(Key keyCode) : m_KeyCode(keyCode) {}
		
		Key m_KeyCode;
	};

	class KeyPressedEvent : public KeyEvent
	{
	public:
		EVENT_CLASS_TYPE(KeyPressed, Eagle.KeyCode,uint)

		KeyPressedEvent(Key keyCode, uint16_t repeatCount)
			: KeyEvent(keyCode), m_RepeatCount(repeatCount) {}

		uint32_t GetRepeatCount() const { return m_RepeatCount; }

		std::string ToString() const override
		{
			std::stringstream ss;
			ss << "KeyPressedEvent: " << Utils::GetEnumName(m_KeyCode) << " (" << m_RepeatCount << " repeats)";
			return ss.str();
		}

		std::array<void*, 2> GetData() override { return { &m_KeyCode, &m_RepeatCount }; }

	private:
		uint32_t m_RepeatCount;
	};

	class KeyReleasedEvent : public KeyEvent
	{
	public:
		EVENT_CLASS_TYPE(KeyReleased, Eagle.KeyCode)

		KeyReleasedEvent(Key keyCode)
			: KeyEvent(keyCode) {}

		std::string ToString() const override
		{
			return std::string("KeyReleasedEvent: ") + Utils::GetEnumName(m_KeyCode);
		}

		std::array<void*, 2> GetData() override { return { &m_KeyCode, nullptr }; }
	};

	class KeyTypedEvent : public KeyEvent
	{
	public:
		EVENT_CLASS_TYPE(KeyTyped, Eagle.KeyCode)

		KeyTypedEvent(Key keyCode)
			: KeyEvent(keyCode) {}

		std::string ToString() const override
		{
			return std::string("KeyTypedEvent: ") + Utils::GetEnumName(m_KeyCode);
		}
		
		std::array<void*, 2> GetData() override { return { &m_KeyCode, nullptr }; }
	};
}

