using System;
using System.Runtime.CompilerServices;

namespace Eagle
{
    // Picks the entity at a viewport pixel using object picking (see the project settings). Meant for clicks specifically, unlike `Entity.IsMouseHovered`.
    // The entity's ID is readback from the GPU, and since it's a non-blocking readback, we must wait a few frames for the result starting from a frame rendered after `Pick` was called.
    // So the result is for that pixel even if the mouse was moved since then.
    // Usage example: call `PickAtMouse` on a click, then call `TryGetResult` every frame
    public class EntityPicker
    {
        // A pick without a result after this many frames is dropped
        private const ulong MaxWaitFrames = 60;

        private enum State { None, Pending, Ready }

        // Must match the C++ `EntityPickStatus`
        internal enum PickStatus { Pending, Ready, Failed }

        private State m_State = State.None;
        private Vector2 m_Coord;
        private ulong m_Frame;
        private Entity m_Result;

        // Starts picking at `viewportCoord` (see `Input.GetMousePositionInViewport`). Replaces the last pick and its result
        public void Pick(Vector2 viewportCoord)
        {
            m_State = State.Pending;
            m_Coord = viewportCoord;
            m_Frame = Renderer.GetFrameNumber();
            m_Result = null;
            Update();
        }

        public void PickAtMouse()
        {
            GetMouseCoord_Native(out Vector2 coord);
            Pick(coord);
        }

        // Returns true if the result of the last `Pick` call isn't resolved yet
        public bool IsPending()
        {
            return Update() == State.Pending;
        }

        // Returns true once the result of the last `Pick` is known, and keeps returning it until the next `Pick`.
        // `entity` is null for empty space, or if the entity was destroyed.
        // Returns false while it's pending or it failed/dropped
        public bool TryGetResult(out Entity entity)
        {
            if (Update() != State.Ready)
            {
                entity = null;
                return false;
            }

            entity = m_Result != null && m_Result.IsValid() ? m_Result : null;
            return true;
        }

        public void Reset()
        {
            m_State = State.None;
            m_Result = null;
        }

        private State Update()
        {
            if (m_State != State.Pending)
                return m_State;

            Vector2 coord = m_Coord;
            switch (GetEntityAt_Native(ref coord, m_Frame, out Entity entity))
            {
                case PickStatus.Ready:
                    m_Result = entity;
                    m_State = State.Ready;
                    break;
                case PickStatus.Failed:
                    m_State = State.None;
                    break;
                default:
                    if ((Renderer.GetFrameNumber() - m_Frame) > MaxWaitFrames)
                    {
                        Log.Warn($"EntityPicker: dropped the pick at ({m_Coord.X}, {m_Coord.Y}), no result after {MaxWaitFrames} frames");
                        m_State = State.None;
                    }
                    break;
            }
            return m_State;
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern PickStatus GetEntityAt_Native(ref Vector2 viewportCoord, ulong minFrame, out Entity entity);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetMouseCoord_Native(out Vector2 viewportCoord);
    }
}
