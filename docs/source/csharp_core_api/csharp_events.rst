C# Events
=========

There are different types of events that are being passed to ``Entity.OnEvent`` function as they happen.

`Event` class
-------------
It is a base class for all other events. It can be used to get an event type for further typecasting. There are following event types and categories:

.. code-block:: csharp

    public enum EventType
    {
        None = 0,
        WindowClosed, WindowResized, WindowFocused,
        KeyPressed, KeyReleased, KeyTyped,
        MouseButtonPressed, MouseButtonReleased, MouseMoved, MouseScrolled
    }

    public enum EventCategory
    {
        None        = 0,
        Application = 1 << 0,
        Input       = 1 << 1,
        Keyboard    = 1 << 2,
        Mouse       = 1 << 3,
        MouseButton = 1 << 4,
    }

    public abstract class Event
    {
        public abstract EventType GetEventType();
        public abstract EventCategory GetCategoryFlags();
        public bool IsInCategory(EventCategory category);
    }

You can use ``GetEventType`` function for type-casting to an appropriate event type. For example:

.. code-block:: csharp

    if (e.GetEventType() == EventType.KeyPressed)
    {
        KeyPressedEvent keyPressedEvent = e as KeyPressedEvent;
        ...
    }

`Application` events
--------------------
There are three events that are in ``Application`` category: ``Window Resize``, ``Window Close``, and ``Window Focused`` events.

``Window Resize`` event can be used to get the new width and height of the window.

``Window Focused`` event can be used to check if the window lost or gained focus.

.. code-block:: csharp

    public class WindowResizeEvent : Event
    {
        public readonly uint Width;
        public readonly uint Height;
        ...
    }

    public class WindowFocusedEvent : Event
    {
        public readonly bool bFocused;
        ...
    }

`Keyboard` events
-----------------
There are three events that are in ``Keyboard`` and ``Input`` categories: ``Key Pressed``, ``Key Released``, and ``Key Typed`` events.
Each of them is derived from ``KeyEvent`` class and they can be used to get a key that triggered the event. ``Key Pressed`` event also contains ``RepeatCount`` variable which equals to 0 if it is the first press of the key.

.. code-block:: csharp
    
    abstract public class KeyEvent : Event
    {
        public readonly KeyCode Key;
        ...
    }

`Mouse` events
--------------
There are two events that are in ``Mouse`` and ``Input`` categories: ``Mouse Moved`` and ``Mouse Scrolled`` events.
Each of them is derived from ``MouseEvent``. ``Mouse Moved`` event stores current mouse position and ``Mouse Scrolled`` event contains (X, Y) offsets of the scroll.

.. code-block:: csharp

    public class MouseMovedEvent : MouseEvent
    {
        public readonly float X;
        public readonly float Y;
        ...
    }

    public class MouseScrolledEvent : MouseEvent
    {
        public readonly float XOffset;
        public readonly float YOffset;
        ...
    }

And there are two more mouse events that are in ``Mouse``, ``Input``, and ``Mouse Button`` categories: ``Mouse Button Pressed`` and ``Mouse Button Released`` events.
Each of them is derived from ``MouseButtonEvent`` that stores a key that triggered the event.

.. code-block:: csharp
    
    abstract public class MouseButtonEvent : MouseEvent
    {
        public readonly MouseButton Key;
        ...
    }
