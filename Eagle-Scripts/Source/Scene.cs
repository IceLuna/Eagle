using System;
using System.Runtime.CompilerServices;

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

    public struct RendererVertex
    {
        public Vector3 Location;
        public Color3 Color;
    }

    public struct RendererLine
    {
        public RendererVertex Start;
        public RendererVertex End;
    }

    public struct RendererTriangle
    {
        public RendererVertex V0, V1, V2;
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
            DrawLine_Native(ref line.Start.Color, ref line.End.Color, ref line.Start.Location, ref line.End.Location);
        }

        public static void DrawTriangle(RendererTriangle triangle)
        {
            DrawTriangle_Native(ref triangle.V0.Location, ref triangle.V0.Color,
                ref triangle.V1.Location, ref triangle.V1.Color,
                ref triangle.V2.Location, ref triangle.V2.Color);
        }

        public static void SetGravity(Vector3 gravity)
        {
            SetGravity_Native(ref gravity);
        }

        public static Vector3 GetGravity()
        {
            GetGravity_Native(out Vector3 result);
            return result;
        }

        public static Entity[] GetAllEntitiesWithComponent<T>() where T : Component, new()
        {
            return GetAllEntitiesWithComponent_Native(typeof(T));
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void OpenScene_Native(GUID assetID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool Raycast_Native(ref Vector3 origin, ref Vector3 dir, float maxDistance, out GUID hitEntity, out Vector3 position, out Vector3 normal, out float distance);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void DrawLine_Native(ref Color3 startColor, ref Color3 endColor, ref Vector3 start, ref Vector3 end);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void DrawTriangle_Native(ref Vector3 LocationV0, ref Color3 ColorV0, ref Vector3 LocationV1, ref Color3 ColorV1, ref Vector3 LocationV2, ref Color3 ColorV2);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetGravity_Native(ref Vector3 gravity);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void GetGravity_Native(out Vector3 gravity);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern Entity[] GetAllEntitiesWithComponent_Native(Type type);
    }
}
