namespace Eagle
{
    public enum EventType
    {
        None = 0,
        [UIName("Window closed")] WindowClosed,
        [UIName("Window resized")] WindowResized,
        [UIName("Window focused")] WindowFocused,
        [UIName("Window content scale")] WindowContentScale,
        [UIName("Key pressed")] KeyPressed,
        [UIName("Key released")] KeyReleased,
        [UIName("Key typed")] KeyTyped,
        [UIName("Mouse button pressed")] MouseButtonPressed,
        [UIName("Mouse button released")] MouseButtonReleased,
        [UIName("Mouse moved")] MouseMoved,
        [UIName("Mouse scrolled")] MouseScrolled
    }

    public enum EventCategory
    {
        None = 0,
        Application = 1 << 0,
        Input = 1 << 1,
        Keyboard = 1 << 2,
        Mouse = 1 << 3,
        [UIName("Mouse button")] MouseButton = 1 << 4,
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
