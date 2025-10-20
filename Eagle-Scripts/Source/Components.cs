using System;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using static Eagle.CrowdNavigation;

namespace Eagle
{
    public enum CameraProjectionMode
    {
        Perspective = 0,
		Orthographic = 1
	};

    public enum PhysicsBodyType
    {
        Static = 0,
        Dynamic
    }

    public enum PhysicsQueryType
    {
        [Tooltip("Traverse static body types")]
        Static = 1 << 0,
        [Tooltip("Traverse dynamic body types")]
        Dynamic = 1 << 1,
        [UIName("Any hit")]
        [Tooltip("Abort traversal as soon as any hit is found")]
        AnyHit  = 1 << 2,

        [UIName("Static & Dynamic")]
        Default = Static | Dynamic
    }

    public enum CollisionDetectionType
    {
        Discrete,
		Continuous,
        [UIName("Continuous Speculative")] ContinuousSpeculative
    }

    public enum CollisionGroup : uint
    {
        Object = 1 << 0,
		Projectile = 1 << 1,
        Any = 0xFFFFFFFF,
    };

    public enum ForceMode
    {
        Force = 0,
        Impulse,
        [UIName("Velocity change")] VelocityChange,
        Acceleration
    }

    public enum ActorLockFlag
    {
        LocationX = 1 << 0, LocationY = 1 << 1, LocationZ = 1 << 2, Location = LocationX | LocationY | LocationZ,
        RotationX = 1 << 3, RotationY = 1 << 4, RotationZ = 1 << 5, Rotation = RotationX | RotationY | RotationZ
    }

    public enum RootMotionLockFlag
    {
        None = 0,
        PositionX = 1 << 0, PositionY = 1 << 1, PositionZ = 1 << 2,
        Position = PositionX | PositionY | PositionZ
    }

    public enum AnimationType
    {
        Clip,
		Graph
    }

    public abstract class Component
    {
        public Entity Parent { get; internal set; }
    }

    public abstract class SceneComponent : Component
    {
        protected Type m_Type = typeof(SceneComponent);

        public Transform WorldTransform
        {
            get
            {
                GetWorldTransform_Native(Parent.ID, m_Type, out Transform result);
                return result;
            }
            set
            {
                SetWorldTransform_Native(Parent.ID, m_Type, ref value);
            }
        }

        public Vector3 WorldLocation
        {
            get
            {
                GetWorldLocation_Native(Parent.ID, m_Type, out Vector3 result);
                return result;
            }
            set
            {
                SetWorldLocation_Native(Parent.ID, m_Type, ref value);
            }
        }

        public Rotator WorldRotation
        {
            get
            {
                GetWorldRotation_Native(Parent.ID, m_Type, out Rotator result);
                return result;
            }
            set
            {
                SetWorldRotation_Native(Parent.ID, m_Type, ref value);
            }
        }

        public Vector3 WorldScale
        {
            get
            {
                GetWorldScale_Native(Parent.ID, m_Type, out Vector3 result);
                return result;
            }
            set
            {
                SetWorldScale_Native(Parent.ID, m_Type, ref value);
            }
        }

        public Transform RelativeTransform
        {
            get
            {
                GetRelativeTransform_Native(Parent.ID, m_Type, out Transform result);
                return result;
            }
            set
            {
                SetRelativeTransform_Native(Parent.ID, m_Type, ref value);
            }
        }

        public Vector3 RelativeLocation
        {
            get
            {
                GetRelativeLocation_Native(Parent.ID, m_Type, out Vector3 result);
                return result;
            }
            set
            {
                SetRelativeLocation_Native(Parent.ID, m_Type, ref value);
            }
        }

        public Rotator RelativeRotation
        {
            get
            {
                GetRelativeRotation_Native(Parent.ID, m_Type, out Rotator result);
                return result;
            }
            set
            {
                SetRelativeRotation_Native(Parent.ID, m_Type, ref value);
            }
        }

        public Vector3 RelativeScale
        {
            get
            {
                GetRelativeScale_Native(Parent.ID, m_Type, out Vector3 result);
                return result;
            }
            set
            {
                SetRelativeScale_Native(Parent.ID, m_Type, ref value);
            }
        }

        public Vector3 GetForwardVector()
        {
            GetForwardVector_Native(Parent.ID, m_Type, out Vector3 result);
            return result;
        }

        public Vector3 GetRightVector()
        {
            GetRightVector_Native(Parent.ID, m_Type, out Vector3 result);
            return result;
        }

        public Vector3 GetUpVector()
        {
            GetUpVector_Native(Parent.ID, m_Type, out Vector3 result);
            return result;
        }

        //---World functions---
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetWorldTransform_Native(in GUID entityID, Type type, out Transform outTransform);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetWorldTransform_Native(in GUID entityID, Type type, ref Transform inTransform);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetWorldLocation_Native(in GUID entityID, Type type, out Vector3 outLocation);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetWorldLocation_Native(in GUID entityID, Type type, ref Vector3 inLocation);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetWorldRotation_Native(in GUID entityID, Type type, out Rotator outRotation);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetWorldRotation_Native(in GUID entityID, Type type, ref Rotator inRotation);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetWorldScale_Native(in GUID entityID, Type type, out Vector3 outScale);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetWorldScale_Native(in GUID entityID, Type type, ref Vector3 inScale);

        //---Relative functions---
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetRelativeTransform_Native(in GUID entityID, Type type, out Transform outTransform);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetRelativeTransform_Native(in GUID entityID, Type type, ref Transform inTransform);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetRelativeLocation_Native(in GUID entityID, Type type, out Vector3 outLocation);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetRelativeLocation_Native(in GUID entityID, Type type, ref Vector3 inLocation);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetRelativeRotation_Native(in GUID entityID, Type type, out Rotator outRotation);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetRelativeRotation_Native(in GUID entityID, Type type, ref Rotator inRotation);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetRelativeScale_Native(in GUID entityID, Type type, out Vector3 outScale);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetRelativeScale_Native(in GUID entityID, Type type, ref Vector3 inScale);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetForwardVector_Native(in GUID entityID, Type type, out Vector3 outScale);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetRightVector_Native(in GUID entityID, Type type, out Vector3 outScale);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetUpVector_Native(in GUID entityID, Type type, out Vector3 outScale);
    }

    public class CameraComponent : SceneComponent
    {
        public CameraComponent()
        {
            m_Type = typeof(CameraComponent);
        }

        public bool IsPrimary
        {
            get { return GetIsPrimary_Native(Parent.ID); }
            set { SetIsPrimary_Native(Parent.ID, value); }
        }

        public float PerspectiveVerticalFOV
        {
            get { return GetPerspectiveVerticalFOV_Native(Parent.ID); }
            set { SetPerspectiveVerticalFOV_Native(Parent.ID, value); }
        }
        
        public float PerspectiveNearClip
        {
            get { return GetPerspectiveNearClip_Native(Parent.ID); }
            set { SetPerspectiveNearClip_Native(Parent.ID, value); }
        }
        
        public float PerspectiveFarClip
        {
            get { return GetPerspectiveFarClip_Native(Parent.ID); }
            set { SetPerspectiveFarClip_Native(Parent.ID, value); }
        }
        
        public float ShadowFarClip
        {
            get { return GetShadowFarClip_Native(Parent.ID); }
            set { SetShadowFarClip_Native(Parent.ID, value); }
        }
        
        public float CascadesSplitAlpha
        {
            get { return GetCascadesSplitAlpha_Native(Parent.ID); }
            set { SetCascadesSplitAlpha_Native(Parent.ID, value); }
        }
        
        public float CascadesSmoothTransitionAlpha
        {
            get { return GetCascadesSmoothTransitionAlpha_Native(Parent.ID); }
            set { SetCascadesSmoothTransitionAlpha_Native(Parent.ID, value); }
        }

        public float GetAspectRatio()
        {
            return GetAspectRatio_Native(Parent.ID);
        }

