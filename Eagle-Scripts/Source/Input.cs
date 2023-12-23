using System.Runtime.CompilerServices;

namespace Eagle
{
    public enum CursorMode
    {
        Normal = 0,
        Hidden = 1
        //Locked = 2
    }

    public static class Input
    {
		public static bool IsKeyPressed(KeyCode keyCode)
        {
			return IsKeyPressed_Native(keyCode);
        }

		public static bool IsMouseButtonPressed(MouseButton button)
        {
			return IsMouseButtonPressed_Native(button);
        }

        // Returns global mouse pos. If a mouse cursor is hidden, this will still update and return virtual mouse pos
        public static Vector2 GetMousePosition()
        {
            GetMousePosition_Native(out Vector2 position);
            return position;
        }

        // Returns mouse pos within a viewport. If a mouse is hidden, this won't change.
        // Also, there's a `Renderer.GetViewportSize`
        public static Vector2 GetMousePositionInViewport()
        {
            GetMousePositionInViewport_Native(out Vector2 position);
            return position;
        }

        // Sets global mouse pos
        public static void SetMousePosition(Vector2 position)
        {
            SetMousePosition_Native(ref position);
        }

        // Sets mouse pos within a viewport. If a viewport is 100x50, then 50x25 coord will set the mouse to the center
        // Also, there's a `Renderer.GetViewportSize`
        public static void SetMousePositionInViewport(Vector2 position)
        {
            SetMousePositionInViewport_Native(ref position);
        }

        public static void SetCursorMode(CursorMode mode) => SetCursorMode_Native(mode);

        public static CursorMode GetCursorMode() => GetCursorMode_Native();


        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool IsMouseButtonPressed_Native(MouseButton button);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool IsKeyPressed_Native(KeyCode keyCode);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void GetMousePosition_Native(out Vector2 position);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void GetMousePositionInViewport_Native(out Vector2 position);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetMousePosition_Native(ref Vector2 position);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetMousePositionInViewport_Native(ref Vector2 position);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetCursorMode_Native(CursorMode mode);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern CursorMode GetCursorMode_Native();

    }


}
