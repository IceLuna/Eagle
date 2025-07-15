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

    public class Scene
    {
        public static void OpenScene(AssetScene scene) { OpenScene_Native(scene.GetGUID()); }

        public static void QuitGame() { QuitGame_Native(); }

        public static bool Raycast(Vector3 origin, Vector3 dir, float maxDistance, out RaycastHit outHit, PhysicsQueryType query = PhysicsQueryType.Default)
        {
            GUID guid = GUID.Null();
            outHit = new RaycastHit();
            bool bHit = Raycast_Native(ref origin, ref dir, maxDistance, query, out guid, out outHit.Position, out outHit.Normal, out outHit.Distance);
            outHit.HitEntity = new Entity(guid);
            return bHit;
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
        private static extern void QuitGame_Native();

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool Raycast_Native(ref Vector3 origin, ref Vector3 dir, float maxDistance, PhysicsQueryType query, out GUID hitEntity, out Vector3 position, out Vector3 normal, out float distance);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetGravity_Native(ref Vector3 gravity);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void GetGravity_Native(out Vector3 gravity);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern Entity[] GetAllEntitiesWithComponent_Native(Type type);
    }
}