        public CameraProjectionMode ProjectionMode
        {
            get { return GetCameraProjectionMode_Native(Parent.ID); }
            set { SetCameraProjectionMode_Native(Parent.ID, value); }
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool GetIsPrimary_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetIsPrimary_Native(in GUID entityID, bool value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetPerspectiveVerticalFOV_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetPerspectiveVerticalFOV_Native(in GUID entityID, float value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetPerspectiveNearClip_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetPerspectiveNearClip_Native(in GUID entityID, float value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetPerspectiveFarClip_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetPerspectiveFarClip_Native(in GUID entityID, float value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetShadowFarClip_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetShadowFarClip_Native(in GUID entityID, float value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetCascadesSplitAlpha_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetCascadesSplitAlpha_Native(in GUID entityID, float value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetCascadesSmoothTransitionAlpha_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetCascadesSmoothTransitionAlpha_Native(in GUID entityID, float value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern CameraProjectionMode GetCameraProjectionMode_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetCameraProjectionMode_Native(in GUID entityID, CameraProjectionMode value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetAspectRatio_Native(in GUID entityID);
    }

    public abstract class LightComponent : SceneComponent
    {
        public LightComponent()
        {
            m_Type = typeof(LightComponent);
        }

        public Color3 LightColor
        {
            get
            {
                GetLightColor_Native(Parent.ID, m_Type, out Color3 result);
                return result;
            }
            set
            {
                SetLightColor_Native(Parent.ID, m_Type, ref value);
            }
        }
        
        public float Intensity
        {
            get
            {
                return GetIntensity_Native(Parent.ID, m_Type);
            }
            set
            {
                SetIntensity_Native(Parent.ID, m_Type, value);
            }
        }

        public float VolumetricFogIntensity
        {
            get
            {
                return GetVolumetricFogIntensity_Native(Parent.ID, m_Type);
            }
            set
            {
                SetVolumetricFogIntensity_Native(Parent.ID, m_Type, value);
            }
        }

        public bool bAffectsWorld
        {
            get
            {
                return GetAffectsWorld_Native(Parent.ID, m_Type);
            }
            set
            {
                SetAffectsWorld_Native(Parent.ID, m_Type, value);
            }
        }

        public bool bCastsShadows
        {
            get
            {
                return GetCastsShadows_Native(Parent.ID, m_Type);
            }
            set
            {
                SetCastsShadows_Native(Parent.ID, m_Type, value);
            }
        }

        public bool bVolumetricLight
        {
            get
            {
                return GetIsVolumetricLight_Native(Parent.ID, m_Type);
            }
            set
            {
                SetIsVolumetricLight_Native(Parent.ID, m_Type, value);
            }
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetLightColor_Native(in GUID entityID, Type type, out Color3 outLightColor);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetIntensity_Native(in GUID entityID, Type type);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetVolumetricFogIntensity_Native(in GUID entityID, Type type);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool GetAffectsWorld_Native(in GUID entityID, Type type);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool GetCastsShadows_Native(in GUID entityID, Type type);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool GetIsVolumetricLight_Native(in GUID entityID, Type type);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetLightColor_Native(in GUID entityID, Type type, ref Color3 lightColor);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetIntensity_Native(in GUID entityID, Type type, float intensity);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetVolumetricFogIntensity_Native(in GUID entityID, Type type, float intensity);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetAffectsWorld_Native(in GUID entityID, Type type, bool value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetCastsShadows_Native(in GUID entityID, Type type, bool value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetIsVolumetricLight_Native(in GUID entityID, Type type, bool value);
    }

    public class PointLightComponent : LightComponent
    {
        public PointLightComponent()
        {
            m_Type = typeof(PointLightComponent);
        }

        public float Radius
        {
            get
            {
                return GetRadius_Native(Parent.ID);
            }
            set
            {
                SetRadius_Native(Parent.ID, ref value);
            }
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetRadius_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetRadius_Native(in GUID entityID, ref float intensity);
    }

    public class DirectionalLightComponent : LightComponent
    {
        public DirectionalLightComponent()
        {
            m_Type = typeof(DirectionalLightComponent);
        }

        public Color3 Ambient
        {
            get
            {
                GetAmbient_Native(Parent.ID, out Color3 result);
                return result;
            }
            set
            {
                SetAmbient_Native(Parent.ID, ref value);
            }
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetAmbient_Native(in GUID entityID, out Color3 outAmbient);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetAmbient_Native(in GUID entityID, ref Color3 ambient);
    }

    public class SpotLightComponent : LightComponent
    {
        public SpotLightComponent()
        {
            m_Type = typeof(SpotLightComponent);
        }

        public float InnerCutoffAngle
        {
            get
            {
                return GetInnerCutoffAngle_Native(Parent.ID);
            }
            set
            {
                SetInnerCutoffAngle_Native(Parent.ID, ref value);
            }
        }
        public float OuterCutoffAngle
        {
            get
            {
                return GetOuterCutoffAngle_Native(Parent.ID);
            }
            set
            {
                SetOuterCutoffAngle_Native(Parent.ID, ref value);
            }
        }
        public float Distance
        {
            get
            {
                return GetDistance_Native(Parent.ID);
            }
            set
            {
                SetDistance_Native(Parent.ID, ref value);
            }
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetInnerCutoffAngle_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetOuterCutoffAngle_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetDistance_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetInnerCutoffAngle_Native(in GUID entityID, ref float innerCutoffAngle);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetOuterCutoffAngle_Native(in GUID entityID, ref float outerCutoffAngle);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetDistance_Native(in GUID entityID, ref float distance);
    }

    public class StaticMeshComponent : SceneComponent
    {
        public StaticMeshComponent()
        {
            m_Type = typeof(StaticMeshComponent);
        }

        public AssetStaticMesh MeshAsset
        {
            get
            {
                GUID assetID = GetMesh_Native(Parent.ID);
                if (assetID.IsNull())
                    return null;
                return new AssetStaticMesh(assetID);
            }

            set
            {
                SetMesh_Native(Parent.ID, value != null ? value.GetGUID() : GUID.Null());
            }
        }

        public bool bCastsShadows
        {
            get { return DoesCastShadows_Native(Parent.ID); }
            set { SetCastsShadows_Native(Parent.ID, value); }
        }

        public bool bReceivesDecals
        {
            get { return DoesReceiveDecals_Native(Parent.ID); }
            set { SetReceivesDecals_Native(Parent.ID, value); }
        }

        public AssetMaterial GetMaterialAsset(uint index)
        {
            GetMaterial_Native(Parent.ID, index, out GUID assetID);
            if (assetID.IsNull())
                return null;

            return new AssetMaterial(assetID);
        }

        public void SetMaterialAsset(uint index, AssetMaterial value)
        {
            SetMaterial_Native(Parent.ID, index, (value != null) ? value.GetGUID() : GUID.Null());
        }

        public uint GetMaterialsSlotsCount()
        {
            return GetMaterialsSlotsCount_Native(Parent.ID);
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetMesh_Native(in GUID entityID, GUID meshGUID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern GUID GetMesh_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetMaterial_Native(in GUID entityID, uint index, out GUID assetID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetMaterial_Native(in GUID entityID, uint index, in GUID assetID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetCastsShadows_Native(in GUID entityID, bool value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool DoesCastShadows_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetReceivesDecals_Native(in GUID entityID, bool value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool DoesReceiveDecals_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern uint GetMaterialsSlotsCount_Native(in GUID entityID);
    }

    public class SkeletalMeshComponent : SceneComponent
    {
        public SkeletalMeshComponent()
        {
            m_Type = typeof(SkeletalMeshComponent);
        }

        public AssetSkeletalMesh MeshAsset
        {
            get
            {
                GUID assetID = GetMesh_Native(Parent.ID);
                if (assetID.IsNull())
                    return null;
                return new AssetSkeletalMesh(assetID);
            }

            set
            {
                SetMesh_Native(Parent.ID, value != null ? value.GetGUID() : GUID.Null());
            }
        }

        public bool bCastsShadows
        {
            get { return DoesCastShadows_Native(Parent.ID); }
            set { SetCastsShadows_Native(Parent.ID, value); }
        }

        public bool bReceivesDecals
        {
            get { return DoesReceiveDecals_Native(Parent.ID); }
            set { SetReceivesDecals_Native(Parent.ID, value); }
        }

        public AssetMaterial GetMaterialAsset(uint index)
        {
            GetMaterial_Native(Parent.ID, index, out GUID assetID);
            if (assetID.IsNull())
                return null;

            return new AssetMaterial(assetID);
        }

        public void SetMaterialAsset(AssetMaterial value, uint index)
        {
            SetMaterial_Native(Parent.ID, index, (value != null) ? value.GetGUID() : GUID.Null());
        }

        public uint GetMaterialsSlotsCount()
        {
            return GetMaterialsSlotsCount_Native(Parent.ID);
        }

        public AssetAnimation GetAnimationAsset()
        {
            GetAnimation_Native(Parent.ID, out GUID assetID);
            if (assetID.IsNull())
                return null;

            return new AssetAnimation(assetID);
        }

        public void SetAnimationAsset(AssetAnimation value)
        {
            SetAnimation_Native(Parent.ID, (value != null) ? value.GetGUID() : GUID.Null());
        }
        
        public AssetAnimationGraph GetAnimationGraphAsset()
        {
            GetAnimationGraph_Native(Parent.ID, out GUID assetID);
            if (assetID.IsNull())
                return null;

            return new AssetAnimationGraph(assetID);
        }

        public void SetAnimationAssetGraph(AssetAnimationGraph value)
        {
            SetAnimationGraph_Native(Parent.ID, (value != null) ? value.GetGUID() : GUID.Null());
        }

        public void SetRagdollEnabled(bool bEnabled)
        {
            SetRagdollEnabled_Native(Parent.ID, bEnabled);
        }

        public void SetRagdollCollisionVisible(bool bVisible)
        {
            SetRagdollCollisionVisible_Native(Parent.ID, bVisible);
        }

        public bool IsRagdollEnabled()
        {
            return IsRagdollEnabled_Native(Parent.ID);
        }

        public Transform GetRagdollBoneWorldTransform(string name)
        {
            Transform result;
            GetRagdollBoneWorldTransform_Native(Parent.ID, name, out result);
            return result;
        }

        // Update all bones
        public void SetRagdollLinearVelocity(Vector3 velocity)
        {
            SetRagdollLinearVelocity_Native(Parent.ID, ref velocity);
        }

		public void SetRagdollAngularVelocity(Vector3 velocity)
        {
            SetRagdollAngularVelocity_Native(Parent.ID, ref velocity);
        }

        public void AddRagdollForce(Vector3 force, ForceMode forceMode)
        {
            AddRagdollForce_Native(Parent.ID, ref force, forceMode);
        }

        public void AddRagdollTorque(Vector3 torque, ForceMode forceMode)
        {
            AddRagdollTorque_Native(Parent.ID, ref torque, forceMode);
        }

        // Update specific bones
        public void SetRagdollBoneLinearVelocity(string boneName, Vector3 velocity)
        {
            SetRagdollBoneLinearVelocity_Native(Parent.ID, boneName, ref velocity);
        }

		public void SetRagdollBoneAngularVelocity(string boneName, Vector3 velocity)
        {
            SetRagdollBoneAngularVelocity_Native(Parent.ID, boneName, ref velocity);
        }

        public Vector3 GetRagdollBoneLinearVelocity(string boneName)
        {
            GetRagdollBoneLinearVelocity_Native(Parent.ID, boneName, out Vector3 velocity);
            return velocity;
        }

        public Vector3 GetRagdollBoneAngularVelocity(string boneName)
        {
            GetRagdollBoneAngularVelocity_Native(Parent.ID, boneName, out Vector3 velocity);
            return velocity;
        }

        public void AddRagdollBoneForce(string boneName, Vector3 force, ForceMode forceMode)
        {
            AddRagdollBoneForce_Native(Parent.ID, boneName, ref force, forceMode);
        }

        public void AddRagdollBoneTorque(string boneName, Vector3 torque, ForceMode forceMode)
        {
            AddRagdollBoneTorque_Native(Parent.ID, boneName, ref torque, forceMode);
        }

        public void PutRagdollToSleep()
        {
            PutRagdollToSleep_Native(Parent.ID);
        }

        public void WakeUpRagdoll()
        {
            WakeUpRagdoll_Native(Parent.ID);
        }

        public Transform GetBoneWorldTransform(string name)
        {
            Transform result;
            GetBoneWorldTransform_Native(Parent.ID, name, out result);
            return result;
        }

        public Vector3 GetBoneWorldLocation(string name)
        {
            Vector3 result;
            GetBoneWorldLocation_Native(Parent.ID, name, out result);
            return result;
        }

        public Rotator GetBoneWorldRotation(string name)
        {
            Rotator result;
            GetBoneWorldRotation_Native(Parent.ID, name, out result);
            return result;
        }

        public Vector3 GetBoneWorldScale(string name)
        {
            Vector3 result;
            GetBoneWorldScale_Native(Parent.ID, name, out result);
            return result;
        }

        public AnimationType AnimType
        {
            get { return GetAnimType_Native(Parent.ID); }
            set { SetAnimType_Native(Parent.ID, value); }
        }

        // Used only if `AnimType` == `AnimationType::Clip`
        public float CurrentClipPlayTime
        {
            get { return GetCurrentClipPlayTime_Native(Parent.ID); }
            set { SetCurrentClipPlayTime_Native(Parent.ID, value); }
        }

        // Used only if `AnimType` == `AnimationType::Clip`
        public float ClipPlaybackSpeed
        {
            get { return GetClipPlaybackSpeed_Native(Parent.ID); }
            set { SetClipPlaybackSpeed_Native(Parent.ID, value); }
        }

        // Used only if `AnimType` == `AnimationType::Clip`
        public bool bClipLooping
        {
            get { return IsClipLooping_Native(Parent.ID); }
            set { SetIsClipLooping_Native(Parent.ID, value); }
        }

        // Used only if `AnimType` == `AnimationType::Graph`
        public void SetAnimGraphVariable(string name, bool value)
        {
            SetAnimGraphVariableBool_Native(Parent.ID, name, value);
        }

        // Used only if `AnimType` == `AnimationType::Graph`
        public void SetAnimGraphVariable(string name, float value)
        {
            SetAnimGraphVariableFloat_Native(Parent.ID, name, value);
        }

        // Used only if `AnimType` == `AnimationType::Graph`
        public void SetAnimGraphVariable(string name, int value)
        {
            SetAnimGraphVariableInt_Native(Parent.ID, name, value);
        }

        // Used only if `AnimType` == `AnimationType::Graph`
        public void SetAnimGraphVariable(string name, AssetAnimation value)
        {
            SetAnimGraphVariableAnim_Native(Parent.ID, name, value != null ? value.GetGUID() : GUID.Null());
        }

        // Used only if `AnimType` == `AnimationType::Graph`
        public void SetAnimGraphVariable(string name, string value)
        {
            SetAnimGraphVariableString_Native(Parent.ID, name, value);
        }

        // Used only if `AnimType` == `AnimationType::Graph`
        public void SetAnimGraphVariable(string name, Vector4 value)
        {
            SetAnimGraphVariableVec4_Native(Parent.ID, name, ref value);
        }

        // Used only if `AnimType` == `AnimationType::Graph`
        public bool GetAnimGraphVariableBool(string name)
        {
            return GetAnimGraphVariableBool_Native(Parent.ID, name);
        }

        // Used only if `AnimType` == `AnimationType::Graph`
        public int GetAnimGraphVariableInt(string name)
        {
            return GetAnimGraphVariableInt_Native(Parent.ID, name);
        }

        // Used only if `AnimType` == `AnimationType::Graph`
        public float GetAnimGraphVariableFloat(string name)
        {
            return GetAnimGraphVariableFloat_Native(Parent.ID, name);
        }

        // Used only if `AnimType` == `AnimationType::Graph`
        public AssetAnimation GetAnimGraphVariableAnimation(string name)
        {
            GUID animGuid = GetAnimGraphVariableAnim_Native(Parent.ID, name);
            if (animGuid.IsNull())
                return null;

            return new AssetAnimation(animGuid);
        }

        // Used only if `AnimType` == `AnimationType::Graph`
        public string GetAnimGraphVariableString(string name)
        {
            return GetAnimGraphVariableString_Native(Parent.ID, name);
        }

        // Used only if `AnimType` == `AnimationType::Graph`
        public Vector4 GetAnimGraphVariableVec4(string name)
        {
            GetAnimGraphVariableVec4_Native(Parent.ID, name, out Vector4 result);
            return result;
        }

        public bool IsRootMotionLockFlagSet(RootMotionLockFlag flag)
        {
            return IsRootMotionLockFlagSet_Native(Parent.ID, flag);
        }

        public void SetRootMotionLockFlag(RootMotionLockFlag flag, bool value)
        {
            SetRootMotionLockFlagBool_Native(Parent.ID, flag, value);
        }

        public void SetRootMotionLockFlag(RootMotionLockFlag flag)
        {
            SetRootMotionLockFlag_Native(Parent.ID, flag);
        }

        public RootMotionLockFlag GetRootMotionLockFlags()
        {
            return GetRootMotionLockFlags_Native(Parent.ID);
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetReceivesDecals_Native(in GUID entityID, bool value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool DoesReceiveDecals_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetMesh_Native(in GUID entityID, GUID meshGUID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern GUID GetMesh_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetMaterial_Native(in GUID entityID, uint index, out GUID assetID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetMaterial_Native(in GUID entityID, uint index, in GUID assetID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetAnimation_Native(in GUID entityID, out GUID assetID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetAnimation_Native(in GUID entityID, in GUID assetID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetAnimationGraph_Native(in GUID entityID, out GUID assetID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetAnimationGraph_Native(in GUID entityID, in GUID assetID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetRagdollEnabled_Native(in GUID entityID, bool bEnabled);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetRagdollCollisionVisible_Native(in GUID entityID, bool bEnabled);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool IsRagdollEnabled_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetCastsShadows_Native(in GUID entityID, bool value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetCurrentClipPlayTime_Native(in GUID entityID, float value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetClipPlaybackSpeed_Native(in GUID entityID, float value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetIsClipLooping_Native(in GUID entityID, bool value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool DoesCastShadows_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetCurrentClipPlayTime_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetClipPlaybackSpeed_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool IsClipLooping_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetAnimGraphVariableBool_Native(in GUID entityID, string name, bool value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetAnimGraphVariableInt_Native(in GUID entityID, string name, int value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetAnimGraphVariableFloat_Native(in GUID entityID, string name, float value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetAnimGraphVariableAnim_Native(in GUID entityID, string name, in GUID animID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetAnimGraphVariableString_Native(in GUID entityID, string name, string value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetAnimGraphVariableVec4_Native(in GUID entityID, string name, ref Vector4 value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool GetAnimGraphVariableBool_Native(in GUID entityID, string name);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int GetAnimGraphVariableInt_Native(in GUID entityID, string name);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetAnimGraphVariableFloat_Native(in GUID entityID, string name);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern GUID GetAnimGraphVariableAnim_Native(in GUID entityID, string name);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern string GetAnimGraphVariableString_Native(in GUID entityID, string name);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetAnimGraphVariableVec4_Native(in GUID entityID, string name, out Vector4 result);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern AnimationType GetAnimType_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetAnimType_Native(in GUID entityID, AnimationType value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetRagdollBoneWorldTransform_Native(in GUID entityID, string name, out Transform result);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetRagdollLinearVelocity_Native(in GUID entityID, ref Vector3 velocity);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetRagdollAngularVelocity_Native(in GUID entityID, ref Vector3 velocity);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void AddRagdollForce_Native(in GUID entityID, ref Vector3 force, ForceMode forceMode);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void AddRagdollTorque_Native(in GUID entityID, ref Vector3 force, ForceMode forceMode);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetRagdollBoneLinearVelocity_Native(in GUID entityID, string boneName, ref Vector3 velocity);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetRagdollBoneAngularVelocity_Native(in GUID entityID, string boneName, ref Vector3 velocity);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetRagdollBoneLinearVelocity_Native(in GUID entityID, string boneName, out Vector3 velocity);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetRagdollBoneAngularVelocity_Native(in GUID entityID, string boneName, out Vector3 velocity);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void AddRagdollBoneForce_Native(in GUID entityID, string boneName, ref Vector3 force, ForceMode forceMode);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void AddRagdollBoneTorque_Native(in GUID entityID, string boneName, ref Vector3 force, ForceMode forceMode);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void PutRagdollToSleep_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void WakeUpRagdoll_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetBoneWorldTransform_Native(in GUID entityID, string name, out Transform result);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetBoneWorldLocation_Native(in GUID entityID, string name, out Vector3 result);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetBoneWorldRotation_Native(in GUID entityID, string name, out Rotator result);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetBoneWorldScale_Native(in GUID entityID, string name, out Vector3 result);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern uint GetMaterialsSlotsCount_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool IsRootMotionLockFlagSet_Native(GUID entityID, RootMotionLockFlag value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetRootMotionLockFlagBool_Native(GUID entityID, RootMotionLockFlag flag, bool value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetRootMotionLockFlag_Native(GUID entityID, RootMotionLockFlag value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern RootMotionLockFlag GetRootMotionLockFlags_Native(GUID entityID);
    }

    public class SpriteComponent : SceneComponent
    {
        public SpriteComponent()
        {
            m_Type = typeof(SpriteComponent);
        }

        public AssetMaterial GetMaterialAsset()
        {
            GetMaterial_Native(Parent.ID, out GUID assetID);
            if (assetID.IsNull())
                return null;

            return new AssetMaterial(assetID);
        }

        public void SetMaterialAsset(AssetMaterial value)
        {
            SetMaterial_Native(Parent.ID, (value != null) ? value.GetGUID() : GUID.Null());
        }

        public Vector2 AtlasSpriteCoords
        {
            get { GetAtlasSpriteCoords_Native(Parent.ID, out Vector2 result); return result; }
            set { SetAtlasSpriteCoords_Native(Parent.ID, ref value); }
        }

        public Vector2 AtlasSpriteSize
        {
            get { GetAtlasSpriteSize_Native(Parent.ID, out Vector2 result); return result; }
            set { SetAtlasSpriteSize_Native(Parent.ID, ref value); }
        }

        public Vector2 AtlasSpriteSizeCoef
        {
            get { GetAtlasSpriteSizeCoef_Native(Parent.ID, out Vector2 result); return result; }
            set { SetAtlasSpriteSizeCoef_Native(Parent.ID, ref value); }
        }

        public bool bAtlas
        {
            get { return GetIsAtlas_Native(Parent.ID); }
            set { SetIsAtlas_Native(Parent.ID, value); }
        }

        public bool bCastsShadows
        {
            get { return DoesCastShadows_Native(Parent.ID); }
            set { SetCastsShadows_Native(Parent.ID, value); }
        }

        public bool bReceivesDecals
        {
            get { return DoesReceiveDecals_Native(Parent.ID); }
            set { SetReceivesDecals_Native(Parent.ID, value); }
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetMaterial_Native(in GUID entityID, out GUID assetID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetMaterial_Native(in GUID entityID, in GUID assetID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetAtlasSpriteCoords_Native(in GUID entityID, out Vector2 outValue);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetAtlasSpriteCoords_Native(in GUID entityID, ref Vector2 value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetAtlasSpriteSize_Native(in GUID entityID, out Vector2 outValue);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetAtlasSpriteSize_Native(in GUID entityID, ref Vector2 value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetAtlasSpriteSizeCoef_Native(in GUID entityID, out Vector2 outValue);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetAtlasSpriteSizeCoef_Native(in GUID entityID, ref Vector2 value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool GetIsAtlas_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetIsAtlas_Native(in GUID entityID, bool value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetCastsShadows_Native(in GUID entityID, bool value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool DoesCastShadows_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetReceivesDecals_Native(in GUID entityID, bool value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool DoesReceiveDecals_Native(in GUID entityID);
    }

    public class BillboardComponent : SceneComponent
    {
        public BillboardComponent()
        {
            m_Type = typeof(BillboardComponent);
        }

        public AssetTexture2D TextureAsset
        {
            set
            {
                SetTexture_Native(Parent.ID, value != null ? value.GetGUID() : GUID.Null());
            }
            get
            {
                GUID assetGUID = GetTexture_Native(Parent.ID);
                if (assetGUID.IsNull())
                    return null;

                return new AssetTexture2D(assetGUID);
            }
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetTexture_Native(in GUID entityID, in GUID textureID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern GUID GetTexture_Native(in GUID entityID);
    }

    public class TextComponent : SceneComponent
    {
        public TextComponent()
        {
            m_Type = typeof(TextComponent);
        }

        public AssetFont Font
        {
            get
            {
                GUID assetID = GetFont_Native(Parent.ID);
                if (assetID.IsNull())
                    return null;

                return new AssetFont(assetID);
            }
            set
            {
                SetFont_Native(Parent.ID, value != null ? value.GetGUID() : GUID.Null());
            }
        }

        public AssetMaterial Material
        {
            get
            {
                GetMaterial_Native(Parent.ID, out GUID assetID);
                if (assetID.IsNull())
                    return null;

                return new AssetMaterial(assetID);
            }
            set
            {
                SetMaterial_Native(Parent.ID, (value != null) ? value.GetGUID() : GUID.Null());
            }
        }

        public string Text
        {
            get { return GetText_Native(Parent.ID); }
            set { SetText_Native(Parent.ID, value); }
        }

        public Color3 Color // Used only if bLit is false. It's an HDR value
        {
            get { GetColor_Native(Parent.ID, out Color3 result); return result; }
            set { SetColor_Native(Parent.ID, ref value); }
        }
        
        public float LineSpacing
        {
            get { return GetLineSpacing_Native(Parent.ID); }
            set { SetLineSpacing_Native(Parent.ID, value); }
        }
        
        public float Kerning
        {
            get { return GetKerning_Native(Parent.ID); }
            set { SetKerning_Native(Parent.ID, value); }
        }
        
        public float MaxWidth
        {
            get { return GetMaxWidth_Native(Parent.ID); }
            set { SetMaxWidth_Native(Parent.ID, value); }
        }

        public bool bCastsShadows
        {
            get { return DoesCastShadows_Native(Parent.ID); }
            set { SetCastsShadows_Native(Parent.ID, value); }
        }

        public bool bReceivesDecals
        {
            get { return DoesReceiveDecals_Native(Parent.ID); }
            set { SetReceivesDecals_Native(Parent.ID, value); }
        }

        public bool bLit
        {
            get { return GetIsLit_Native(Parent.ID); }
            set { SetIsLit_Native(Parent.ID, value); }
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern GUID GetFont_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetFont_Native(in GUID entityID, in GUID assetID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern string GetText_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetText_Native(in GUID entityID, string value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetColor_Native(in GUID entityID, out Color3 outValue);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetColor_Native(in GUID entityID, ref Color3 value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetLineSpacing_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetLineSpacing_Native(in GUID entityID, float value);
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetKerning_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetKerning_Native(in GUID entityID, float value);
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetMaxWidth_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetMaxWidth_Native(in GUID entityID, float value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetIsLit_Native(in GUID entityID, bool value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool GetIsLit_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetCastsShadows_Native(in GUID entityID, bool value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool DoesCastShadows_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetReceivesDecals_Native(in GUID entityID, bool value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool DoesReceiveDecals_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetMaterial_Native(in GUID entityID, out GUID assetID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetMaterial_Native(in GUID entityID, in GUID assetID);

    }
    
    public class Text2DComponent : Component
    {
        public AssetFont Font
        {
            get
            {
                GUID assetID = GetFont_Native(Parent.ID);
                if (assetID.IsNull())
                    return null;

                return new AssetFont(assetID);
            }
            set
            {
                SetFont_Native(Parent.ID, value != null ? value.GetGUID() : GUID.Null());
            }
        }
        
        public string Text
        {
            get { return GetText_Native(Parent.ID); }
            set { SetText_Native(Parent.ID, value); }
        }

        public Color3 Color
        {
            get { GetColor_Native(Parent.ID, out Color3 result); return result; }
            set { SetColor_Native(Parent.ID, ref value); }
        }
        public float LineSpacing
        {
            get { return GetLineSpacing_Native(Parent.ID); }
            set { SetLineSpacing_Native(Parent.ID, value); }
        }

        public Vector2 Position // Normalized device coords
        {
            get { GetPosition_Native(Parent.ID, out Vector2 result); return result; }
            set { SetPosition_Native(Parent.ID, ref value); }
        }

        public Vector2 Scale
        {
            get { GetScale_Native(Parent.ID, out Vector2 result); return result; }
            set { SetScale_Native(Parent.ID, ref value); }
        }

        public float Rotation
        {
            get { return GetRotation_Native(Parent.ID); }
            set { SetRotation_Native(Parent.ID, value); }
        }

        public float Kerning
        {
            get { return GetKerning_Native(Parent.ID); }
            set { SetKerning_Native(Parent.ID, value); }
        }
        public float MaxWidth
        {
            get { return GetMaxWidth_Native(Parent.ID); }
            set { SetMaxWidth_Native(Parent.ID, value); }
        }

        public float Opacity
        {
            get { return GetOpacity_Native(Parent.ID); }
            set { SetOpacity_Native(Parent.ID, value); }
        }

        public bool IsVisible
        {
            get { return IsVisible_Native(Parent.ID); }
            set { SetIsVisible_Native(Parent.ID, value); }
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern GUID GetFont_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetFont_Native(in GUID entityID, in GUID assetID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern string GetText_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetText_Native(in GUID entityID, string value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetColor_Native(in GUID entityID, out Color3 outValue);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetColor_Native(in GUID entityID, ref Color3 value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetPosition_Native(in GUID entityID, out Vector2 outValue);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetPosition_Native(in GUID entityID, ref Vector2 value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetScale_Native(in GUID entityID, out Vector2 outValue);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetScale_Native(in GUID entityID, ref Vector2 value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetRotation_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetRotation_Native(in GUID entityID, float value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetLineSpacing_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetLineSpacing_Native(in GUID entityID, float value);
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetKerning_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetKerning_Native(in GUID entityID, float value);
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetMaxWidth_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetMaxWidth_Native(in GUID entityID, float value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetOpacity_Native(in GUID entityID, float value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetOpacity_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetIsVisible_Native(in GUID entityID, bool value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool IsVisible_Native(in GUID entityID);
    }
    
    public class Image2DComponent : Component
    {
        public AssetTexture2D Texture
        {
            set
            {
                SetTexture_Native(Parent.ID, value != null ? value.GetGUID() : GUID.Null());
            }
            get
            {
                GUID assetGUID = GetTexture_Native(Parent.ID);
                if (assetGUID.IsNull())
                    return null;

                return new AssetTexture2D(assetGUID);
            }
        }

        public Color3 Tint
        {
            get { GetTint_Native(Parent.ID, out Color3 result); return result; }
            set { SetTint_Native(Parent.ID, ref value); }
        }

        public Vector2 Position // Normalized device coords
        {
            get { GetPosition_Native(Parent.ID, out Vector2 result); return result; }
            set { SetPosition_Native(Parent.ID, ref value); }
        }

        public Vector2 Scale
        {
            get { GetScale_Native(Parent.ID, out Vector2 result); return result; }
            set { SetScale_Native(Parent.ID, ref value); }
        }

        public float Rotation
        {
            get { return GetRotation_Native(Parent.ID); }
            set { SetRotation_Native(Parent.ID, value); }
        }

        public float Opacity
        {
            get { return GetOpacity_Native(Parent.ID); }
            set { SetOpacity_Native(Parent.ID, value); }
        }

        public bool IsVisible
        {
            get { return IsVisible_Native(Parent.ID); }
            set { SetIsVisible_Native(Parent.ID, value); }
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetTexture_Native(in GUID entityID, in GUID textureID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern GUID GetTexture_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetTint_Native(in GUID entityID, out Color3 outValue);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetTint_Native(in GUID entityID, ref Color3 value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetPosition_Native(in GUID entityID, out Vector2 outValue);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetPosition_Native(in GUID entityID, ref Vector2 value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetScale_Native(in GUID entityID, out Vector2 outValue);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetScale_Native(in GUID entityID, ref Vector2 value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetRotation_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetRotation_Native(in GUID entityID, float value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetOpacity_Native(in GUID entityID, float value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetOpacity_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetIsVisible_Native(in GUID entityID, bool value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool IsVisible_Native(in GUID entityID);
    }

    public class AudioComponent : SceneComponent
    {
        public AudioComponent()
        {
            m_Type = typeof(AudioComponent);
        }

        public void SetMinDistance(float minDistance) { SetMinDistance_Native(Parent.ID, minDistance); }
        
        public void SetMaxDistance(float maxDistance) { SetMaxDistance_Native(Parent.ID, maxDistance); }
        
        public void SetMinMaxDistance(float minDistance, float maxDistance)
        {
            SetMinMaxDistance_Native(Parent.ID, minDistance, maxDistance);
        }
        
        public float GetMinDistance() { return GetMinDistance_Native(Parent.ID); }
        
        public float GetMaxDistance() { return GetMaxDistance_Native(Parent.ID); }
        
        public void SetRollOffModel(RollOffModel rollOff)
        {
            SetRollOffModel_Native(Parent.ID, rollOff);
        }
        
        public RollOffModel GetRollOffModel() { return GetRollOffModel_Native(Parent.ID); }
        
        public void SetVolume(float volume)
        {
            SetVolume_Native(Parent.ID, volume);
        }
        
        public float GetVolume() { return GetVolume_Native(Parent.ID); }
        
        public void SetPitch(float pitch)
        {
            SetPitch_Native(Parent.ID, pitch);
        }
        
        public float GetPitch() { return GetPitch_Native(Parent.ID); }
        
        public void SetPan(float pan)
        {
            SetPan_Native(Parent.ID, pan);
        }
        
        public float GetPan() { return GetPan_Native(Parent.ID); }
        
        public void SetLoopCount(int loopCount)
        {
            SetLoopCount_Native(Parent.ID, loopCount);
        }
        
        public int GetLoopCount() { return GetLoopCount_Native(Parent.ID); }
        
        public void SetLooping(bool bLooping)
        {
            SetLooping_Native(Parent.ID, bLooping);
        }
        
        public bool IsLooping() { return IsLooping_Native(Parent.ID); }
        
        public void SetMuted(bool bMuted)
        {
            SetMuted_Native(Parent.ID, bMuted);
        }
        
        public bool IsMuted() { return IsMuted_Native(Parent.ID); }
        
        public void SetAudioAsset(AssetAudio asset)
        {
            SetAudioAsset_Native(Parent.ID, asset != null ? asset.GetGUID() : GUID.Null());
        }

        public AssetAudio GetAudioAsset()
        {
            GUID assetID = GetAudioAsset_Native(Parent.ID);
            if (assetID.IsNull())
                return null;
            return new AssetAudio(assetID);
        }
        
        public void SetStreaming(bool bStreaming)
        {
            SetStreaming_Native(Parent.ID, bStreaming);
        }
        
        public bool IsStreaming() { return IsStreaming_Native(Parent.ID); }
        
        public void Play()
        {
            Play_Native(Parent.ID);
        }
        
        public void Stop()
        {
            Stop_Native(Parent.ID);
        }
        
        public void SetPaused(bool bPaused)
        {
            SetPaused_Native(Parent.ID, bPaused);
        }
        
        public bool IsPlaying()
        {
            return IsPlaying_Native(Parent.ID);
        }

        public void SetDopplerEffectEnabled(bool bEnable)
        {
            SetDopplerEffectEnabled_Native(Parent.ID, bEnable);
        }

        public bool IsDopplerEffectEnabled()
        {
            return IsDopplerEffectEnabled_Native(Parent.ID);
        }

        public void SetFFTEnabled(bool bEnable)
        {
            SetFFTEnabled_Native(Parent.ID, bEnable);
        }

        public bool IsFFTEnabled()
        {
            return IsFFTEnabled_Native(Parent.ID);
        }

        public void SetFFTSamples(uint samples)
        {
            SetFFTSamples_Native(Parent.ID, samples);
        }

        public uint GetFFTSamples()
        {
            return GetFFTSamples_Native(Parent.ID);
        }

        public void SetFFTType(FFTWindowType type)
        {
            SetFFTType_Native(Parent.ID, type);
        }

        public FFTWindowType GetFFTType()
        {
            return GetFFTType_Native(Parent.ID);
        }

		// channelIndex. Allows to get data from a specific audio channel. Starts from 0. If -1, get average over all channels
        public bool GetSpectrumData(ref float[] data, int channelIndex = -1)
        {
            return GetSpectrumData_Native(Parent.ID, data, channelIndex);
        }

        public float GetSampleRate()
        {
            return GetSampleRate_Native(Parent.ID);
        }

        public int GetChannelsCount()
        {
            return GetChannelsCount_Native(Parent.ID);
        }

        public void SetPosition(uint ms)
        {
            SetPosition_Native(Parent.ID, ms);
        }

        public uint GetPosition()
        {
            return GetPosition_Native(Parent.ID);
        }

        public void SetIs3D(bool b3D)
        {
            SetIs3D_Native(Parent.ID, b3D);
        }

        public bool Is3D()
        {
            return Is3D_Native(Parent.ID);
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetMinDistance_Native(in GUID entityID, float minDistance);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetMaxDistance_Native(in GUID entityID, float maxDistance);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetMinMaxDistance_Native(in GUID entityID, float minDistance, float maxDistance);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetRollOffModel_Native(in GUID entityID, RollOffModel rollOff);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetVolume_Native(in GUID entityID, float volume);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetPitch_Native(in GUID entityID, float pitch);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetPan_Native(in GUID entityID, float pan);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetLoopCount_Native(in GUID entityID, int loopCount);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetLooping_Native(in GUID entityID, bool bLooping);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetMuted_Native(in GUID entityID, bool bMuted);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetAudioAsset_Native(in GUID entityID, in GUID assetID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetStreaming_Native(in GUID entityID, bool bStreaming);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Play_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Stop_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetPaused_Native(in GUID entityID, bool bPaused);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetDopplerEffectEnabled_Native(in GUID entityID, bool bEnable);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetMinDistance_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetMaxDistance_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern RollOffModel GetRollOffModel_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetVolume_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetPitch_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetPan_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int GetLoopCount_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool IsLooping_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool IsMuted_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern GUID GetAudioAsset_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool IsStreaming_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool IsPlaying_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool IsDopplerEffectEnabled_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetFFTEnabled_Native(GUID id, bool value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool IsFFTEnabled_Native(GUID id);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetFFTSamples_Native(GUID id, uint value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern uint GetFFTSamples_Native(GUID id);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetFFTType_Native(GUID id, FFTWindowType value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern FFTWindowType GetFFTType_Native(GUID id);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool GetSpectrumData_Native(in GUID entityID, float[] data, int channelIndex);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetSampleRate_Native(GUID id);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetPosition_Native(GUID id, uint ms);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern uint GetPosition_Native(GUID id);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetIs3D_Native(GUID id, bool value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool Is3D_Native(GUID id);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int GetChannelsCount_Native(GUID id);
    }

    public class ReverbComponent : SceneComponent
    {
        public ReverbComponent()
        {
            m_Type = typeof(ReverbComponent);
        }

        public bool bActive
        {
            get { return IsActive_Native(Parent.ID); }
            set { SetIsActive_Native(Parent.ID, value); }
        }
        public ReverbPreset Preset
        {
            get { return GetReverbPreset_Native(Parent.ID); }
            set { SetReverbPreset_Native(Parent.ID, value); }
        }
        public float MinDistance
        {
            get { return GetMinDistance_Native(Parent.ID); }
            set { SetMinDistance_Native(Parent.ID, value); }
        }
        public float MaxDistance
        {
            get { return GetMaxDistance_Native(Parent.ID); }
            set { SetMaxDistance_Native(Parent.ID, value); }
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool IsActive_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetIsActive_Native(in GUID entityID, bool value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern ReverbPreset GetReverbPreset_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetReverbPreset_Native(in GUID entityID, ReverbPreset value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetMinDistance_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetMinDistance_Native(in GUID entityID, float value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetMaxDistance_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetMaxDistance_Native(in GUID entityID, float value);
    }

    public class RigidBodyComponent : Component
    {
        public void SetBodyType(PhysicsBodyType bodyType) { SetBodyType_Native(Parent.ID, bodyType); }
        
        public PhysicsBodyType GetBodyType() { return GetBodyType_Native(Parent.ID); }

        public void SetCollisionDetectionType(CollisionDetectionType type) { SetCollisionDetectionType_Native(Parent.ID, type); }

        public CollisionDetectionType GetCollisionDetectionType() { return GetCollisionDetectionType_Native(Parent.ID); }

        /*
            The solver iteration count determines how accurately joints and contacts are resolved.
            If you are having trouble with jointed bodies oscillating and behaving erratically, then
            setting a higher position iteration count may improve their stability. Range: [1, 255]

            If intersecting bodies are being depenetrated too violently, increase the number of velocity
            iterations.More velocity iterations will drive the relative exit velocity of the intersecting
            objects closer to the correct value given the restitution. Range: [0, 255]
        */
        public void SetPositionSolverIterations(uint iterations) { SetPositionSolverIterations_Native(Parent.ID, iterations); }
        public void SetVelocitySolverIterations(uint iterations) { SetVelocitySolverIterations_Native(Parent.ID, iterations); }
        public uint GetPositionSolverIterations() { return GetPositionSolverIterations_Native(Parent.ID); }
        public uint GetVelocitySolverIterations() { return GetVelocitySolverIterations_Native(Parent.ID); }

        public void SetMass(float mass) { SetMass_Native(Parent.ID, mass); }
        
        public float GetMass() { return GetMass_Native(Parent.ID); }

        public void SetLinearDamping(float linearDamping) { SetLinearDamping_Native(Parent.ID, linearDamping); }
        
        public float GetLinearDamping() { return GetLinearDamping_Native(Parent.ID); }

        public void SetAngularDamping(float angularDamping) { SetAngularDamping_Native(Parent.ID, angularDamping); }
        
        public float GetAngularDamping() { return GetAngularDamping_Native(Parent.ID); }

        public void SetEnableGravity(bool bEnable) { SetEnableGravity_Native(Parent.ID, bEnable); }
        
        public bool IsGravityEnabled() { return IsGravityEnabled_Native(Parent.ID); }

        public void SetIsKinematic(bool bKinematic) { SetIsKinematic_Native(Parent.ID, bKinematic); }
        
        public bool IsKinematic() { return IsKinematic_Native(Parent.ID); }

        public void WakeUp()
        {
            WakeUp_Native(Parent.ID);
        }

        public void PutToSleep()
        {
            PutToSleep_Native(Parent.ID);
        }

        public void AddForce(in Vector3 force, ForceMode forceMode)
        {
            AddForce_Native(Parent.ID, in force, forceMode);
        }

        public void AddTorque(in Vector3 torque, ForceMode forceMode)
        {
            AddTorque_Native(Parent.ID, in torque, forceMode);
        }

        public Vector3 GetLinearVelocity()
        {
            GetLinearVelocity_Native(Parent.ID, out Vector3 result);
            return result;
        }

        public void SetLinearVelocity(in Vector3 velocity)
        {
            SetLinearVelocity_Native(Parent.ID, in velocity);
        }

        public Vector3 GetAngularVelocity()
        {
            GetAngularVelocity_Native(Parent.ID, out Vector3 result);
            return result;
        }

        public void SetAngularVelocity(in Vector3 velocity)
        {
            SetAngularVelocity_Native(Parent.ID, in velocity);
        }

        public float GetMaxLinearVelocity()
        {
            return GetMaxLinearVelocity_Native(Parent.ID);
        }

        public void SetMaxLinearVelocity(float maxVelocity)
        {
            SetMaxLinearVelocity_Native(Parent.ID, maxVelocity);
        }

        public float GetMaxAngularVelocity()
        {
            return GetMaxAngularVelocity_Native(Parent.ID);
        }

        public void SetMaxAngularVelocity(float maxVelocity)
        {
            SetMaxAngularVelocity_Native(Parent.ID, maxVelocity);
        }

        public bool IsDynamic()
        {
            return IsDynamic_Native(Parent.ID);
        }

        public Transform GetKinematicTarget()
        {
            GetKinematicTarget_Native(Parent.ID, out Transform result);
            return result;
        }

        public Vector3 GetKinematicTargetLocation()
        {
            GetKinematicTargetLocation_Native(Parent.ID, out Vector3 result);
            return result;
        }

        public Rotator GetKinematicTargetRotation()
        {
            GetKinematicTargetRotation_Native(Parent.ID, out Rotator result);
            return result;
        }

        public void SetKinematicTarget(Vector3 location, Rotator rotation)
        {
            SetKinematicTarget_Native(Parent.ID, ref location, ref rotation);
        }

        public void SetKinematicTargetLocation(Vector3 location)
        {
            SetKinematicTargetLocation_Native(Parent.ID, ref location);
        }

        public void SetKinematicTargetRotation(Rotator rotation)
        {
            SetKinematicTargetRotation_Native(Parent.ID, ref rotation);
        }

        public bool IsLockFlagSet(ActorLockFlag flag)
        {
            return IsLockFlagSet_Native(Parent.ID, flag);
        }

        public void SetLockFlag(ActorLockFlag flag, bool value)
        {
            SetLockFlag_Native(Parent.ID, flag, value);
        }

        public ActorLockFlag GetLockFlags()
        {
            return GetLockFlags_Native(Parent.ID);
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetBodyType_Native(in GUID entityID, PhysicsBodyType type);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern PhysicsBodyType GetBodyType_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetCollisionDetectionType_Native(in GUID entityID, CollisionDetectionType type);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern CollisionDetectionType GetCollisionDetectionType_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetPositionSolverIterations_Native(in GUID entityID, uint value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetVelocitySolverIterations_Native(in GUID entityID, uint value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern uint GetPositionSolverIterations_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern uint GetVelocitySolverIterations_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetMass_Native(in GUID entityID, float mass);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetMass_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetLinearDamping_Native(in GUID entityID, float linearDamping);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetLinearDamping_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetAngularDamping_Native(in GUID entityID, float angularDamping);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetAngularDamping_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetEnableGravity_Native(in GUID entityID, bool bEnable);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool IsGravityEnabled_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetIsKinematic_Native(in GUID entityID, bool bKinematic);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool IsKinematic_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void WakeUp_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void PutToSleep_Native(in GUID entityID);

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
        internal static extern bool IsDynamic_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetKinematicTarget_Native(in GUID entityID, out Transform transform);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetKinematicTargetLocation_Native(in GUID entityID, out Vector3 location);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetKinematicTargetRotation_Native(in GUID entityID, out Rotator rotation);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetKinematicTarget_Native(in GUID entityID, ref Vector3 location, ref Rotator rotation);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetKinematicTargetLocation_Native(in GUID entityID, ref Vector3 location);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetKinematicTargetRotation_Native(in GUID entityID, ref Rotator rotation);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool IsLockFlagSet_Native(in GUID entityID, ActorLockFlag flag);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern ActorLockFlag GetLockFlags_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetLockFlag_Native(in GUID entityID, ActorLockFlag flag, bool value);
    }

    abstract public class BaseColliderComponent : SceneComponent
    {
        public BaseColliderComponent()
        {
            m_Type = typeof(BaseColliderComponent);
        }

        // Collision groups it belongs to. It can belong to different groups (use XOR to combine groups)
        public void SetCollisionGroup(CollisionGroup groups) { SetCollisionGroup_Native(Parent.ID, m_Type, groups); }
		public CollisionGroup GetCollisionGroup() { return GetCollisionGroup_Native(Parent.ID, m_Type); }

		// Collision groups it can interact with
		public void SetInteractingCollisionGroup(CollisionGroup groups) { SetInteractingCollisionGroup_Native(Parent.ID, m_Type, groups); }
		public CollisionGroup GetInteractingCollisionGroup() { return GetInteractingCollisionGroup_Native(Parent.ID, m_Type); }

        public void SetIsTrigger(bool bTrigger) { SetIsTrigger_Native(Parent.ID, m_Type, bTrigger); }
        
        public bool IsTrigger() { return IsTrigger_Native(Parent.ID, m_Type); }
        
        public AssetPhysicsMaterial GetPhysicsMaterial()
        {
            GUID assetID = GetPhysicsMaterial_Native(Parent.ID, m_Type);
            if (assetID.IsNull())
                return null;
            return new AssetPhysicsMaterial(assetID);
        }

        public void SetPhysicsMaterial(AssetPhysicsMaterial material)
        {
            SetPhysicsMaterial_Native(Parent.ID, m_Type, material != null ? material.GetGUID() : GUID.Null());
        }

        public void SetCollisionVisible(bool bVisible) { SetCollisionVisible_Native(Parent.ID, m_Type, bVisible); }

        public bool IsCollisionVisible(bool bVisible) { return IsCollisionVisible_Native(Parent.ID, m_Type); }

        public void SetAffectsNavMeshBuild(bool bAffects)
        {
            SetAffectsNavMeshBuild_Native(Parent.ID, m_Type, bAffects);
        }

        public bool DoesAffectNavMeshBuild()
        {
            return DoesAffectNavMeshBuild_Native(Parent.ID, m_Type);
        }

        public void SetIsObstacle(bool bObstacle)
        {
            SetIsObstacle_Native(Parent.ID, m_Type, bObstacle);
        }

        public bool IsObstacle()
        {
            return IsObstacle_Native(Parent.ID, m_Type);
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetCollisionGroup_Native(in GUID entityID, Type type, CollisionGroup groups);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern CollisionGroup GetCollisionGroup_Native(in GUID entityID, Type type);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetInteractingCollisionGroup_Native(in GUID entityID, Type type, CollisionGroup groups);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern CollisionGroup GetInteractingCollisionGroup_Native(in GUID entityID, Type type);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetIsTrigger_Native(in GUID entityID, Type type, bool bTrigger);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool IsTrigger_Native(in GUID entityID, Type type);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetCollisionVisible_Native(in GUID entityID, Type type, bool bShow);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool IsCollisionVisible_Native(in GUID entityID, Type type);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetAffectsNavMeshBuild_Native(in GUID entityID, Type type, bool bAffects);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool DoesAffectNavMeshBuild_Native(in GUID entityID, Type type);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetIsObstacle_Native(in GUID entityID, Type type, bool bObstacle);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool IsObstacle_Native(in GUID entityID, Type type);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern GUID GetPhysicsMaterial_Native(in GUID entityID, Type type);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetPhysicsMaterial_Native(in GUID entityID, Type type, in GUID assetID);
    }

    public class BoxColliderComponent : BaseColliderComponent
    {
        public BoxColliderComponent()
        {
            m_Type = typeof(BoxColliderComponent);
        }

        public void SetSize(Vector3 size) { SetSize_Native(Parent.ID, ref size); }

		public Vector3 GetSize()
        {
            GetSize_Native(Parent.ID, out Vector3 result);
            return result;
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetSize_Native(in GUID entityID, ref Vector3 val);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetSize_Native(in GUID entityID, out Vector3 val);
    }

    public class SphereColliderComponent : BaseColliderComponent
    {
        public SphereColliderComponent()
        {
            m_Type = typeof(SphereColliderComponent);
        }

        public void SetRadius(float radius) { SetRadius_Native(Parent.ID, radius); }

        public float GetRadius()
        {
            return GetRadius_Native(Parent.ID);
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetRadius_Native(in GUID entityID, float val);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetRadius_Native(in GUID entityID);
    }

    public class CapsuleColliderComponent : BaseColliderComponent
    {
        public CapsuleColliderComponent()
        {
            m_Type = typeof(CapsuleColliderComponent);
        }

        public void SetRadius(float radius) { SetRadius_Native(Parent.ID, radius); }
        public float GetRadius() { return GetRadius_Native(Parent.ID); }
        public void SetHeight(float height) { SetHeight_Native(Parent.ID, height); }
        public float GetHeight() { return GetHeight_Native(Parent.ID); }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetRadius_Native(in GUID entityID, float val);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetRadius_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetHeight_Native(in GUID entityID, float val);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetHeight_Native(in GUID entityID);

    }

    public class MeshColliderComponent : BaseColliderComponent
    {
        public MeshColliderComponent()
        {
            m_Type = typeof(MeshColliderComponent);
        }

        public void SetIsConvex(bool bConvex) { SetIsConvex_Native(Parent.ID, bConvex); }
        
        public bool IsConvex() { return IsConvex_Native(Parent.ID); }

        // Only affects non-convex mesh colliders. Non-convex meshes are one-sided meaning collision won't be registered from the back side. For example, that might be a problem for windows.
        // So to fix this problem, you can set this flag to true
        public void SetIsTwoSided(bool bConvex) { SetIsTwoSided_Native(Parent.ID, bConvex); }
        
        public bool IsTwoSided() { return IsTwoSided_Native(Parent.ID); }

        public void SetCollisionMeshAsset(AssetStaticMesh mesh)
        {
            SetCollisionMesh_Native(Parent.ID, mesh != null ? mesh.GetGUID() : GUID.Null());
        }
        
        public AssetStaticMesh GetCollisionMeshAsset()
        {
            GUID assetID = GetCollisionMesh_Native(Parent.ID);
            if (assetID.IsNull())
                return null;
            return new AssetStaticMesh(assetID);
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetIsConvex_Native(in GUID entityID, bool val);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool IsConvex_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetIsTwoSided_Native(in GUID entityID, bool val);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool IsTwoSided_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetCollisionMesh_Native(in GUID entityID, GUID meshGUID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern GUID GetCollisionMesh_Native(in GUID entityID);

    }

    public class ScriptComponent : Component
    {
        // Removes old script and calls `OnDestroy` if present.
        // Creates a new script and calls `OnCreate`.
        // @type. Must be derived from `Eagle.Entity` class. It can be null to remove a script
        public void SetScript(Type type)
        {
            SetScript_Native(Parent.ID, type);
        }

        public Type GetScriptType()
        {
            return GetScriptType_Native(Parent.ID);
        }

        // Call this method to correctly retrieve script instance.
        // For example, if you have a script `public class MyScript : Entity`, you can call this to get the correct Entity ref.
        // You need to call `ScriptComponent.GetInstance()` to get script instance that you can cast to `MyScript`.
        // Probably, you'll never need this function because all `Entity` object should already by correct instances.
        // I think the only scenario you'll need this if you do:
        //      Entity entity = Scene.CreateEntity("MyEntity");
        //      entity.AddComponent<ScriptComponent>().SetScript(typeof(MyScript));
        // because initial `entity` instance was create without knowing anything about `MyScript`,
        // which means now you'll need to update the ref:
        //      entity = entity.GetComponent<ScriptComponent>().GetInstance() as MyScript;
        public Entity GetInstance()
        {
            return GetInstance_Native(Parent.ID);
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetScript_Native(in GUID entityID, Type type);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern Type GetScriptType_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern Entity GetInstance_Native(in GUID entityID);
    }

    public class ParticleSystemComponent : SceneComponent
    {
        public ParticleSystemComponent()
        {
            m_Type = typeof(ParticleSystemComponent);
        }

        public void SetAsset(AssetParticleSystem ps)
        {
            SetAsset_Native(Parent.ID, ps != null ? ps.GetGUID() : GUID.Null());
        }

        public AssetParticleSystem GetAsset()
        {
            GUID assetID = GetAsset_Native(Parent.ID);
            if (assetID.IsNull())
                return null;
            return new AssetParticleSystem(assetID);
        }

        // Needs to be called every frame.
        // Forces emitter to copy animation pose of the skeletal component.
        // Can be used to replicate animation
        // @emitterIndex. Emitter index inside `GetAsset().GetEmitters()`
        // @comp. Component to replicate anim from
        public void DuplicatePose(uint emitterIndex, Entity src)
        {
            DuplicatePose_Native(Parent.ID, emitterIndex, src.ID);
        }

        public void Spawn() { Spawn_Native(Parent.ID); }
        public void Destroy() { Destroy_Native(Parent.ID); }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetAsset_Native(GUID entityID, GUID assetGUID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern GUID GetAsset_Native(GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Spawn_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Destroy_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void DuplicatePose_Native(GUID entityID, uint emitterIndex, GUID compEntityID);
    }

    public class DecalComponent : SceneComponent
    {
        public DecalComponent()
        {
            m_Type = typeof(DecalComponent);
        }

        public AssetMaterial MaterialAsset
        {
            set
            {
                SetMaterial_Native(Parent.ID, value != null ? value.GetGUID() : GUID.Null());
            }
            get
            {
                GUID assetGUID = GetMaterial_Native(Parent.ID);
                if (assetGUID.IsNull())
                    return null;

                return new AssetMaterial(assetGUID);
            }
        }

        public uint SortPriority
        {
            set
            {
                SetSortPriority_Native(Parent.ID, value);
            }
            get
            {
                return GetSortPriority_Native(Parent.ID);
            }
        }

        public bool bAdjustAspectRatio
        {
            set
            {
                SetAdjustAspectRatioEnabled_Native(Parent.ID, value);
            }
            get
            {
                return IsAdjustAspectRatioEnabled_Native(Parent.ID);
            }
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetMaterial_Native(in GUID entityID, in GUID assetID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern GUID GetMaterial_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetAdjustAspectRatioEnabled_Native(in GUID entityID, bool value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool IsAdjustAspectRatioEnabled_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetSortPriority_Native(in GUID entityID, uint value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern uint GetSortPriority_Native(in GUID entityID);
    }

    public class NavigationMeshComponent : SceneComponent
    {
        public NavigationMeshComponent()
        {
            m_Type = typeof(NavigationMeshComponent);
        }

        public bool bAutoRebuild // Rebuilds on changes if activated
        {
            get { return GetAutoRebuild_Native(Parent.ID); }
            set { SetAutoRebuild_Native(Parent.ID, value); }
        }

        public void SetSettings(NavMeshSettings settings)
        {
            SetSettings_Native(Parent.ID, ref settings);
        }

        public NavMeshSettings GetSettings()
        {
            GetSettings_Native(Parent.ID, out NavMeshSettings settings);
            return settings;
        }

        public void SetCrowdSettings(CrowdSettings settings)
        {
            SetCrowdSettings_Native(Parent.ID, ref settings);
        }

        public CrowdSettings GetCrowdSettings()
        {
            GetCrowdSettings_Native(Parent.ID, out CrowdSettings settings);
            return settings;
        }

        // Currently, only one nav mesh is supported.
        // Building it will invalidate existing nav mesh.
        public void Build()
        {
            Build_Native(Parent.ID);
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Build_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetCrowdSettings_Native(in GUID entityID, ref CrowdSettings settings);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetCrowdSettings_Native(in GUID entityID, out CrowdSettings settings);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetSettings_Native(in GUID entityID, ref NavMeshSettings settings);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetSettings_Native(in GUID entityID, out NavMeshSettings settings);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetAutoRebuild_Native(in GUID entityID, bool value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool GetAutoRebuild_Native(in GUID entityID);
    }

    public class NavigationCrowdAgentComponent : Component
    {
        // Agents are controlled by the crowd system. But if teleportation is required,
        // this function can be used. It'll recreate an agent at a new location
        public void TeleportAgent(Vector3 location)
        {
            TeleportAgent_Native(Parent.ID, ref location);
        }

        public void SetMoveTarget(Vector3 location)
        {
            SetMoveTarget_Native(Parent.ID, ref location);
        }

        public void ResetMoveTarget()
        {
            ResetMoveTarget_Native(Parent.ID);
        }

        public bool IsValid()
        {
            return IsValid_Native(Parent.ID);
        }

        // Entity.WorldLocation should be the same because agents control entities.
        public bool GetLocation(out Vector3 outLocation)
        {
            return GetLocation_Native(Parent.ID, out outLocation);
        }

        public bool GetVelocity(out Vector3 outVelocity)
        {
            return GetVelocity_Native(Parent.ID, out outVelocity);
        }

        public void SetSettings(AgentSettings settings)
        {
            SetSettings_Native(Parent.ID, ref settings);
        }

        public AgentSettings GetSettings()
        {
            GetSettings_Native(Parent.ID, out AgentSettings settings);
            return settings;
        }

        public AgentTargetState GetTargetState()
        {
            return GetTargetState_Native(Parent.ID);
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void TeleportAgent_Native(in GUID entityID, ref Vector3 location);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetMoveTarget_Native(in GUID entityID, ref Vector3 location);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void ResetMoveTarget_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool IsValid_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetSettings_Native(in GUID entityID, ref AgentSettings settings);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetSettings_Native(in GUID entityID, out AgentSettings settings);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool GetLocation_Native(in GUID entityID, out Vector3 location);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool GetVelocity_Native(in GUID entityID, out Vector3 velocity);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern AgentTargetState GetTargetState_Native(in GUID entityID);
    }
}
