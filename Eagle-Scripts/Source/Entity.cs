using System;
using System.Collections.Generic;
using System.Reflection;
using System.Runtime.CompilerServices;

namespace Eagle
{
    public struct CollisionInfo
    {
        public Vector3 Position;
        public Vector3 Normal;
        public Vector3 Impulse;
        public Vector3 Force;
    }

    internal enum ComponentKind
    {
        Engine,  // Declared in Eagle-Scripts, C++ backed, registered in ScriptEngineRegistry
        User,    // Pure C#, stored per-entity by the engine but never exposed to the editor/serializer
        Invalid, // Not a component, or a user type that inherits an engine component (SceneComponent, CameraComponent, ...)
    }

    internal static class ComponentTypeInfo
    {
        private static readonly Assembly s_CoreAssembly = typeof(Component).Assembly;
        private static readonly Dictionary<Type, ComponentKind> s_Cache = new Dictionary<Type, ComponentKind>();

        public static ComponentKind GetKind(Type type)
        {
            if (type == null)
                return ComponentKind.Invalid;

            if (!s_Cache.TryGetValue(type, out ComponentKind kind))
            {
                kind = Classify(type);
                s_Cache[type] = kind;
            }
            return kind;
        }

        public static void LogInvalid(Type type, string functionName)
        {
            string reason;
            if (type == null)
                reason = "Type is null";
            else if (!typeof(Component).IsAssignableFrom(type))
                reason = $"'{type.FullName}' is not a component";
            else
                reason = $"User component '{type.FullName}' must derive from 'Eagle.Component' directly or from another user component. " +
                         $"It inherits engine component '{GetFirstEngineBase(type)?.FullName}'";

            Log.Error($"Failed to call '{functionName}'. {reason}");
        }

        private static ComponentKind Classify(Type type)
        {
            if (!typeof(Component).IsAssignableFrom(type))
                return ComponentKind.Invalid;

            if (type.Assembly == s_CoreAssembly)
                return ComponentKind.Engine;

            // A user component must not inherit engine behaviour (SceneComponent, LightComponent, CameraComponent, ...):
            // those wrappers forward everything to C++ data that doesn't exist for user types.
            return GetFirstEngineBase(type) == typeof(Component) ? ComponentKind.User : ComponentKind.Invalid;
        }

        // Walks up the inheritance chain skipping user classes, returns the first class declared in Eagle-Scripts
        private static Type GetFirstEngineBase(Type type)
        {
            Type engineBase = type.BaseType;
            while (engineBase != null && engineBase.Assembly != s_CoreAssembly)
                engineBase = engineBase.BaseType;
            return engineBase;
        }
    }

    internal static class ComponentTypeInfo<T> where T : Component
    {
        public static readonly ComponentKind Kind = ComponentTypeInfo.GetKind(typeof(T));
    }

    public class Entity
    {
        // First `Entity` is an entity that owns a callback
        private Action<Entity, Entity, CollisionInfo> m_CollisionBeginCallbacks;
        private Action<Entity, Entity, CollisionInfo> m_CollisionEndCallbacks;
        private Action<Entity, Entity> m_TriggerBeginCallbacks;
        private Action<Entity, Entity> m_TriggerEndCallbacks;
        // Entity - entity that took damage;
        // Entity - damage source;
        // float - the damage it actually took
        protected Action<Entity, Entity, float> m_OnTookDamageCallbacks;

        public GUID ID { get; private set; }

        protected Entity() {  }

        internal Entity(GUID id) { ID = id; }

        public virtual void OnCreate() { }

        public virtual void OnDestroy() { }

        public virtual void OnUpdate(float ts) { }

        public virtual void OnPhysicsUpdate(float ts) { }

        public virtual void OnEvent(Event e) { }

        public virtual void OnAnimationEvent(string eventName, float time) { }

        // Called when a scene sequence played by this entity's Scene Sequence component passes an event key.
        // @time. Time of the key inside the sequence
        public virtual void OnSequenceEvent(string eventName, float time) { }

        // Returns actual damage that was taken
        public virtual float TakeDamage(Entity source, float damage)
        {
            if (m_OnTookDamageCallbacks != null)
                m_OnTookDamageCallbacks.Invoke(this, source, damage);
            return damage;
        }

