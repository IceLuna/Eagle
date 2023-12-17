namespace Eagle
{
    public enum EventType
    {
        None = 0,
		WindowClosed, WindowResized, WindowFocused,
		KeyPressed, KeyReleased, KeyTyped,
		MouseButtonPressed, MouseButtonReleased, MouseMoved, MouseScrolled
    }

    public enum EventCategory
    {
        None = 0,
        Application = 1 << 0,
        Input = 1 << 1,
        Keyboard = 1 << 2,
        Mouse = 1 << 3,
        MouseButton = 1 << 4,
    }

    public abstract class Event
    {
        public abstract EventType GetEventType();

        public abstract EventCategory GetCategoryFlags();

        public bool IsInCategory(EventCategory category)
		{
			return (GetCategoryFlags() & category) == category;
		}
    }
}
