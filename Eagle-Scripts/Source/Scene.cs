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

    public enum SceneLoadState
    {
        Queued,    // Waiting for the background thread to start loading it
		Loading,
		Ready,     // Loaded, waiting for `Activate()` (only happens if it was requested with `bActivateWhenReady == false`)
		Activated, // Opened (or it's about to be opened at the beginning of the next frame)
		Failed,
		Cancelled,
	};

    // A scene that's being loaded in the background. See `Scene.OpenSceneAsync`
    public class SceneLoadRequest
    {
        internal SceneLoadRequest(ulong id) { ID = id; }

        internal readonly ulong ID;

        // Loading progress in [0; 1] range
        public float Progress => GetProgress_Native(ID);

        public SceneLoadState State => GetState_Native(ID);

        // Loaded and waiting for `Activate()`
        public bool IsReady => State == SceneLoadState.Ready;

		// True if it's finished (activated, failed or cancelled)
        public bool IsDone
        {
            get
            {
                SceneLoadState state = State;
                return state == SceneLoadState.Activated || state == SceneLoadState.Failed || state == SceneLoadState.Cancelled;
            }
        }

        public AssetScene SceneAsset
        {
            get
            {
                GUID guid = GetSceneAssetGUID_Native(ID);
                return guid.IsNull() ? null : new AssetScene(guid);
            }
        }

        // Opens the loaded scene at the beginning of the next frame. If it's still loading, it's opened as soon as it's loaded
        public void Activate() { Activate_Native(ID); }

        // Stops loading it or drops it, if it's loaded but not activated yet
        public void Cancel() { Cancel_Native(ID); }

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern float GetProgress_Native(ulong id);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern SceneLoadState GetState_Native(ulong id);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern GUID GetSceneAssetGUID_Native(ulong id);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Activate_Native(ulong id);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Cancel_Native(ulong id);
    }

    public class Scene
    {
        // Cancels previous async loads
        public static void OpenScene(AssetScene scene) { OpenScene_Native(scene.GetGUID()); }

        // Loads a scene in the background while the current scene keeps running (for example, a loading screen that displays `Progress`).
        // Only one scene is loaded at a time meaning calling it again (or calling `OpenScene`) cancels the previous load.
        // But you can stack `OpenScene` and `OpenSceneAsync`. For example, you can call a regular `OpenScene` to open a loading screen scene,
        // and right after this call, you can issue `OpenSceneAsync` request. Calling `OpenSceneAsync` won't cancel `OpenScene` since `OpenScene` is not async
        // @bActivateWhenReady. If false, the loaded scene waits until `SceneLoadRequest.Activate()` is called (for example, "Press any key to continue")
        public static SceneLoadRequest OpenSceneAsync(AssetScene scene, bool bActivateWhenReady = true)
        {
            if (scene == null)
                return null;

            ulong id = OpenSceneAsync_Native(scene.GetGUID(), bActivateWhenReady);
            return id == 0 ? null : new SceneLoadRequest(id);
        }

        // Returns the scene that's being loaded in the background, or null.
        // Useful when the load was started by another scene (for example, a menu that opened the loading screen and requested the level)
        public static SceneLoadRequest GetActiveSceneLoad()
        {
            ulong id = GetActiveSceneLoad_Native();
            return id == 0 ? null : new SceneLoadRequest(id);
        }

        public static void QuitGame() { QuitGame_Native(); }

        public static bool Raycast(Vector3 origin, Vector3 dir, float maxDistance, out RaycastHit outHit, PhysicsQueryType query = PhysicsQueryType.Default, CollisionGroup collisionGroup = CollisionGroup.Any, Entity[] entitiesToIgnore = null)
        {
            GUID[] entityGUIDsToIgnore = GetEntityGUIDs(entitiesToIgnore);
            return Raycast_Native(ref origin, ref dir, maxDistance, query, collisionGroup, entityGUIDsToIgnore, out outHit.HitEntity, out outHit.Position, out outHit.Normal, out outHit.Distance);
        }

        public static Entity[] OverlapBox(Transform transform, Vector3 boxHalfSize, PhysicsQueryType query = PhysicsQueryType.Default, CollisionGroup collisionGroup = CollisionGroup.Any, Entity[] entitiesToIgnore = null)
        {
            GUID[] entityGUIDsToIgnore = GetEntityGUIDs(entitiesToIgnore);
            return OverlapBox_Native(ref transform, ref boxHalfSize, query, collisionGroup, entityGUIDsToIgnore);
        }

        public static Entity[] OverlapCapsule(Transform transform, float radius, float halfHeight, PhysicsQueryType query = PhysicsQueryType.Default, CollisionGroup collisionGroup = CollisionGroup.Any, Entity[] entitiesToIgnore = null)
        {
            GUID[] entityGUIDsToIgnore = GetEntityGUIDs(entitiesToIgnore);
            return OverlapCapsule_Native(ref transform, radius, halfHeight, query, collisionGroup, entityGUIDsToIgnore);
        }

        public static Entity[] OverlapSphere(Transform transform, float radius, PhysicsQueryType query = PhysicsQueryType.Default, CollisionGroup collisionGroup = CollisionGroup.Any, Entity[] entitiesToIgnore = null)
        {
            GUID[] entityGUIDsToIgnore = GetEntityGUIDs(entitiesToIgnore);
            return OverlapSphere_Native(ref transform, radius, query, collisionGroup, entityGUIDsToIgnore);
        }

        public static Entity[] SweepBox(Transform transform, Vector3 boxHalfSize, Vector3 direction, float distance, PhysicsQueryType query = PhysicsQueryType.Default, CollisionGroup collisionGroup = CollisionGroup.Any, Entity[] entitiesToIgnore = null)
        {
            GUID[] entityGUIDsToIgnore = GetEntityGUIDs(entitiesToIgnore);
            return SweepBox_Native(ref transform, ref boxHalfSize, ref direction, distance, query, collisionGroup, entityGUIDsToIgnore);
        }

        public static Entity[] SweepCapsule(Transform transform, float radius, float halfHeight, Vector3 direction, float distance, PhysicsQueryType query = PhysicsQueryType.Default, CollisionGroup collisionGroup = CollisionGroup.Any, Entity[] entitiesToIgnore = null)
        {
            GUID[] entityGUIDsToIgnore = GetEntityGUIDs(entitiesToIgnore);
            return SweepCapsule_Native(ref transform, radius, halfHeight, ref direction, distance, query, collisionGroup, entityGUIDsToIgnore);
        }

        public static Entity[] SweepSphere(Transform transform, float radius, Vector3 direction, float distance, PhysicsQueryType query = PhysicsQueryType.Default, CollisionGroup collisionGroup = CollisionGroup.Any, Entity[] entitiesToIgnore = null)
        {
            GUID[] entityGUIDsToIgnore = GetEntityGUIDs(entitiesToIgnore);
            return SweepSphere_Native(ref transform, radius, ref direction, distance, query, collisionGroup, entityGUIDsToIgnore);
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

        private static GUID[] GetEntityGUIDs(Entity[] entitiesToIgnore)
        {
            GUID[] entityGUIDs= null;
            if (entitiesToIgnore != null)
            {
                entityGUIDs = new GUID[entitiesToIgnore.Length];
                for (int i = 0; i < entitiesToIgnore.Length; ++i)
                {
                    entityGUIDs[i] = entitiesToIgnore[i].ID;
                }
            }

            return entityGUIDs;
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void OpenScene_Native(GUID assetID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern ulong OpenSceneAsync_Native(GUID assetID, bool bActivateWhenReady);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern ulong GetActiveSceneLoad_Native();

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void QuitGame_Native();

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool Raycast_Native(ref Vector3 origin, ref Vector3 dir, float maxDistance, PhysicsQueryType query, CollisionGroup group, GUID[] entitiesToIgnore, out Entity hitEntity, out Vector3 position, out Vector3 normal, out float distance);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern Entity[] OverlapBox_Native(ref Transform transform, ref Vector3 boxHalfSize, PhysicsQueryType query, CollisionGroup collisionGroup, GUID[] entitiesToIgnore);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern Entity[] OverlapCapsule_Native(ref Transform transform, float radius, float halfHeight, PhysicsQueryType query, CollisionGroup collisionGroup, GUID[] entitiesToIgnore);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern Entity[] OverlapSphere_Native(ref Transform transform, float radius, PhysicsQueryType query, CollisionGroup collisionGroup, GUID[] entitiesToIgnore);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern Entity[] SweepBox_Native(ref Transform transform, ref Vector3 boxHalfSize, ref Vector3 direction, float distance, PhysicsQueryType query, CollisionGroup collisionGroup, GUID[] entitiesToIgnore);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern Entity[] SweepCapsule_Native(ref Transform transform, float radius, float halfHeight, ref Vector3 direction, float distance, PhysicsQueryType query, CollisionGroup collisionGroup, GUID[] entitiesToIgnore);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern Entity[] SweepSphere_Native(ref Transform transform, float radius, ref Vector3 direction, float distance, PhysicsQueryType query, CollisionGroup collisionGroup, GUID[] entitiesToIgnore);

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
