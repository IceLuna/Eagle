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

        public static bool Raycast(Vector3 origin, Vector3 dir, float maxDistance, out RaycastHit outHit, PhysicsQueryType query = PhysicsQueryType.Default, CollisionGroup collisionGroup = CollisionGroup.Any, Entity[] entitiesToIgnore = null)
        {
            GUID guid = GUID.Null();
            outHit = new RaycastHit();

            GUID[] entityGUIDsToIgnore = null;
            if (entitiesToIgnore != null)
            {
                entityGUIDsToIgnore = new GUID[entitiesToIgnore.Length];
                for (int i = 0; i < entitiesToIgnore.Length; ++i)
                {
                    entityGUIDsToIgnore[i] = entitiesToIgnore[i].ID;
                }
            }

            bool bHit = Raycast_Native(ref origin, ref dir, maxDistance, query, collisionGroup, entityGUIDsToIgnore, out outHit.HitEntity, out outHit.Position, out outHit.Normal, out outHit.Distance);
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

        public static Entity SpawnEntity(string name = "")
        {
            return new Entity(SpawnEntity_Native(name));
        }

        public static Entity SpawnEntity(AssetEntity asset)
        {
            return SpawnEntityFromAsset_Native(asset.GetGUID());
        }

        // Spawns an entity that has a particle system component
        // @ps. Can be null if you plan to provide it manually immediately after entity creation. Otherwise, entity will be destroyed at the end of the frame
        // @bAutoDestroy. When particle system has finished, entity is automatically destroyed.
        //     For this to work all emitters must be finite (no endless loop counts). Entity is destroyed when all emitters finish
        //     Note: the timer is reset when you call `SetAsset()` on particle system component of this entity, or `SetEmitters()` on paticle system asset that's used by the component
        public static Entity SpawnParticleSystem(Transform worldTransform, AssetParticleSystem ps, bool bAutoDestroy, string name = "C# Particle System")
        {
            return new Entity(SpawnParticleSystem_Native(name, ref worldTransform, ps == null ? GUID.Null() : ps.GetGUID(), bAutoDestroy));
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void OpenScene_Native(GUID assetID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void QuitGame_Native();

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool Raycast_Native(ref Vector3 origin, ref Vector3 dir, float maxDistance, PhysicsQueryType query, CollisionGroup group, GUID[] entitiesToIgnore, out Entity hitEntity, out Vector3 position, out Vector3 normal, out float distance);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetGravity_Native(ref Vector3 gravity);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void GetGravity_Native(out Vector3 gravity);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern Entity[] GetAllEntitiesWithComponent_Native(Type type);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern GUID SpawnEntity_Native(string name);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern Entity SpawnEntityFromAsset_Native(GUID assetID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern GUID SpawnParticleSystem_Native(string name, ref Transform tr, GUID ps, bool bAutoDestroy);
    }
}
