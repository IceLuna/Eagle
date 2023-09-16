namespace Eagle
{
    abstract public class KeyEvent : Event
    {
        public readonly KeyCode Key;

        public KeyEvent(KeyCode key)
        {
            Key = key;
        }

        public override EventCategory GetCategoryFlags()
        {
            return EventCategory.Input | EventCategory.Keyboard;
        }
    }

    public class KeyPressedEvent : KeyEvent
    {
        public readonly uint RepeatCount;

        public KeyPressedEvent(KeyCode key, uint repeatCount) : base(key)
        {
            RepeatCount = repeatCount;
        }

        public override EventType GetEventType()
        {
            return EventType.KeyPressed;
        }

        public override string ToString()
		{
			return $"KeyPressedEvent: {Key} ({RepeatCount} repeats)";
		}
    }

    public class KeyReleasedEvent : KeyEvent
    {
        public KeyReleasedEvent(KeyCode key) : base(key)
        {
        }

        public override EventType GetEventType()
        {
            return EventType.KeyReleased;
        }

        public override string ToString()
		{
			return $"KeyReleasedEvent: {Key}";
		}
    }

    public class KeyTypedEvent : KeyEvent
    {
        public KeyTypedEvent(KeyCode key) : base(key)
        {
        }

        public override EventType GetEventType()
        {
            return EventType.KeyTyped;
        }

        public override string ToString()
		{
			return $"KeyTypedEvent: {Key}";
		}
    }
}
