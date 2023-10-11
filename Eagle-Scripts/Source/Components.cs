using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

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

    public class TransformComponent : Component
    {
        public Transform WorldTransform
        {
            get
            {
                GetWorldTransform_Native(Parent.ID, out Transform result);
                return result;
            }
            set
            {
                SetWorldTransform_Native(Parent.ID, ref value);
            }
        }

        public Vector3 WorldLocation
        {
            get
            {
                GetWorldLocation_Native(Parent.ID, out Vector3 result);
                return result;
            }
            set
            {
                SetWorldLocation_Native(Parent.ID, ref value);
            }
        }

        public Rotator WorldRotation
        {
            get
            {
                GetWorldRotation_Native(Parent.ID, out Rotator result);
                return result;
            }
            set
            {
                SetWorldRotation_Native(Parent.ID, ref value);
            }
        }

        public Vector3 WorldScale
        {
            get
            {
                GetWorldScale_Native(Parent.ID, out Vector3 result);
                return result;
            }
            set
            {
                SetWorldScale_Native(Parent.ID, ref value);
            }
        }

        public Transform RelativeTransform
        {
            get
            {
                GetRelativeTransform_Native(Parent.ID, out Transform result);
                return result;
            }
            set
            {
                SetRelativeTransform_Native(Parent.ID, ref value);
            }
        }

        public Vector3 RelativeLocation
        {
            get
            {
                GetRelativeLocation_Native(Parent.ID, out Vector3 result);
                return result;
            }
            set
            {
                SetRelativeLocation_Native(Parent.ID, ref value);
            }
        }

        public Rotator RelativeRotation
        {
            get
            {
                GetRelativeRotation_Native(Parent.ID, out Rotator result);
                return result;
            }
            set
            {
                SetRelativeRotation_Native(Parent.ID, ref value);
            }
        }

        public Vector3 RelativeScale
        {
            get
            {
                GetRelativeScale_Native(Parent.ID, out Vector3 result);
                return result;
            }
            set
            {
                SetRelativeScale_Native(Parent.ID, ref value);
            }
        }

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

        public StaticMesh Mesh
        {
            get
            {
                StaticMesh temp = new StaticMesh();
                temp.ID = GetMesh_Native(Parent.ID);
                return temp;
            }

            set
            {
                SetMesh_Native(Parent.ID, value.ID);
            }
        }

        public bool bCastsShadows
        {
            get { return DoesCastShadows_Native(Parent.ID); }
            set { SetCastsShadows_Native(Parent.ID, value); }
        }

        public Material GetMaterial()
        {
            Material result = new Material();
            GetMaterial_Native(Parent.ID, out GUID albedo, out GUID metallness, out GUID normal, out GUID roughness, out GUID ao, out GUID emissiveTexture, out GUID opacityTexture, out GUID opacityMaskTexture,
                out Color4 tint, out Vector3 emissiveIntensity, out float tilingFactor, out MaterialBlendMode blendMode);
            result.AlbedoTexture = new Texture2D(albedo);
            result.MetallnessTexture = new Texture2D(metallness);
            result.NormalTexture = new Texture2D(normal);
            result.RoughnessTexture = new Texture2D(roughness);
            result.AOTexture = new Texture2D(ao);
            result.EmissiveTexture = new Texture2D(emissiveTexture);
            result.OpacityTexture = new Texture2D(opacityTexture);
            result.OpacityMaskTexture = new Texture2D(opacityMaskTexture);
            result.TintColor = tint;
            result.EmissiveIntensity = emissiveIntensity;
            result.TilingFactor = tilingFactor;
            result.BlendMode = blendMode;

            return result;
        }

        public void SetMaterial(Material value)
        {
            GUID albedoID = value.AlbedoTexture != null ? value.AlbedoTexture.ID : new GUID();
            GUID metallnessID = value.MetallnessTexture != null ? value.MetallnessTexture.ID : new GUID();
            GUID normalID = value.NormalTexture != null ? value.NormalTexture.ID : new GUID();
            GUID roughnessID = value.RoughnessTexture != null ? value.RoughnessTexture.ID : new GUID();
            GUID aoID = value.AOTexture != null ? value.AOTexture.ID : new GUID();
            GUID emissiveID = value.EmissiveTexture != null ? value.EmissiveTexture.ID : new GUID();
            GUID opacityID = value.OpacityTexture != null ? value.OpacityTexture.ID : new GUID();
            GUID opacityMaskID = value.OpacityMaskTexture != null ? value.OpacityMaskTexture.ID : new GUID();

            SetMaterial_Native(Parent.ID, albedoID, metallnessID, normalID, roughnessID, aoID, emissiveID, opacityID, opacityMaskID, ref value.TintColor, ref value.EmissiveIntensity, value.TilingFactor, value.BlendMode);
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetMesh_Native(in GUID entityID, GUID meshGUID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern GUID GetMesh_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetMaterial_Native(in GUID entityID, out GUID albedo, out GUID metallness, out GUID normal, out GUID roughness, out GUID ao, out GUID emissiveTexture, out GUID opacityTexture, out GUID opacityMaskTexture,
            out Color4 tint, out Vector3 emissiveIntensity, out float tilingFactor, out MaterialBlendMode blendMode);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetMaterial_Native(in GUID entityID, in GUID albedo, in GUID metallness, in GUID normal, in GUID roughness, in GUID ao, in GUID emissiveTexture, in GUID opacityTexture, in GUID opacityMaskTexture,
            ref Color4 tint, ref Vector3 emissiveIntensity, float tilingFactor, MaterialBlendMode blendMode);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetCastsShadows_Native(in GUID entityID, bool value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool DoesCastShadows_Native(in GUID entityID);
    }

    public class SpriteComponent : SceneComponent
    {
        public SpriteComponent()
        {
            m_Type = typeof(SpriteComponent);
        }

        public Material GetMaterial()
        {
            Material result = new Material();
            GetMaterial_Native(Parent.ID, out GUID albedo, out GUID metallness, out GUID normal, out GUID roughness, out GUID ao, out GUID emissiveTexture, out GUID opacityTexture, out GUID opacityMaskTexture,
                out Color4 tint, out Vector3 emissiveIntensity, out float tilingFactor, out MaterialBlendMode blendMode);
            result.AlbedoTexture = new Texture2D(albedo);
            result.MetallnessTexture = new Texture2D(metallness);
            result.NormalTexture = new Texture2D(normal);
            result.RoughnessTexture = new Texture2D(roughness);
            result.AOTexture = new Texture2D(ao);
            result.EmissiveTexture = new Texture2D(emissiveTexture);
            result.OpacityTexture = new Texture2D(opacityTexture);
            result.OpacityMaskTexture = new Texture2D(opacityMaskTexture);
            result.TintColor = tint;
            result.EmissiveIntensity = emissiveIntensity;
            result.TilingFactor = tilingFactor;
            result.BlendMode = blendMode;

            return result;
        }

        void SetMaterial(Material value)
        {
            GUID albedoID = value.AlbedoTexture != null ? value.AlbedoTexture.ID : new GUID();
            GUID metallnessID = value.MetallnessTexture != null ? value.MetallnessTexture.ID : new GUID();
            GUID normalID = value.NormalTexture != null ? value.NormalTexture.ID : new GUID();
            GUID roughnessID = value.RoughnessTexture != null ? value.RoughnessTexture.ID : new GUID();
            GUID aoID = value.AOTexture != null ? value.AOTexture.ID : new GUID();
            GUID emissiveID = value.EmissiveTexture != null ? value.EmissiveTexture.ID : new GUID();
            GUID opacityID = value.OpacityTexture != null ? value.OpacityTexture.ID : new GUID();
            GUID opacityMaskID = value.OpacityMaskTexture != null ? value.OpacityMaskTexture.ID : new GUID();

            SetMaterial_Native(Parent.ID, albedoID, metallnessID, normalID, roughnessID, aoID, emissiveID, opacityID, opacityMaskID, ref value.TintColor, ref value.EmissiveIntensity, value.TilingFactor, value.BlendMode);
        }
    
        public Texture2D Subtexture
        {
            set { SetSubtexture_Native(Parent.ID, value.ID); }
            get
            {
                Texture2D tex = new Texture2D(GetSubtexture_Native(Parent.ID));
                return tex;
            }
        }

        public Vector2 SubtextureCoords
        {
            get { GetSubtextureCoords_Native(Parent.ID, out Vector2 result); return result; }
            set { SetSubtextureCoords_Native(Parent.ID, ref value); }
        }

        public Vector2 SpriteSize
        {
            get { GetSpriteSize_Native(Parent.ID, out Vector2 result); return result; }
            set { SetSpriteSize_Native(Parent.ID, ref value); }
        }

        public Vector2 SpriteSizeCoef
        {
            get { GetSpriteSizeCoef_Native(Parent.ID, out Vector2 result); return result; }
            set { SetSpriteSizeCoef_Native(Parent.ID, ref value); }
        }

        public bool bSubtexture
        {
            get { return GetIsSubtexture_Native(Parent.ID); }
            set { SetIsSubtexture_Native(Parent.ID, value); }
        }

        public bool bCastsShadows
        {
            get { return DoesCastShadows_Native(Parent.ID); }
            set { SetCastsShadows_Native(Parent.ID, value); }
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetMaterial_Native(in GUID entityID, out GUID albedo, out GUID metallness, out GUID normal, out GUID roughness, out GUID ao, out GUID emissiveTexture, out GUID opacityTexture, out GUID opacityMaskTexture,
            out Color4 tint, out Vector3 emissiveIntensity, out float tilingFactor, out MaterialBlendMode blendMode);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetMaterial_Native(in GUID entityID, in GUID albedo, in GUID metallness, in GUID normal, in GUID roughness, in GUID ao, in GUID emissiveTexture, in GUID opacityTexture, in GUID opacityMaskTexture,
            ref Color4 tint, ref Vector3 emissiveIntensity, float tilingFactor, MaterialBlendMode blendMode);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetSubtexture_Native(in GUID entityID, in GUID subtexture);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern GUID GetSubtexture_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetSubtextureCoords_Native(in GUID entityID, out Vector2 outValue);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetSubtextureCoords_Native(in GUID entityID, ref Vector2 value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetSpriteSize_Native(in GUID entityID, out Vector2 outValue);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetSpriteSize_Native(in GUID entityID, ref Vector2 value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetSpriteSizeCoef_Native(in GUID entityID, out Vector2 outValue);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetSpriteSizeCoef_Native(in GUID entityID, ref Vector2 value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool GetIsSubtexture_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetIsSubtexture_Native(in GUID entityID, bool value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetCastsShadows_Native(in GUID entityID, bool value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool DoesCastShadows_Native(in GUID entityID);
    }

    public class BillboardComponent : SceneComponent
    {
        public BillboardComponent()
        {
            m_Type = typeof(BillboardComponent);
        }

        public Texture2D Texture
        {
            set { SetTexture_Native(Parent.ID, value.ID); }
            get
            {
                Texture2D tex = new Texture2D(GetTexture_Native(Parent.ID));
                return tex;
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

        public string Text
        {
            get { return GetText_Native(Parent.ID); }
            set { SetText_Native(Parent.ID, value); }
        }

        public MaterialBlendMode BlendMode
        {
            get { return GetBlendMode_Native(Parent.ID); }
            set { SetBlendMode_Native(Parent.ID, value); }
        }

        // Used only if bLit is false. It's an HDR value
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

        // Values below are only if bLit is true
        public bool bLit
        {
            get { return GetIsLit_Native(Parent.ID); }
            set { SetIsLit_Native(Parent.ID, value); }
        }

        public Color3 Albedo
        {
            get { GetAlbedo_Native(Parent.ID, out Color3 result); return result; }
            set { SetAlbedo_Native(Parent.ID, ref value); }
        }
        
        public Color3 Emissive
        {
            get { GetEmissive_Native(Parent.ID, out Color3 result); return result; }
            set { SetEmissive_Native(Parent.ID, ref value); }
        }

        public float Metallness
        {
            get { return GetMetallness_Native(Parent.ID); }
            set { SetMetallness_Native(Parent.ID, value); }
        }

        public float Roughness
        {
            get { return GetRoughness_Native(Parent.ID); }
            set { SetRoughness_Native(Parent.ID, value); }
        }

        public float AmbientOcclusion
        {
            get { return GetAO_Native(Parent.ID); }
            set { SetAO_Native(Parent.ID, value); }
        }

        public float Opacity
        {
            get { return GetOpacity_Native(Parent.ID); }
            set { SetOpacity_Native(Parent.ID, value); }
        }

        public float OpacityMask
        {
            get { return GetOpacityMask_Native(Parent.ID); }
            set { SetOpacityMask_Native(Parent.ID, value); }
        }

        public bool bCastsShadows
        {
            get { return DoesCastShadows_Native(Parent.ID); }
            set { SetCastsShadows_Native(Parent.ID, value); }
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern string GetText_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetText_Native(in GUID entityID, string value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern MaterialBlendMode GetBlendMode_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetBlendMode_Native(in GUID entityID, MaterialBlendMode value);

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
        internal static extern void GetAlbedo_Native(in GUID entityID, out Color3 outValue);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetAlbedo_Native(in GUID entityID, ref Color3 value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetEmissive_Native(in GUID entityID, out Color3 outValue);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetEmissive_Native(in GUID entityID, ref Color3 value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetMetallness_Native(in GUID entityID, float value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetMetallness_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetRoughness_Native(in GUID entityID, float value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetRoughness_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetAO_Native(in GUID entityID, float value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetAO_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetOpacity_Native(in GUID entityID, float value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetOpacity_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetOpacityMask_Native(in GUID entityID, float value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetOpacityMask_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetIsLit_Native(in GUID entityID, bool value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool GetIsLit_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetCastsShadows_Native(in GUID entityID, bool value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool DoesCastShadows_Native(in GUID entityID);

    }
    
    public class Text2DComponent : Component
    {
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
        public Texture2D Texture
        {
            set { SetTexture_Native(Parent.ID, value.ID); }
            get
            {
                Texture2D tex = new Texture2D(GetTexture_Native(Parent.ID));
                return tex;
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
        
        public void SetSound(in string filepath)
        {
            SetSound_Native(Parent.ID, filepath);
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
        internal static extern void SetLoopCount_Native(in GUID entityID, int loopCount);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetLooping_Native(in GUID entityID, bool bLooping);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetMuted_Native(in GUID entityID, bool bMuted);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetSound_Native(in GUID entityID, string filepath);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetStreaming_Native(in GUID entityID, bool bStreaming);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Play_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Stop_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetPaused_Native(in GUID entityID, bool bPaused);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetMinDistance_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetMaxDistance_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern RollOffModel GetRollOffModel_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetVolume_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int GetLoopCount_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool IsLooping_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool IsMuted_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool IsStreaming_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool IsPlaying_Native(in GUID entityID);
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
        public void SetIsTrigger(bool bTrigger) { SetIsTrigger_Native(Parent.ID, m_Type, bTrigger); }
        public bool IsTrigger() { return IsTrigger_Native(Parent.ID, m_Type); }
        public void SetStaticFriction(float staticFriction) { SetStaticFriction_Native(Parent.ID, m_Type, staticFriction); }
        public void SetDynamicFriction(float dynamicFriction) { SetDynamicFriction_Native(Parent.ID, m_Type, dynamicFriction); }
        public void SetBounciness(float bounciness) { SetBounciness_Native(Parent.ID, m_Type, bounciness); }
		public float GetStaticFriction() { return GetStaticFriction_Native(Parent.ID, m_Type); }
		public float GetDynamicFriction() { return GetDynamicFriction_Native(Parent.ID, m_Type); }
		public float GetBounciness() { return GetBounciness_Native(Parent.ID, m_Type); }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetIsTrigger_Native(in GUID entityID, Type type, bool bTrigger);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool IsTrigger_Native(in GUID entityID, Type type);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetDynamicFriction_Native(in GUID entityID, Type type, float value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetBounciness_Native(in GUID entityID, Type type, float value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetStaticFriction_Native(in GUID entityID, Type type, float value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetStaticFriction_Native(in GUID entityID, Type type);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetDynamicFriction_Native(in GUID entityID, Type type);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetBounciness_Native(in GUID entityID, Type type);
    }

    public class BoxColliderComponent : BaseColliderComponent
    {
        public BoxColliderComponent()
        {
            m_Type = typeof(BoxColliderComponent);
        }

        public void SetSize(ref Vector3 size) { SetSize_Native(Parent.ID, ref size); }

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

        // Only affects non-convex mesh colliders.Non-convex meshes are one-sided meaning collision won't be registered from the back side. For example, that might be a problem for windows.
        // So to fix this problem, you can set this flag to true
        public void SetIsTwoSided(bool bConvex) { SetIsTwoSided_Native(Parent.ID, bConvex); }
        public bool IsTwoSided() { return IsTwoSided_Native(Parent.ID); }

        public void SetCollisionMesh(StaticMesh mesh)
        {
            SetCollisionMesh_Native(Parent.ID, mesh.ID);
        }
        public StaticMesh GetCollisionMesh()
        {
            StaticMesh temp = new StaticMesh();
            temp.ID = GetCollisionMesh_Native(Parent.ID);
            return temp;
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
        public Type GetScriptType()
        {
            return GetScriptType_Native(Parent.ID);
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern Type GetScriptType_Native(in GUID entityID);
    }
}