        public Entity Parent
        {
            get => GetParent_Native(ID);
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

        public string Tag
        {
            get
            {
                return GetTag_Native(ID);
            }
            set
            {
                SetTag_Native(ID, value);
            }
        }

        public T AddComponent<T>() where T : Component, new()
        {
            ComponentKind kind = ComponentTypeInfo<T>.Kind;
            if (kind == ComponentKind.Invalid)
            {
                ComponentTypeInfo.LogInvalid(typeof(T), nameof(AddComponent));
                return null;
            }

            if (kind == ComponentKind.User)
            {
                // Same semantics as engine components: adding twice is an error and you get the existing one back.
                if (HasUserComponent_Native(ID, typeof(T)))
                {
                    Log.Error($"Couldn't add component '{typeof(T).Name}' to '{GetName()}'. This component already exists!");
                    return GetUserComponent_Native(ID, typeof(T)) as T;
                }

                T userComponent = new T();
                userComponent.Parent = this;
                return AddUserComponent_Native(ID, userComponent) ? userComponent : null;
            }

            AddComponent_Native(ID, typeof(T));
            T component = new T();
            component.Parent = this;
            return component;
        }

        public void RemoveComponent<T>() where T : Component, new()
        {
            switch (ComponentTypeInfo<T>.Kind)
            {
                case ComponentKind.User:   RemoveUserComponent_Native(ID, typeof(T)); break;
                case ComponentKind.Engine: RemoveComponent_Native(ID, typeof(T)); break;
                default: ComponentTypeInfo.LogInvalid(typeof(T), nameof(RemoveComponent)); break;
            }
        }

        public bool HasComponent<T>() where T : Component, new()
        {
            return HasComponent(ComponentTypeInfo<T>.Kind, typeof(T));
        }

        public bool HasComponent(Type type)
        {
            return HasComponent(ComponentTypeInfo.GetKind(type), type);
        }

        private bool HasComponent(ComponentKind kind, Type type)
        {
            switch (kind)
            {
                case ComponentKind.User:   return HasUserComponent_Native(ID, type);
                case ComponentKind.Engine: return HasComponent_Native(ID, type);
                default:
                    ComponentTypeInfo.LogInvalid(type, nameof(HasComponent));
                    return false;
            }
        }

        public T GetComponent<T>() where T : Component, new()
        {
            ComponentKind kind = ComponentTypeInfo<T>.Kind;
            if (kind == ComponentKind.Invalid)
            {
                ComponentTypeInfo.LogInvalid(typeof(T), nameof(GetComponent));
                return null;
            }

            if (kind == ComponentKind.User)
                return GetUserComponent_Native(ID, typeof(T)) as T;

            if (HasComponent_Native(ID, typeof(T)))
            {
                T component = new T();
                component.Parent = this;
                return component;
            }
            return null;
        }

        public bool IsValid() { return IsValid_Native(ID); }

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
        public bool IsMouseHovered(Vector2 mouseCoord) { return IsMouseHoveredByCoord_Native(ID, ref mouseCoord); }

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

        public void AddOnTookDamageCallback(Action<Entity, Entity, float> callback)
        {
            m_OnTookDamageCallbacks += callback;
        }

        public void RemoveOnTookDamageCallback(Action<Entity,   Entity, float> callback)
        {
            m_OnTookDamageCallbacks -= callback;
        }

        private void OnCollisionBegin(Entity otherEntity, Vector3 position, Vector3 normal, Vector3 impulse, Vector3 force)
        {
            if (m_CollisionBeginCallbacks != null)
            {
                CollisionInfo collisionInfo = new CollisionInfo();
                collisionInfo.Position = position;
                collisionInfo.Normal = normal;
                collisionInfo.Impulse = impulse;
                collisionInfo.Force = force;
                m_CollisionBeginCallbacks.Invoke(this, otherEntity, collisionInfo);
            }
        }

        private void OnCollisionEnd(Entity otherEntity, Vector3 position, Vector3 normal, Vector3 impulse, Vector3 force)
        {
            if (m_CollisionEndCallbacks != null)
            {
                CollisionInfo collisionInfo = new CollisionInfo();
                collisionInfo.Position = position;
                collisionInfo.Normal = normal;
                collisionInfo.Impulse = impulse;
                collisionInfo.Force = force;
                m_CollisionEndCallbacks.Invoke(this, otherEntity, collisionInfo);
            }
        }

        private void OnTriggerBegin(Entity otherEntity)
        {
            if (m_TriggerBeginCallbacks != null)
                m_TriggerBeginCallbacks.Invoke(this, otherEntity);
        }

        private void OnTriggerEnd(Entity otherEntity)
        {
            if (m_TriggerEndCallbacks != null)
                m_TriggerEndCallbacks.Invoke(this, otherEntity);
        }

        public string GetName()
        {
            return GetEntityName_Native(ID);
        }

        public override string ToString()
        {
            return GetName();
        }

        public Entity GetChildrenByName(string name)
        {
            return GetChildrenByName_Native(ID, name);
        }

        public override bool Equals(object obj) => obj is Entity other && ID == other.ID;

        public static bool operator ==(Entity obj1, Entity obj2)
        {
            if (ReferenceEquals(obj1, null))
            {
                return ReferenceEquals(obj2, null);
            }
            return obj1.Equals(obj2); // Delegate to Equals method
        }

        public static bool operator !=(Entity left, Entity right) => !(left == right);

        public override int GetHashCode() => ID.GetHashCode();

        // C++ Method Implementations
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern Entity GetParent_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetParent_Native(in GUID entityID, GUID parentID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern Entity[] GetChildren_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void AddComponent_Native(in GUID entityID, Type type);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void RemoveComponent_Native(in GUID entityID, Type type);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool HasComponent_Native(in GUID entityID, Type type);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool AddUserComponent_Native(in GUID entityID, Component component);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void RemoveUserComponent_Native(in GUID entityID, Type type);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool HasUserComponent_Native(in GUID entityID, Type type);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern Component GetUserComponent_Native(in GUID entityID, Type type);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool IsValid_Native(in GUID entityID);

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
        internal static extern Entity GetChildrenByName_Native(in GUID entityID, string name);

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

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetTag_Native(in GUID entityID, string tag);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern string GetTag_Native(in GUID entityID);
    }
}
