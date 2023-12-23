namespace Eagle
{
    public class WindowResizeEvent : Event
	{
        public readonly uint Width;
        public readonly uint Height;

        public WindowResizeEvent(uint width, uint height)
		{
			Width = width;
			Height = height;
		}
		
        public override string ToString()
		{
			return $"WindowResizeEvent: {Width}, {Height}";
		}
        
        public override EventType GetEventType()
        {
            return EventType.WindowResized;
        }
        
        public override EventCategory GetCategoryFlags()
        {
            return EventCategory.Application;
        }
    }

    public class WindowCloseEvent : Event
	{
        public override EventType GetEventType()
        {
            return EventType.WindowClosed;
        }
        
        public override EventCategory GetCategoryFlags()
        {
            return EventCategory.Application;
        }
    }

    public class WindowFocusedEvent : Event
    {
        public readonly bool bFocused;

        public WindowFocusedEvent(bool bFocused)
        {
            this.bFocused = bFocused;
        }

        public override string ToString()
        {
            return $"WindowFocusedEvent: {bFocused}";
        }

        public override EventType GetEventType()
        {
            return EventType.WindowFocused;
        }

        public override EventCategory GetCategoryFlags()
        {
            return EventCategory.Application;
        }
    }
}
