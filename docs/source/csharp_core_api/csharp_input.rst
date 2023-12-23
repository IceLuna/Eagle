C# Inputs
=========

It is a static class that allows you to query states of some input devices.

.. code-block:: csharp

    public enum CursorMode
    {
        Normal = 0,
        Hidden = 1
    }

    public static class Input
    {
        public static bool IsKeyPressed(KeyCode keyCode);

        public static bool IsMouseButtonPressed(MouseButton button);

        // Returns global mouse pos. If a mouse cursor is hidden, this will still update and return virtual mouse pos
        public static Vector2 GetMousePosition();

        // Returns mouse pos within a viewport. If a mouse is hidden, this won't change.
        // Also, there's a `Renderer.GetViewportSize`
        public static Vector2 GetMousePositionInViewport();

        // Sets global mouse pos
        public static void SetMousePosition(Vector2 position);

        // Sets mouse pos within a viewport. If a viewport is 100x50, then 50x25 coord will set the mouse to the center
        // Also, there's a `Renderer.GetViewportSize`
        public static void SetMousePositionInViewport(Vector2 position);

        public static void SetCursorMode(CursorMode mode) => SetCursorMode_Native(mode);
        public static CursorMode GetCursorMode() => GetCursorMode_Native();
    }
