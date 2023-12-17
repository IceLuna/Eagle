#pragma once

#include "Eagle/Core/Core.h"
#include "Eagle/Core/EnumUtils.h"
#include "Eagle/Utils/Utils.h"

#include <string>

namespace Eagle
{
	// Events in Eagle are currently blocking, meaning when an event occurs it
	// immediately gets dispatched and must be dealt with right then and there.
	// For the future, a better strategy might be to buffer events in an event
	// bus and process them during the "event" part of the update stage.

	enum class EventType
	{
		None = 0,
		WindowClose, WindowResize, WindowFocused,
		KeyPressed, KeyReleased, KeyTyped,
		MouseButtonPressed, MouseButtonReleased, MouseMoved, MouseScrolled
	};

	enum class EventCategory
	{
		None = 0,
		Application = BIT(0),
		Input = BIT(1),
		Keyboard = BIT(2),
		Mouse = BIT(3),
		MouseButton = BIT(4),
	};
	DECLARE_FLAGS(EventCategory);

#define EVENT_CLASS_TYPE(type, ...) static EventType GetStaticType() { return EventType::type; }\
								virtual EventType GetEventType() const override { return GetStaticType(); }\
								virtual const char* GetName() const override { return #type; }\
								virtual const char* GetCSharpCtor() const override { return "Eagle." #type "Event:.ctor " "(" #__VA_ARGS__ ")"; }\

#define EVENT_CLASS_CATEGORY(category) virtual EventCategory GetCategoryFlags() const override { return category; }

	class Event
	{
	public:
		virtual ~Event() = default;

		bool Handled = false;

		virtual EventType GetEventType() const = 0;
		virtual const char* GetName() const = 0;
		virtual const char* GetCSharpCtor() const = 0;
		virtual EventCategory GetCategoryFlags() const = 0;
		virtual std::string ToString() const { return GetName(); }
		virtual std::array<void*, 2> GetData() { return { nullptr, nullptr }; }

		bool IsInCategory(EventCategory category) const noexcept
		{
			return (GetCategoryFlags() & category) == category;
		}

	};

	class EventDispatcher
	{
	public:
		EventDispatcher(Event& event) : m_Event(event) {}

		//F will be deduced by the compiler
		template<typename T, typename F>
		bool Dispatch(const F& func)
		{
			if (m_Event.GetEventType() == T::GetStaticType())
			{
				m_Event.Handled |= func(static_cast<T&>(m_Event));
				return true;
			}
			return false;
		}

	private:
		Event& m_Event;
	};

	inline std::ostream& operator<< (std::ostream& os, const Event& e)
	{
		return os << e.ToString();
	}
}


