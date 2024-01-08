using System;
using System.Runtime.CompilerServices;

namespace Eagle
{
    public class Entity
    {
        // First `Entity` is an entity that owns a callback
        private Action<Entity, Entity, CollisionInfo> m_CollisionBeginCallbacks;
        private Action<Entity, Entity, CollisionInfo> m_CollisionEndCallbacks;
        private Action<Entity, Entity> m_TriggerBeginCallbacks;
        private Action<Entity, Entity> m_TriggerEndCallbacks;

        public GUID ID { get; private set; }

        protected Entity() {  }

        internal Entity(GUID id) { ID = id; }

        public virtual void OnCreate() { }

        public virtual void OnDestroy() { }

        public virtual void OnUpdate(float ts) { }

        public virtual void OnPhysicsUpdate(float ts) { }

        public virtual void OnEvent(Event e) { }

        public Entity Parent
        {
            get => new Entity(GetParent_Native(ID));
            set => SetParent_Native(ID, value.ID);
        }

        public Entity[] Children
        {
            get => GetChildren_Native(ID);
        }

        public Transform WorldTransform
        {
            get
            {
                GetWorldTransform_Native(ID, out Transform result);
                return result;
            }
            set
            {
                SetWorldTransform_Native(ID, ref value);
            }
        }

        public Vector3 WorldLocation
        {
            get
            {
                GetWorldLocation_Native(ID, out Vector3 result);
                return result;
            }
            set
            {
                SetWorldLocation_Native(ID, ref value);
            }
        }

        public Rotator WorldRotation
        {
            get
            {
                GetWorldRotation_Native(ID, out Rotator result);
                return result;
            }
            set
            {
                SetWorldRotation_Native(ID, ref value);
            }
        }

        public Vector3 WorldScale
        {
            get
            {
                GetWorldScale_Native(ID, out Vector3 result);
                return result;
            }
            set
            {
                SetWorldScale_Native(ID, ref value);
            }
        }

        public Transform RelativeTransform
        {
            get
            {
                GetRelativeTransform_Native(ID, out Transform result);
                return result;
            }
            set
            {
                SetRelativeTransform_Native(ID, ref value);
            }
        }

        public Vector3 RelativeLocation
        {
            get
            {
                GetRelativeLocation_Native(ID, out Vector3 result);
                return result;
            }
            set
            {
                SetRelativeLocation_Native(ID, ref value);
            }
        }

        public Rotator RelativeRotation
        {
            get
            {
                GetRelativeRotation_Native(ID, out Rotator result);
                return result;
            }
            set
            {
                SetRelativeRotation_Native(ID, ref value);
            }
        }

        public Vector3 RelativeScale
        {
            get
            {
                GetRelativeScale_Native(ID, out Vector3 result);
                return result;
            }
            set
            {
                SetRelativeScale_Native(ID, ref value);
            }
        }

        public T AddComponent<T>() where T : Component, new()
        {
            AddComponent_Native(ID, typeof(T));
            T component = new T();
            component.Parent = this;
            return component;
        }

        public bool HasComponent<T>() where T : Component, new()
        {
            return HasComponent_Native(ID, typeof(T));
        }

        public bool HasComponent(Type type)
        {
            return HasComponent_Native(ID, type);
        }

        public T GetComponent<T>() where T : Component, new()
        {
            if (HasComponent<T>())
            {
                T component = new T();
                component.Parent = this;
                return component;
            }
            return null;
        }

        public GUID GetID() { return ID; }

        public Vector3 GetForwardVector()
        {
            GetForwardVector_Native(ID, out Vector3 result);
            return result;
        }
        
        public Vector3 GetRightVector()
        {
            GetRightVector_Native(ID, out Vector3 result);
            return result;
        }

        public Vector3 GetUpVector()
        {
            GetUpVector_Native(ID, out Vector3 result);
            return result;
        }

        public void Destroy()
        {
            DestroyEntity_Native(ID);
        }

        public bool IsMouseHovered() { return IsMouseHovered_Native(ID); }

        // @ mouseCoord. Mouse coord within a viewport
        public bool IsMouseHovered(ref Vector2 mouseCoord) { return IsMouseHoveredByCoord_Native(ID, ref mouseCoord); }

        public void AddCollisionBeginCallback(Action<Entity, Entity, CollisionInfo> callback)
        {
            m_CollisionBeginCallbacks += callback;
        }

        public void RemoveCollisionBeginCallback(Action<Entity, Entity, CollisionInfo> callback)
        {
            m_CollisionBeginCallbacks -= callback;
        }

        public void AddCollisionEndCallback(Action<Entity, Entity, CollisionInfo> callback)
        {
            m_CollisionEndCallbacks += callback;
        }

