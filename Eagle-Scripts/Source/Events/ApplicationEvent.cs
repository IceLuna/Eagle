﻿namespace Eagle
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
            return EventType.WindowResize;
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
            return EventType.WindowClose;
        }
        
        public override EventCategory GetCategoryFlags()
        {
            return EventCategory.Application;
        }
    }
}
