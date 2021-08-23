using System;
using System.Runtime.CompilerServices;

namespace Eagle
{
    public abstract class Component
    {
        public Entity Owner { get; set; }
    }

    public class TransformComponent : Component
    {
        public Transform Transform
        {
            get
            {
                GetTransform_Native(Owner.ID, out Transform result);
                return result;
            }
            set
            {
                SetTransform_Native(Owner.ID, ref value);
            }
        }

        public Vector3 Translation
        {
            get
            {
                GetTranslation_Native(Owner.ID, out Vector3 result);
                return result ;
            }
            set
            {
                SetTranslation_Native(Owner.ID, ref value);
            }
        }

        public Vector3 Rotation
        {
            get
            {
                GetRotation_Native(Owner.ID, out Vector3 result);
                return result;
            }
            set
            {
                SetRotation_Native(Owner.ID, ref value);
            }
        }

        public Vector3 Scale
        {
            get
            {
                GetScale_Native(Owner.ID, out Vector3 result);
                return result;
            }
            set
            {
                SetScale_Native(Owner.ID, ref value);
            }
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetTransform_Native(ulong entityID, out Transform outTransform);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetTransform_Native(ulong entityID, ref Transform inTransform);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetTranslation_Native(ulong entityID, out Vector3 outTranslation);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetTranslation_Native(ulong entityID, ref Vector3 inTranslation);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetRotation_Native(ulong entityID, out Vector3 outRotation);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetRotation_Native(ulong entityID, ref Vector3 inRotation);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetScale_Native(ulong entityID, out Vector3 outScale);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetScale_Native(ulong entityID, ref Vector3 inScale);
    }
}
