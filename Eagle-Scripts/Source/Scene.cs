using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace Eagle
{
    public struct RaycastHit
    {
        public Entity HitEntity;
        public Vector3 Position;
        public float Distance;
        public Vector3 Normal;
    };

    public struct CollisionInfo
    {
        public Vector3 Position;
        public Vector3 Normal;
        public Vector3 Impulse;
        public Vector3 Force;
    }

    public struct RendererLine
    {
        public Color3 Color;
        public Vector3 StartPos;
        public Vector3 EndPos;
    }

    public class Scene
    {
        public static void OpenScene(AssetScene scene) { OpenScene_Native(scene.GetGUID()); }

        public static bool Raycast(Vector3 origin, Vector3 dir, float maxDistance, out RaycastHit outHit)
        {
            GUID guid = GUID.Null();
            outHit = new RaycastHit();
            bool bHit = Raycast_Native(ref origin, ref dir, maxDistance, out guid, out outHit.Position, out outHit.Normal, out outHit.Distance);
            outHit.HitEntity = new Entity(guid);
            return bHit;
        }

        public static void DrawLine(RendererLine line)
        {
            DrawLine_Native(ref line.Color, ref line.StartPos, ref line.EndPos);
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void OpenScene_Native(GUID assetID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool Raycast_Native(ref Vector3 origin, ref Vector3 dir, float maxDistance, out GUID hitEntity, out Vector3 position, out Vector3 normal, out float distance);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void DrawLine_Native(ref Color3 color, ref Vector3 start, ref Vector3 end);
    }
}
