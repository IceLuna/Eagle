﻿using System;
using System.Runtime.CompilerServices;

namespace Eagle
{
    public abstract class Component
    {
        public Entity Parent { get; set; }
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
                return result ;
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

    public abstract class LightComponent : SceneComponent
    {
        public LightComponent()
        {
            m_Type = typeof(LightComponent);
        }

        public Vector3 LightColor
        {
            get
            {
                GetLightColor_Native(Parent.ID, m_Type, out Vector3 result);
                return result;
            }
            set
            {
                SetLightColor_Native(Parent.ID, m_Type, ref value);
            }
        }

        public Vector3 AmbientColor
        {
            get
            {
                GetAmbientColor_Native(Parent.ID, m_Type, out Vector3 result);
                return result;
            }
            set
            {
                SetAmbientColor_Native(Parent.ID, m_Type, ref value);
            }
        }

        public Vector3 SpecularColor
        {
            get
            {
                GetSpecularColor_Native(Parent.ID, m_Type, out Vector3 result);
                return result;
            }
            set
            {
                SetSpecularColor_Native(Parent.ID, m_Type, ref value);
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
                SetAffectsWorld_Native(Parent.ID, m_Type, ref value);
            }
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetLightColor_Native(in GUID entityID, Type type, out Vector3 outLightColor);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetAmbientColor_Native(in GUID entityID, Type type, out Vector3 outAmbientColor);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetSpecularColor_Native(in GUID entityID, Type type, out Vector3 outSpecularColor);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool GetAffectsWorld_Native(in GUID entityID, Type type);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetLightColor_Native(in GUID entityID, Type type, ref Vector3 lightColor);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetAmbientColor_Native(in GUID entityID, Type type, ref Vector3 ambientColor);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetSpecularColor_Native(in GUID entityID, Type type, ref Vector3 specularColor);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetAffectsWorld_Native(in GUID entityID, Type type, ref bool value);
    }

    public class PointLightComponent : LightComponent
    {
        public PointLightComponent()
        {
            m_Type = typeof(PointLightComponent);
        }

        public float Intensity
        {
            get
            {
                GetIntensity_Native(Parent.ID, out float result);
                return result;
            }
            set
            {
                SetIntensity_Native(Parent.ID, ref value);
            }
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetIntensity_Native(in GUID entityID, out float outDistance);
        
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetIntensity_Native(in GUID entityID, ref float intensity);
    }

    public class DirectionalLightComponent : LightComponent
    {
        public DirectionalLightComponent()
        {
            m_Type = typeof(DirectionalLightComponent);
        }
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
                GetInnerCutoffAngle_Native(Parent.ID, out float result);
                return result;
            }
            set
            {
                SetInnerCutoffAngle_Native(Parent.ID, ref value);
            }
        }
        public float OuterCutOffAngle
        {
            get
            {
                GetOuterCutoffAngle_Native(Parent.ID, out float result);
                return result;
            }
            set
            {
                SetOuterCutoffAngle_Native(Parent.ID, ref value);
            }
        }

        public float Intensity
        {
            get
            {
                GetIntensity_Native(Parent.ID, out float result);
                return result;
            }
            set
            {
                SetIntensity_Native(Parent.ID, ref value);
            }
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetInnerCutoffAngle_Native(in GUID entityID, out float outInnerCutoffAngle);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetOuterCutoffAngle_Native(in GUID entityID, out float outOuterCutoffAngle);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetInnerCutoffAngle_Native(in GUID entityID, ref float innerCutoffAngle);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetOuterCutoffAngle_Native(in GUID entityID, ref float outerCutoffAngle);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetIntensity_Native(in GUID entityID, ref float intensity);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetIntensity_Native(in GUID entityID, out float outDistance);
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
                temp.ParentID = Parent.ID;
                temp.ID = GetMesh_Native(Parent.ID);
                return temp;
            }

            set
            {
                SetMesh_Native(Parent.ID, value.ID);
            }
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetMesh_Native(in GUID entityID, GUID meshGUID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern GUID GetMesh_Native(in GUID entityID);
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
}
