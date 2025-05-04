using System;
using System.Runtime.CompilerServices;

namespace Eagle
{
    public enum FilterMode
    {
        Point,
        Bilinear,
        Trilinear
    };

    public enum AddressMode
    {
        Wrap,
        Mirror,
        Clamp,
        ClampToOpaqueBlack,
        ClampToOpaqueWhite
    };

    public enum AssetType
    {
        None,
		Texture2D,
		TextureCube,
		StaticMesh,
		SkeletalMesh,
		Audio,
		SoundGroup,
		Font,
		Material,
		PhysicsMaterial,
        Entity,
        Scene,
        Animation,
        AnimationGraph,
        ParticleSystem,
    };

    public enum AssetTexture2DFormat
    {
        RGBA8,
		RG8,
		R8,

		Default = RGBA8
    };

    public enum AssetTextureCubeFormat
    {
        RGBA32,
		RGBA16,
		R11G11B10,

		Default = R11G11B10
    };

    // Note that changing assets affects the whole asset, meaning it will affect the editor
    public class Asset
    {
        internal Asset(AssetType type, GUID guid)
        {
            m_Type = type;
            m_GUID = guid;
        }

        internal Asset(GUID guid)
        {
            m_Type = AssetType.None;
            m_GUID = guid;
        }

        public string GetPath() { return GetPath_Native(m_GUID); }
        
        public AssetType GetAssetType()
        {
            if (m_Type != AssetType.None)
                return m_Type;
            return GetAssetType_Native(m_GUID);
        }

        public GUID GetGUID() { return m_GUID; }

        public static Asset Get(string path)
        {
            if (Get_Native(path, out AssetType type, out GUID guid))
            {
                switch (type)
                {
                    case AssetType.None: return null;
                    case AssetType.Texture2D: return new AssetTexture2D(guid);
                    case AssetType.TextureCube: return new AssetTextureCube(guid);
                    case AssetType.StaticMesh: return new AssetStaticMesh(guid);
                    case AssetType.SkeletalMesh: return new AssetSkeletalMesh(guid);
                    case AssetType.Audio: return new AssetAudio(guid);
                    case AssetType.SoundGroup: return new AssetSoundGroup(guid);
                    case AssetType.Font: return new AssetFont(guid);
                    case AssetType.Material: return new AssetMaterial(guid);
                    case AssetType.PhysicsMaterial: return new AssetPhysicsMaterial(guid);
                    case AssetType.Entity: return new AssetEntity(guid);
                    case AssetType.Scene: return new AssetScene(guid);
                    case AssetType.Animation: return new AssetAnimation(guid);
                    case AssetType.AnimationGraph: return new AssetAnimationGraph(guid);
                    case AssetType.ParticleSystem: return new AssetParticleSystem(guid);
                    default: return null;
                }
            }
            return null;
        }

        protected AssetType m_Type;
        protected GUID m_GUID;

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool Get_Native(string filepath, out AssetType type, out GUID guid);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern string GetPath_Native(GUID guid);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern AssetType GetAssetType_Native(GUID guid);
    }

    public class AssetTexture2D : Asset
    {
        internal AssetTexture2D(GUID guid) : base(AssetType.Texture2D, guid)
        {
        }

        public void SetAnisotropy(float anisotropy) { SetAnisotropy_Native(m_GUID, anisotropy); }

        public void SetFilterMode(FilterMode filterMode) { SetFilterMode_Native(m_GUID, filterMode); }

        public void SetAddressMode(AddressMode addressMode) { SetAddressMode_Native(m_GUID, addressMode); }

        public void SetMipsCount(uint mips) { SetMipsCount_Native(m_GUID, mips); }

        public void SetFormat(AssetTexture2DFormat format) { SetFormat_Native(m_GUID, format); }

        public void SetIsNormalMap(bool bNormalMap) { SetIsNormalMap_Native(m_GUID, bNormalMap); }

        public void SetNeedsAlpha(bool bNeedsAlpha) { SetNeedsAlpha_Native(m_GUID, bNeedsAlpha); }

        public void SetIsCompressed(bool bCompressed) { SetIsCompressed_Native(m_GUID, bCompressed); }

        public float GetAnisotropy() { return GetAnisotropy_Native(m_GUID); }

