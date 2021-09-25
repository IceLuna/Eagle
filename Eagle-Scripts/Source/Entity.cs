using System;
using System.Runtime.CompilerServices;

namespace Eagle
{
    public enum ForceMode
    {
        Force = 0,
        Impulse,
        VelocityChange,
        Acceleration
    }
    public enum ActorLockFlag
    {
        LocationX = 1 << 0, LocationY = 1 << 1, LocationZ = 1 << 2, Location = LocationX | LocationY | LocationZ,
		RotationX = 1 << 3, RotationY = 1 << 4, RotationZ = 1 << 5, Rotation = RotationX | RotationY | RotationZ
    };

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

        public Vector3 WorldRotation
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

        public Vector3 RelativeRotation
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

        static public Entity Create()
        {
            return new Entity(CreateEntity_Native());
        }

        public void Destroy()
        {
            DestroyEntity_Native(ID);
        }

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

        public void WakeUp()
        {
            WakeUp_Native(ID);
        }

        public void PutToSleep()
        {
            PutToSleep_Native(ID);
        }

        public float GetMass()
        {
            return GetMass_Native(ID);
        }

        public void SetMass(float mass)
        {
            SetMass_Native(ID, mass);
        }

        public void AddForce(in Vector3 force, ForceMode forceMode)
        {
            AddForce_Native(ID, in force, forceMode);
        }
        public void AddTorque(in Vector3 torque, ForceMode forceMode)
        {
            AddTorque_Native(ID, in torque, forceMode);
        }

        public Vector3 GetLinearVelocity()
        {
            GetLinearVelocity_Native(ID, out Vector3 result);
            return result;
        }
        public void SetLinearVelocity(in Vector3 velocity)
        {
            SetLinearVelocity_Native(ID, in velocity);
        }
		public Vector3 GetAngularVelocity()
        {
            GetAngularVelocity_Native(ID, out Vector3 result);
            return result;
        }
        public void SetAngularVelocity(in Vector3 velocity)
        {
            SetAngularVelocity_Native(ID, in velocity);
        }

		public float GetMaxLinearVelocity()
        {
            return GetMaxLinearVelocity_Native(ID);
        }
        public void SetMaxLinearVelocity(float maxVelocity)
        {
            SetMaxLinearVelocity_Native(ID, maxVelocity);
        }
        public float GetMaxAngularVelocity()
        {
            return GetMaxAngularVelocity_Native(ID);
        }
        public void SetMaxAngularVelocity(float maxVelocity)
        {
            SetMaxAngularVelocity_Native(ID, maxVelocity);
        }

        public void SetLinearDamping(float damping)
        {
            SetLinearDamping_Native(ID, damping);
        }
        public void SetAngularDamping(float damping)
        {
            SetAngularDamping_Native(ID, damping);
        }

        public bool IsDynamic() 
        {
            return IsDynamic_Native(ID);
        }

        public bool IsKinematic()
        {
            return IsKinematic_Native(ID);
        }
        public void SetKinematic(bool bKinematic)
        {
            SetKinematic_Native(ID, bKinematic);
        }

        public bool IsGravityEnabled()
        {
            return IsGravityEnabled_Native(ID);
        }
		public void SetGravityEnabled(bool bEnabled)
        {
            SetGravityEnabled_Native(ID, bEnabled);
        }

        public bool IsLockFlagSet(ActorLockFlag flag)
        {
            return IsLockFlagSet_Native(ID, flag);
        }
		public void SetLockFlag(ActorLockFlag flag, bool value)
        {
            SetLockFlag_Native(ID, flag, value);
        }
        ActorLockFlag GetLockFlags()
        {
            return GetLockFlags_Native(ID);
        }

        public override string ToString()
        {
            return GetEntityName_Native(ID);
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
        internal static extern GUID CreateEntity_Native();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void DestroyEntity_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern string GetEntityName_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void WakeUp_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void PutToSleep_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetMass_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetMass_Native(in GUID entityID, float mass);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void AddForce_Native(in GUID entityID, in Vector3 force, ForceMode forceMode);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void AddTorque_Native(in GUID entityID, in Vector3 force, ForceMode forceMode);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetLinearVelocity_Native(in GUID entityID, out Vector3 result);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetLinearVelocity_Native(in GUID entityID, in Vector3 velocity);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetAngularVelocity_Native(in GUID entityID, out Vector3 result);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetAngularVelocity_Native(in GUID entityID, in Vector3 velocity);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetMaxLinearVelocity_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetMaxLinearVelocity_Native(in GUID entityID, float maxVelocity);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetMaxAngularVelocity_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetMaxAngularVelocity_Native(in GUID entityID, float maxVelocity);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetLinearDamping_Native(in GUID entityID, float damping);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetAngularDamping_Native(in GUID entityID, float damping);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool IsDynamic_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool IsKinematic_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool IsGravityEnabled_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool IsLockFlagSet_Native(in GUID entityID, ActorLockFlag flag);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern ActorLockFlag GetLockFlags_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetKinematic_Native(in GUID entityID, bool value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetGravityEnabled_Native(in GUID entityID, bool value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetLockFlag_Native(in GUID entityID, ActorLockFlag flag, bool value);

    }
}