        public void RemoveCollisionEndCallback(Action<Entity, Entity, CollisionInfo> callback)
        {
            m_CollisionEndCallbacks -= callback;
        }

        public void AddTriggerBeginCallback(Action<Entity, Entity> callback)
        {
            m_TriggerBeginCallbacks += callback;
        }

        public void RemoveTriggerBeginCallback(Action<Entity, Entity> callback)
        {
            m_TriggerBeginCallbacks -= callback;
        }

        public void AddTriggerEndCallback(Action<Entity, Entity> callback)
        {
            m_TriggerEndCallbacks += callback;
        }

        public void RemoveTriggerEndCallback(Action<Entity, Entity> callback)
        {
            m_TriggerEndCallbacks -= callback;
        }

        private void OnCollisionBegin(GUID id, Vector3 position, Vector3 normal, Vector3 impulse, Vector3 force)
        {
            if (m_CollisionBeginCallbacks != null)
            {
                CollisionInfo collisionInfo = new CollisionInfo();
                collisionInfo.Position = position;
                collisionInfo.Normal = normal;
                collisionInfo.Impulse = impulse;
                collisionInfo.Force = force;
                m_CollisionBeginCallbacks.Invoke(this, new Entity(id), collisionInfo);
            }
        }

        private void OnCollisionEnd(GUID id, Vector3 position, Vector3 normal, Vector3 impulse, Vector3 force)
        {
            if (m_CollisionEndCallbacks != null)
            {
                CollisionInfo collisionInfo = new CollisionInfo();
                collisionInfo.Position = position;
                collisionInfo.Normal = normal;
                collisionInfo.Impulse = impulse;
                collisionInfo.Force = force;
                m_CollisionEndCallbacks.Invoke(this, new Entity(id), collisionInfo);
            }
        }

        private void OnTriggerBegin(GUID id)
        {
            if (m_TriggerBeginCallbacks != null)
                m_TriggerBeginCallbacks.Invoke(this, new Entity(id));
        }

        private void OnTriggerEnd(GUID id)
        {
            if (m_TriggerEndCallbacks != null)
                m_TriggerEndCallbacks.Invoke(this, new Entity(id));
        }

        public override string ToString()
        {
            return GetEntityName_Native(ID);
        }

        public Entity GetChildrenByName(string name)
        {
            return new Entity(GetChildrenByName_Native(ID, name));
        }

        static public Entity SpawnEntity(string name = "")
        {
            return new Entity(SpawnEntity_Native(name));
        }

        static public Entity SpawnEntity(AssetEntity asset)
        {
            return new Entity(SpawnEntityFromAsset_Native(asset.GetGUID()));
        }

        // C++ Method Implementations
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern GUID GetParent_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetParent_Native(in GUID entityID, GUID parentID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern Entity[] GetChildren_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void AddComponent_Native(in GUID entityID, Type type);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool HasComponent_Native(in GUID entityID, Type type);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void DestroyEntity_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern string GetEntityName_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetForwardVector_Native(in GUID entityID, out Vector3 result);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetRightVector_Native(in GUID entityID, out Vector3 result);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetUpVector_Native(in GUID entityID, out Vector3 result);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern GUID GetChildrenByName_Native(in GUID entityID, string name);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern GUID SpawnEntity_Native(string name);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern GUID SpawnEntityFromAsset_Native(GUID assetID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool IsMouseHovered_Native(GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool IsMouseHoveredByCoord_Native(GUID entityID, ref Vector2 mouseCoord);

        //---World functions---
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetWorldTransform_Native(in GUID entityID, out Transform outTransform);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetWorldTransform_Native(in GUID entityID, ref Transform inTransform);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetWorldLocation_Native(in GUID entityID, out Vector3 outLocation);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetWorldLocation_Native(in GUID entityID, ref Vector3 inLocation);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetWorldRotation_Native(in GUID entityID, out Rotator outRotation);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetWorldRotation_Native(in GUID entityID, ref Rotator inRotation);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetWorldScale_Native(in GUID entityID, out Vector3 outScale);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetWorldScale_Native(in GUID entityID, ref Vector3 inScale);

        //---Relative functions---
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetRelativeTransform_Native(in GUID entityID, out Transform outTransform);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetRelativeTransform_Native(in GUID entityID, ref Transform inTransform);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetRelativeLocation_Native(in GUID entityID, out Vector3 outLocation);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetRelativeLocation_Native(in GUID entityID, ref Vector3 inLocation);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetRelativeRotation_Native(in GUID entityID, out Rotator outRotation);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetRelativeRotation_Native(in GUID entityID, ref Rotator inRotation);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetRelativeScale_Native(in GUID entityID, out Vector3 outScale);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetRelativeScale_Native(in GUID entityID, ref Vector3 inScale);
    }
}
