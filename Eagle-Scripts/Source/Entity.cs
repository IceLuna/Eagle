using System;
using System.Runtime.CompilerServices;

namespace Eagle
{
    public class Entity
    {
        private Action<Entity> m_CollisionBeginCallbacks;
        private Action<Entity> m_CollisionEndCallbacks;
        private Action<Entity> m_TriggerBeginCallbacks;
        private Action<Entity> m_TriggerEndCallbacks;

        public GUID ID { get; private set; }

        protected Entity() {  }

        internal Entity(GUID id) { ID = id; }

        ~Entity() {}

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
                return GetComponent<TransformComponent>().WorldTransform;
            }
            set
            {
                GetComponent<TransformComponent>().WorldTransform = value;
            }
        }

        public Vector3 WorldLocation
        {
            get
            {
                return GetComponent<TransformComponent>().WorldLocation;
            }
            set
            {
                GetComponent<TransformComponent>().WorldLocation = value;
            }
        }

        public Rotator WorldRotation
        {
            get
            {
                return GetComponent<TransformComponent>().WorldRotation;
            }
            set
            {
                GetComponent<TransformComponent>().WorldRotation = value;
            }
        }

        public Vector3 WorldScale
        {
            get
            {
                return GetComponent<TransformComponent>().WorldScale;
            }
            set
            {
                GetComponent<TransformComponent>().WorldScale = value;
            }
        }

        public Transform RelativeTransform
        {
            get
            {
                return GetComponent<TransformComponent>().RelativeTransform;
            }
            set
            {
                GetComponent<TransformComponent>().RelativeTransform = value;
            }
        }

        public Vector3 RelativeLocation
        {
            get
            {
                return GetComponent<TransformComponent>().RelativeLocation;
            }
            set
            {
                GetComponent<TransformComponent>().RelativeLocation = value;
            }
        }

        public Rotator RelativeRotation
        {
            get
            {
                return GetComponent<TransformComponent>().RelativeRotation;
            }
            set
            {
                GetComponent<TransformComponent>().RelativeRotation = value;
            }
        }

        public Vector3 RelativeScale
        {
            get
            {
                return GetComponent<TransformComponent>().RelativeScale;
            }
            set
            {
                GetComponent<TransformComponent>().RelativeScale = value;
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

        public void AddCollisionBeginCallback(Action<Entity> callback)
        {
            m_CollisionBeginCallbacks += callback;
        }

        public void RemoveCollisionBeginCallback(Action<Entity> callback)
        {
            m_CollisionBeginCallbacks -= callback;
        }

        public void AddCollisionEndCallback(Action<Entity> callback)
        {
            m_CollisionEndCallbacks += callback;
        }

        public void RemoveCollisionEndCallback(Action<Entity> callback)
        {
            m_CollisionEndCallbacks -= callback;
        }

        public void AddTriggerBeginCallback(Action<Entity> callback)
        {
            m_TriggerBeginCallbacks += callback;
        }

        public void RemoveTriggerBeginCallback(Action<Entity> callback)
        {
            m_TriggerBeginCallbacks -= callback;
        }

        public void AddTriggerEndCallback(Action<Entity> callback)
        {
            m_TriggerEndCallbacks += callback;
        }

        public void RemoveTriggerEndCallback(Action<Entity> callback)
        {
            m_TriggerEndCallbacks -= callback;
        }

        private void OnCollisionBegin(GUID id)
        {
            if (m_CollisionBeginCallbacks != null)
                m_CollisionBeginCallbacks.Invoke(new Entity(id));
        }

        private void OnCollisionEnd(GUID id)
        {
            if (m_CollisionEndCallbacks != null)
                m_CollisionEndCallbacks.Invoke(new Entity(id));
        }

        private void OnTriggerBegin(GUID id)
        {
            if (m_TriggerBeginCallbacks != null)
                m_TriggerBeginCallbacks.Invoke(new Entity(id));
        }

        private void OnTriggerEnd(GUID id)
        {
            if (m_TriggerEndCallbacks != null)
                m_TriggerEndCallbacks.Invoke(new Entity(id));
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

        //C++ Method Implementations
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
        internal static extern bool IsMouseHovered_Native(GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool IsMouseHoveredByCoord_Native(GUID entityID, ref Vector2 mouseCoord);
    }
}
