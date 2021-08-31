using System;
using System.Runtime.CompilerServices;

namespace Eagle
{
    public class Entity
    {
        public ulong ID { get; private set; }

        protected Entity() { ID = 0; }

        internal Entity(ulong id) { ID = id; }

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

        public ulong GetID() { return ID; }

        static public Entity Create()
        {
            return new Entity(CreateEntity_Native());
        }

        public void Destroy()
        {
            DestroyEntity_Native(ID);
        }

        //C++ Method Implementations
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern ulong GetParent_Native(ulong entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetParent_Native(ulong entityID, ulong parentID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern Entity[] GetChildren_Native(ulong entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void AddComponent_Native(ulong entityID, Type type);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool HasComponent_Native(ulong entityID, Type type);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern ulong CreateEntity_Native();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void DestroyEntity_Native(ulong entityID);
    }
}