        public FilterMode GetFilterMode() { return GetFilterMode_Native(m_GUID); }

        public AddressMode GetAddressMode() { return GetAddressMode_Native(m_GUID); }

        public uint GetMipsCount() { return GetMipsCount_Native(m_GUID); }

        public AssetTexture2DFormat GetFormat() { return GetFormat_Native(m_GUID); }

        public bool IsNormalMap() { return IsNormalMap_Native(m_GUID); }

        public bool DoesNeedAlpha() { return DoesNeedAlpha_Native(m_GUID); }

        public bool IsCompressed() { return IsCompressed_Native(m_GUID); }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetAnisotropy_Native(GUID id, float value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetAnisotropy_Native(GUID id);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetFilterMode_Native(GUID id, FilterMode value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetFormat_Native(GUID id, AssetTexture2DFormat value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetIsNormalMap_Native(GUID id, bool value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetNeedsAlpha_Native(GUID id, bool value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetIsCompressed_Native(GUID id, bool value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern FilterMode GetFilterMode_Native(GUID id);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetAddressMode_Native(GUID id, AddressMode value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern AddressMode GetAddressMode_Native(GUID id);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetMipsCount_Native(GUID id, uint value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern uint GetMipsCount_Native(GUID id);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern AssetTexture2DFormat GetFormat_Native(GUID id);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool IsNormalMap_Native(GUID id);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool DoesNeedAlpha_Native(GUID id);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool IsCompressed_Native(GUID id);
    }

    public class AssetTextureCube : Asset
    {
        internal AssetTextureCube(GUID guid) : base(AssetType.TextureCube, guid)
        {
        }

        public void SetLayerSize(uint layerSize) { SetLayerSize_Native(m_GUID, layerSize); }

        public void SetPrefilterSize(uint prefilter) { SetPrefilterSize_Native(m_GUID, prefilter); }

        public bool SetFormat(AssetTextureCubeFormat format) { return SetFormat_Native(m_GUID, format); }

        public uint GetLayerSize() { return GetLayerSize_Native(m_GUID); }

        public uint GetPrefilterSize() { return GetPrefilterSize_Native(m_GUID); }

        public AssetTextureCubeFormat GetFormat() { return GetFormat_Native(m_GUID); }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetLayerSize_Native(GUID id, uint value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetPrefilterSize_Native(GUID id, uint value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool SetFormat_Native(GUID id, AssetTextureCubeFormat value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern uint GetLayerSize_Native(GUID id);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern uint GetPrefilterSize_Native(GUID id);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern AssetTextureCubeFormat GetFormat_Native(GUID id);
    }

    public class AssetStaticMesh : Asset
    {
        internal AssetStaticMesh(GUID guid) : base(AssetType.StaticMesh, guid)
        {
        }
    }

    public class AssetSkeletalMesh : Asset
    {
        internal AssetSkeletalMesh(GUID guid) : base(AssetType.SkeletalMesh, guid)
        {
        }
    }

    public class AssetMaterial : Asset
    {
        internal AssetMaterial(GUID guid) : base(AssetType.Material, guid)
        {
        }

        public static AssetMaterial Create()
        {
            GUID id = Create_Native();
            if (id.IsNull())
                return null;

            return new AssetMaterial(id);
        }

        public static AssetMaterial Create(Material value)
        {
            if (value == null)
                return null;

            AssetMaterial asset = Create();
            if (asset != null)
            {
                asset.SetMaterial(value);
            }
            return asset;
        }

        public static AssetMaterial Create(AssetMaterial material)
        {
            if (material == null)
                return null;

            GUID id = CreateFromAsset_Native(material.GetGUID());
            if (id.IsNull())
                return null;

            return new AssetMaterial(id);
        }

        public Material GetMaterial()
        {
            Material result = new Material();
            GetMaterial_Native(m_GUID,
                out GUID albedoTexture, out GUID metalnessTexture, out GUID normalTexture, out GUID roughnessTexture, out GUID aoTexture, out GUID emissiveTexture, out GUID opacityTexture, out GUID opacityMaskTexture,
                out Color3 albedo, out float metalness, out float roughness, out float ao, out Color3 emissive, out float opacity, out float opacityMask,
                out bool bUseAlbedoTexture, out bool bUseMetalnessTexture, out bool bUseRoughnessTexture, out bool bUseAOTexture, out bool bUseEmissiveTexture, out bool bUseOpacityTexture, out bool bUseOpacityMaskTexture,
                out Color4 tint, out Color3 emissiveIntensity, out float tilingFactor, out MaterialBlendMode blendMode,
                out TextureChannel metalnessTextureChannel, out TextureChannel roughnessTextureChannel, out TextureChannel aoTextureChannel, out TextureChannel opacityTextureChannel, out TextureChannel opacityMaskTextureChannel);

            result.AlbedoAsset = new AssetTexture2D(albedoTexture);
            result.MetalnessAsset = new AssetTexture2D(metalnessTexture);
            result.NormalAsset = new AssetTexture2D(normalTexture);
            result.RoughnessAsset = new AssetTexture2D(roughnessTexture);
            result.AOAsset = new AssetTexture2D(aoTexture);
            result.EmissiveAsset = new AssetTexture2D(emissiveTexture);
            result.OpacityAsset = new AssetTexture2D(opacityTexture);
            result.OpacityMaskAsset = new AssetTexture2D(opacityMaskTexture);

            result.MetalnessTextureChannel = metalnessTextureChannel;
            result.RoughnessTextureChannel = roughnessTextureChannel;
            result.AOTextureChannel = aoTextureChannel;
            result.OpacityTextureChannel = opacityTextureChannel;
            result.OpacityMaskTextureChannel = opacityMaskTextureChannel;

            result.Albedo = albedo;
            result.Metalness = metalness;
            result.Roughness = roughness;
            result.AO = ao;
            result.Emissive = emissive;
            result.Opacity = opacity;
            result.OpacityMask = opacityMask;

            result.bUseAlbedoTexture = bUseAlbedoTexture;
            result.bUseMetalnessTexture = bUseMetalnessTexture;
            result.bUseRoughnessTexture = bUseRoughnessTexture;
            result.bUseAOTexture = bUseAOTexture;
            result.bUseEmissiveTexture = bUseEmissiveTexture;
            result.bUseOpacityTexture = bUseOpacityTexture;
            result.bUseOpacityMaskTexture = bUseOpacityMaskTexture;

            result.TintColor = tint;
            result.EmissiveIntensity = emissiveIntensity;
            result.TilingFactor = tilingFactor;
            result.BlendMode = blendMode;

            return result;
        }

        public void SetMaterial(Material value)
        {
            GUID nullGUID = GUID.Null();

            GUID albedoID = value.AlbedoAsset != null ? value.AlbedoAsset.GetGUID() : nullGUID;
            GUID metalnessID = value.MetalnessAsset != null ? value.MetalnessAsset.GetGUID() : nullGUID;
            GUID normalID = value.NormalAsset != null ? value.NormalAsset.GetGUID() : nullGUID;
            GUID roughnessID = value.RoughnessAsset != null ? value.RoughnessAsset.GetGUID() : nullGUID;
            GUID aoID = value.AOAsset != null ? value.AOAsset.GetGUID() : nullGUID;
            GUID emissiveID = value.EmissiveAsset != null ? value.EmissiveAsset.GetGUID() : nullGUID;
            GUID opacityID = value.OpacityAsset != null ? value.OpacityAsset.GetGUID() : nullGUID;
            GUID opacityMaskID = value.OpacityMaskAsset != null ? value.OpacityMaskAsset.GetGUID() : nullGUID;

            SetMaterial_Native(m_GUID,
                albedoID, metalnessID, normalID, roughnessID, aoID, emissiveID, opacityID, opacityMaskID,
                ref value.Albedo, value.Metalness, value.Roughness, value.AO, ref value.Emissive, value.Opacity, value.OpacityMask,
                value.bUseAlbedoTexture, value.bUseMetalnessTexture, value.bUseRoughnessTexture, value.bUseAOTexture, value.bUseEmissiveTexture, value.bUseOpacityTexture, value.bUseOpacityMaskTexture,
                ref value.TintColor, ref value.EmissiveIntensity, value.TilingFactor, value.BlendMode,
                value.MetalnessTextureChannel, value.RoughnessTextureChannel, value.AOTextureChannel, value.OpacityTextureChannel, value.OpacityMaskTextureChannel);
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetMaterial_Native(in GUID assetID,
            out GUID albedoTexture, out GUID metalnessTexture, out GUID normalTexture, out GUID roughnessTexture, out GUID aoTexture, out GUID emissiveTexture, out GUID opacityTexture, out GUID opacityMaskTexture,
            out Color3 albedo, out float metalness, out float roughness, out float ao, out Color3 emissive, out float opacity, out float opacityMask,
            out bool bUseAlbedoTexture, out bool bUseMetalnessTexture, out bool bUseRoughnessTexture, out bool bUseAOTexture, out bool bUseEmissiveTexture, out bool bUseOpacityTexture, out bool bUseOpacityMaskTexture,
            out Color4 tint, out Color3 emissiveIntensity, out float tilingFactor, out MaterialBlendMode blendMode,
            out TextureChannel metalnessTextureChannel, out TextureChannel roughnessTextureChannel, out TextureChannel aoTextureChannel, out TextureChannel opacityTextureChannel, out TextureChannel opacityMaskTextureChannel);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetMaterial_Native(in GUID assetID,
            in GUID albedoTexture, in GUID metalnessTexture, in GUID normalTexture, in GUID roughnessTexture, in GUID aoTexture, in GUID emissiveTexture, in GUID opacityTexture, in GUID opacityMaskTexture,
            ref Color3 albedo, float metalness, float roughness, float ao, ref Color3 emissive, float opacity, float opacityMask,
            bool bUseAlbedoTexture, bool bUseMetalnessTexture, bool bUseRoughnessTexture, bool bUseAOTexture, bool bUseEmissiveTexture, bool bUseOpacityTexture, bool bUseOpacityMaskTexture,
            ref Color4 tint, ref Color3 emissiveIntensity, float tilingFactor, MaterialBlendMode blendMode,
            TextureChannel metalnessTextureChannel, TextureChannel roughnessTextureChannel, TextureChannel aoTextureChannel, TextureChannel opacityTextureChannel, TextureChannel opacityMaskTextureChannel);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern GUID Create_Native();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern GUID CreateFromAsset_Native(GUID assetID);
    }

    public class AssetAudio : Asset
    {
        internal AssetAudio(GUID guid) : base(AssetType.Audio, guid)
        {
        }

        public void SetVolume(float volume)
        {
            SetVolume_Native(m_GUID, volume);
        }

        public float GetVolume()
        {
            return GetVolume_Native(m_GUID);
        }

        public void SetPitch(float pitch)
        {
            SetPitch_Native(m_GUID, pitch);
        }

        public float GetPitch()
        {
            return GetPitch_Native(m_GUID);
        }

        public void SetPan(float pan)
        {
            SetPan_Native(m_GUID, pan);
        }

        public float GetPan()
        {
            return GetPan_Native(m_GUID);
        }

        public void SetSoundGroup(AssetSoundGroup soundGroup)
        {
            SetSoundGroup_Native(m_GUID, soundGroup != null ? soundGroup.GetGUID() : GUID.Null());
        }

        public AssetSoundGroup GetSoundGroup()
        {
            GUID soundGroupGUID = GetSoundGroup_Native(m_GUID);
            if (soundGroupGUID.IsNull())
                return null;
            return new AssetSoundGroup(soundGroupGUID);
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetVolume_Native(GUID id, float value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetVolume_Native(GUID id);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetPitch_Native(GUID id, float pitch);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetPitch_Native(GUID id);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetPan_Native(GUID id, float pan);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetPan_Native(GUID id);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetSoundGroup_Native(GUID id, GUID soundGroupID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern GUID GetSoundGroup_Native(GUID id);
    }

    public class AssetPhysicsMaterial : Asset
    {
        internal AssetPhysicsMaterial(GUID guid) : base(AssetType.PhysicsMaterial, guid)
        {
        }

        public static AssetPhysicsMaterial Create(float staticFriction = 0.6f, float dynamicFriction = 0.6f, float bounciness = 0.5f)
        {
            GUID id = Create_Native(staticFriction, dynamicFriction, bounciness);
            if (id.IsNull())
                return null;

            return new AssetPhysicsMaterial(id);
        }

        public static AssetPhysicsMaterial Create(AssetPhysicsMaterial material)
        {
            if (material == null)
                return null;

            GUID id = CreateFromAsset_Native(material.GetGUID());
            if (id.IsNull())
                return null;

            return new AssetPhysicsMaterial(id);
        }

        public float GetStaticFriction()
        {
            return GetStaticFriction_Native(m_GUID);
        }

        public float GetDynamicFriction()
        {
            return GetDynamicFriction_Native(m_GUID);
        }

        public float GetBounciness()
        {
            return GetBounciness_Native(m_GUID);
        }

        public void SetStaticFriction(float value)
        {
            SetStaticFriction_Native(m_GUID, value);
        }

        public void SetDynamicFriction(float value)
        {
            SetDynamicFriction_Native(m_GUID, value);
        }

        public void SetBounciness(float value)
        {
            SetBounciness_Native(m_GUID, value);
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetDynamicFriction_Native(in GUID assetID, float value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetBounciness_Native(in GUID assetID, float value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetStaticFriction_Native(in GUID assetID, float value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetStaticFriction_Native(in GUID assetID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetDynamicFriction_Native(in GUID assetID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetBounciness_Native(in GUID assetID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern GUID Create_Native(float staticFriction, float dynamicFriction, float bounciness);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern GUID CreateFromAsset_Native(GUID assetID);
    }

    public class AssetFont : Asset
    {
        internal AssetFont(GUID guid) : base(AssetType.Font, guid)
        {
        }
    }

    public class AssetSoundGroup : Asset
    {
        internal AssetSoundGroup(GUID guid) : base(AssetType.SoundGroup, guid)
        {
        }

        public void Stop()
        {
            Stop_Native(m_GUID);
        }

        public void SetPaused(bool bPaused)
        {
            SetPaused_Native(m_GUID, bPaused);
        }

        public void SetVolume(float volume)
        {
            SetVolume_Native(m_GUID, volume);
        }

        public void SetMuted(bool bMuted)
        {
            SetMuted_Native(m_GUID, bMuted);
        }

        //@ Pitch. Any value between 0 and 10
        public void SetPitch(float pitch)
        {
            SetPitch_Native(m_GUID, pitch);
        }

        public float GetVolume()
        {
            return GetVolume_Native(m_GUID);
        }

        public float GetPitch()
        {
            return GetPitch_Native(m_GUID);
        }

        public bool IsPaused()
        {
            return IsPaused_Native(m_GUID);
        }

        public bool IsMuted()
        {
            return IsMuted_Native(m_GUID);
        }


        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Stop_Native(in GUID assetID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetPaused_Native(in GUID assetID, bool value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetVolume_Native(in GUID assetID, float value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetMuted_Native(in GUID assetID, bool value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetPitch_Native(in GUID assetID, float value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetVolume_Native(in GUID assetID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetPitch_Native(in GUID assetID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool IsPaused_Native(in GUID assetID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool IsMuted_Native(in GUID assetID);
    }

    public class AssetEntity: Asset
    {
        internal AssetEntity(GUID guid) : base(AssetType.Entity, guid)
        {
        }
    }

    public class AssetScene: Asset
    {
        internal AssetScene(GUID guid) : base(AssetType.Scene, guid)
        {
        }
    }

    public class AssetAnimation: Asset
    {
        internal AssetAnimation(GUID guid) : base(AssetType.Animation, guid)
        {
        }

        public void SetRootMotionEnabled(bool bEnabled) { SetRootMotionEnabled_Native(m_GUID, bEnabled); }

        // Duration in ticks
        public float GetDuration() { return GetDuration_Native(m_GUID); }

        public float GetTicksPerSecond() { return GetTicksPerSecond_Native(m_GUID); }

        // Note: there can be multiple events with the same name
        // @name. When the event is triggered, C# `Entity.OnAnimationEvent()` is called with this as a parameter.
        // @time. A value between [0; Duration] when an event should be triggered.
        public void AddAnimationEvent(string name, float time) { AddAnimationEvent_Native(m_GUID, name, time); }

        // Returns true if event was removed
        public bool RemoveAnimationEvent(string name) { return RemoveAnimationEvent_Native(m_GUID, name); }

        public bool HasAnimationEvent(string name) { return HasAnimationEvent_Native(m_GUID, name); }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetRootMotionEnabled_Native(GUID id, bool value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetDuration_Native(GUID id);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetTicksPerSecond_Native(GUID id);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void AddAnimationEvent_Native(GUID id, string name, float time);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool RemoveAnimationEvent_Native(GUID id, string name);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool HasAnimationEvent_Native(GUID id, string name);
    }

    public class AssetAnimationGraph: Asset
    {
        internal AssetAnimationGraph(GUID guid) : base(AssetType.AnimationGraph, guid)
        {
        }
    }

    public class AssetParticleSystem : Asset
    {
        internal AssetParticleSystem(GUID guid) : base(AssetType.ParticleSystem, guid)
        {
        }

        public static AssetParticleSystem Create()
        {
            return new AssetParticleSystem(Create_Native());
        }

        public static AssetParticleSystem Create(AssetParticleSystem asset)
        {
            if (asset == null)
                return null;

            AssetParticleSystem newAsset = Create();
            if (newAsset != null)
            {
                newAsset.SetEmitters(asset.GetEmitters());
            }

            return null;
        }

        public uint GetEmittersCount()
        {
            return GetEmittersCount_Native(m_GUID);
        }

        public ParticleEmitter[] GetEmitters()
        {
            uint count = GetEmittersCount_Native(m_GUID);
            ParticleEmitter[] emitters = new ParticleEmitter[count];
            for (uint i = 0; i < count; ++i)
            {
                GUID textureID;
                GUID meshID;

                string name = GetEmitter_Native(m_GUID, i,
                    out textureID, out emitters[i].ColorStart, out emitters[i].ColorEnd, out emitters[i].VelocityMin, out emitters[i].VelocityMax,
                    out emitters[i].VelocityCoefStart, out emitters[i].VelocityCoefEnd, out emitters[i].RotationZStart, out emitters[i].RotationZEnd,
                    out emitters[i].SizeStart, out emitters[i].SizeEnd, out emitters[i].ColliderSizeRatio, out emitters[i].LifetimeMin, out emitters[i].LifetimeMax,
                    out emitters[i].BouncinessMin, out emitters[i].BouncinessMax, out emitters[i].RelativeTransform, out emitters[i].VisibilityAABB,
                    out emitters[i].LoopCount, out emitters[i].NumParticles, out emitters[i].NumParticlesRatio, out emitters[i].RadialAcceleration, out emitters[i].TangentialAcceleration,
                    out emitters[i].NormalVelocityFactor, out emitters[i].EmissionShape, out emitters[i].SphereRadius, out emitters[i].BoxMin, out emitters[i].BoxMax,
                    out emitters[i].RingRadius, out emitters[i].RingThickness, out meshID, out emitters[i].CollisionMode, out emitters[i].AnimationImagesNum,
                    out emitters[i].AnimationSpeed, out emitters[i].bDestroyImmediately, out emitters[i].bEmit, out emitters[i].bExplode, out emitters[i].bApplyGravity, out emitters[i].bAlphaBlending,
                    out emitters[i].bAdditive, out emitters[i].bBlendAnimation, out emitters[i].bFaceDirection);

                emitters[i].TextureAsset = new AssetTexture2D(textureID);
                emitters[i].MeshAsset = new AssetStaticMesh(meshID);
                emitters[i].Name = name;
            }

            return emitters;
        }

        public void SetEmitters(ParticleEmitter[] emitters)
        {
            if (emitters.Length == 0)
            {
                RemoveEmitters_Native(m_GUID);
            }
            else
            {
                IntPtr data = SetEmitters_Prepare_Native((uint)emitters.Length);

                for (uint i = 0; i < emitters.Length; i++)
                {
                    GUID textureID = emitters[i].TextureAsset != null ? emitters[i].TextureAsset.GetGUID() : GUID.Null();
                    GUID meshID = emitters[i].MeshAsset != null ? emitters[i].MeshAsset.GetGUID() : GUID.Null();

                    SetEmitter_Native(data, i,
                        textureID, ref emitters[i].ColorStart, ref emitters[i].ColorEnd, ref emitters[i].VelocityMin, ref emitters[i].VelocityMax,
                        ref emitters[i].VelocityCoefStart, ref emitters[i].VelocityCoefEnd, emitters[i].RotationZStart, emitters[i].RotationZEnd,
                        ref emitters[i].SizeStart, ref emitters[i].SizeEnd, ref emitters[i].ColliderSizeRatio, emitters[i].LifetimeMin, emitters[i].LifetimeMax,
                        emitters[i].BouncinessMin, emitters[i].BouncinessMax, emitters[i].Name, ref emitters[i].RelativeTransform, ref emitters[i].VisibilityAABB,
                        emitters[i].LoopCount, emitters[i].NumParticles, emitters[i].NumParticlesRatio, emitters[i].RadialAcceleration, emitters[i].TangentialAcceleration,
                        emitters[i].NormalVelocityFactor, emitters[i].EmissionShape, ref emitters[i].SphereRadius, ref emitters[i].BoxMin, ref emitters[i].BoxMax,
                        ref emitters[i].RingRadius, ref emitters[i].RingThickness, meshID, emitters[i].CollisionMode, ref emitters[i].AnimationImagesNum,
                        emitters[i].AnimationSpeed, emitters[i].bDestroyImmediately, emitters[i].bEmit, emitters[i].bExplode, emitters[i].bApplyGravity, emitters[i].bAlphaBlending,
                        emitters[i].bAdditive, emitters[i].bBlendAnimation, emitters[i].bFaceDirection);
                }

                SetEmitters_Finish_Native(m_GUID, data);
            }
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern GUID Create_Native();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern uint GetEmittersCount_Native(GUID id);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void RemoveEmitters_Native(GUID id);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern IntPtr SetEmitters_Prepare_Native(uint count);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetEmitters_Finish_Native(GUID id, IntPtr data);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetEmitter_Native(IntPtr data, uint index,
            GUID texture, ref Color4 colorStart, ref Color4 colorEnd, ref Vector3 velocityMin, ref Vector3 velocityMax,
            ref Vector3 velocityCoefStart, ref Vector3 velocityCoefEnd, float rotationZStart, float rotationZEnd,
            ref Vector2 sizeStart, ref Vector2 sizeEnd, ref Vector2 colliderSizeRatio, float lifetimeMin, float lifetimeMax,
            float bouncinessMin, float bouncinessMax, string name, ref Transform relativeTransform, ref AABB visibilityAABB,
            uint loopCount, uint numParticles, float numParticlesRatio, float radialAcceleration, float tangentialAcceleration,
            float normalVelocityFactor, EmitterEmissionShapeType emissionShape, ref Vector3 sphereRadius, ref Vector3 boxMin, ref Vector3 boxMax,
            ref Vector3 ringRadius, ref Vector3 ringThickness, GUID meshAsset, EmitterCollisionModeType collisionMode, ref UVector2 animationImagesNum,
            float animationSpeed, bool bDestroyImmediately, bool bEmit, bool bExplode, bool bApplyGravity, bool bAlphaBlending,
            bool bAdditive, bool bBlendAnimation, bool bFaceDirection);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern string GetEmitter_Native(GUID id, uint index,
            out GUID texture, out Color4 colorStart, out Color4 colorEnd, out Vector3 velocityMin, out Vector3 velocityMax,
            out Vector3 velocityCoefStart, out Vector3 velocityCoefEnd, out float rotationZStart, out float rotationZEnd,
            out Vector2 sizeStart, out Vector2 sizeEnd, out Vector2 colliderSizeRatio, out float lifetimeMin, out float lifetimeMax,
            out float bouncinessMin, out float bouncinessMax, out Transform relativeTransform, out AABB visibilityAABB,
            out uint loopCount, out uint numParticles, out float numParticlesRatio, out float radialAcceleration, out float tangentialAcceleration,
            out float normalVelocityFactor, out EmitterEmissionShapeType emissionShape, out Vector3 sphereRadius, out Vector3 boxMin, out Vector3 boxMax,
            out Vector3 ringRadius, out Vector3 ringThickness, out GUID meshAsset, out EmitterCollisionModeType collisionMode, out UVector2 animationImagesNum,
            out float animationSpeed, out bool bDestroyImmediately, out bool bEmit, out bool bExplode, out bool bApplyGravity, out bool bAlphaBlending,
            out bool bAdditive, out bool bBlendAnimation, out bool bFaceDirection);
    }
}
