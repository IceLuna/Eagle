namespace Eagle
{
    abstract public class MouseEvent : Event
    {
        public override EventCategory GetCategoryFlags()
        {
            return EventCategory.Input | EventCategory.Mouse;
        }
    }

    public class MouseMovedEvent : MouseEvent
    {
        public readonly float X;
        public readonly float Y;

        public MouseMovedEvent(float x, float y)
        {
            X = x;
            Y = y;
        }

        public override EventType GetEventType()
        {
            return EventType.MouseMoved;
        }

        public override string ToString()
        {
            return $"MouseMovedEvent: {X}, {Y}";
        }
    }

    public class MouseScrolledEvent : MouseEvent
    {
        public readonly float XOffset;
        public readonly float YOffset;

        public MouseScrolledEvent(float xOffset, float yOffset)
        {
            XOffset = xOffset;
            YOffset = yOffset;
        }

        public override EventType GetEventType()
        {
            return EventType.MouseScrolled;
        }

        public override string ToString()
        {
            return $"MouseScrolledEvent: {XOffset}, {YOffset}";
        }
    }

    abstract public class MouseButtonEvent : Event
    {
        public readonly MouseButton Key;

        public MouseButtonEvent(MouseButton key)
        {
            Key = key;
        }

        public override EventCategory GetCategoryFlags()
        {
            return EventCategory.Input | EventCategory.Mouse | EventCategory.MouseButton;
        }
    }

    public class MouseButtonPressedEvent : MouseButtonEvent
    {
        public MouseButtonPressedEvent(MouseButton key) : base(key)
        {
        }

        public override EventType GetEventType()
        {
            return EventType.MouseButtonPressed;
        }

        public override string ToString()
        {
            return $"MouseButtonPressedEvent: {Key}";
        }
    }

    public class MouseButtonReleasedEvent : MouseButtonEvent
    {
        public MouseButtonReleasedEvent(MouseButton key) : base(key)
        {
        }

        public override EventType GetEventType()
        {
            return EventType.MouseButtonReleased;
        }

        public override string ToString()
        {
            return $"MouseButtonReleasedEvent: {Key}";
        }
    }


}
