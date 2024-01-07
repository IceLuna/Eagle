using System;
using System.Collections.Generic;
using System.Dynamic;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Text;
using System.Threading.Tasks;

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
		Mesh,
		Audio,
		SoundGroup,
		Font,
		Material,
		PhysicsMaterial
    };

    // Note that changing assets affects the whole asset, meaning it will affect the editor
    public class Asset
    {
        internal Asset(AssetType type, GUID guid)
        {
            m_Type = type;
            m_GUID = guid;
        }

        public string GetPath() { return GetPath_Native(m_GUID); }
        
        public AssetType GetAssetType() { return m_Type; }

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
                    case AssetType.Mesh: return new AssetMesh(guid);
                    case AssetType.Audio: return new AssetAudio(guid);
                    case AssetType.SoundGroup: return new Asset(type, guid);
                    case AssetType.Font: return new Asset(type, guid);
                    case AssetType.Material: return new AssetMaterial(guid);
                    case AssetType.PhysicsMaterial: return new AssetPhysicsMaterial(guid);
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

        public float GetAnisotropy() { return GetAnisotropy_Native(m_GUID); }

        public FilterMode GetFilterMode() { return GetFilterMode_Native(m_GUID); }

        public AddressMode GetAddressMode() { return GetAddressMode_Native(m_GUID); }

        public uint GetMipsCount() { return GetMipsCount_Native(m_GUID); }


        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetAnisotropy_Native(GUID id, float value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetAnisotropy_Native(GUID id);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetFilterMode_Native(GUID id, FilterMode value);

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
    }

    public class AssetTextureCube : Asset
    {
        internal AssetTextureCube(GUID guid) : base(AssetType.TextureCube, guid)
        {
        }
    }

    public class AssetMesh : Asset
    {
        internal AssetMesh(GUID guid) : base(AssetType.Mesh, guid)
        {
        }
    }

    public class AssetMaterial : Asset
    {
        internal AssetMaterial(GUID guid) : base(AssetType.Material, guid)
        {
        }

        public Material GetMaterial()
        {
            Material result = new Material();
            GetMaterial_Native(m_GUID, out GUID albedo, out GUID metallness, out GUID normal, out GUID roughness, out GUID ao, out GUID emissiveTexture, out GUID opacityTexture, out GUID opacityMaskTexture,
                out Color4 tint, out Vector3 emissiveIntensity, out float tilingFactor, out MaterialBlendMode blendMode);
            result.AlbedoAsset = new AssetTexture2D(albedo);
            result.MetallnessAsset = new AssetTexture2D(metallness);
            result.NormalAsset = new AssetTexture2D(normal);
            result.RoughnessAsset = new AssetTexture2D(roughness);
            result.AOAsset = new AssetTexture2D(ao);
            result.EmissiveAsset = new AssetTexture2D(emissiveTexture);
            result.OpacityAsset = new AssetTexture2D(opacityTexture);
            result.OpacityMaskAsset = new AssetTexture2D(opacityMaskTexture);
            result.TintColor = tint;
            result.EmissiveIntensity = emissiveIntensity;
            result.TilingFactor = tilingFactor;
            result.BlendMode = blendMode;

            return result;
        }

        void SetMaterial(Material value)
        {
            GUID nullGUID = GUID.Null();

            GUID albedoID = value.AlbedoAsset != null ? value.AlbedoAsset.GetGUID() : nullGUID;
            GUID metallnessID = value.MetallnessAsset != null ? value.MetallnessAsset.GetGUID() : nullGUID;
            GUID normalID = value.NormalAsset != null ? value.NormalAsset.GetGUID() : nullGUID;
            GUID roughnessID = value.RoughnessAsset != null ? value.RoughnessAsset.GetGUID() : nullGUID;
            GUID aoID = value.AOAsset != null ? value.AOAsset.GetGUID() : nullGUID;
            GUID emissiveID = value.EmissiveAsset != null ? value.EmissiveAsset.GetGUID() : nullGUID;
            GUID opacityID = value.OpacityAsset != null ? value.OpacityAsset.GetGUID() : nullGUID;
            GUID opacityMaskID = value.OpacityMaskAsset != null ? value.OpacityMaskAsset.GetGUID() : nullGUID;

            SetMaterial_Native(m_GUID, albedoID, metallnessID, normalID, roughnessID, aoID, emissiveID, opacityID, opacityMaskID, ref value.TintColor, ref value.EmissiveIntensity, value.TilingFactor, value.BlendMode);
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetMaterial_Native(in GUID entityID, out GUID albedo, out GUID metallness, out GUID normal, out GUID roughness, out GUID ao, out GUID emissiveTexture, out GUID opacityTexture, out GUID opacityMaskTexture,
            out Color4 tint, out Vector3 emissiveIntensity, out float tilingFactor, out MaterialBlendMode blendMode);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetMaterial_Native(in GUID entityID, in GUID albedo, in GUID metallness, in GUID normal, in GUID roughness, in GUID ao, in GUID emissiveTexture, in GUID opacityTexture, in GUID opacityMaskTexture,
            ref Color4 tint, ref Vector3 emissiveIntensity, float tilingFactor, MaterialBlendMode blendMode);
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

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetVolume_Native(GUID id, float value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetVolume_Native(GUID id);
    }

    public class AssetPhysicsMaterial : Asset
    {
        internal AssetPhysicsMaterial(GUID guid) : base(AssetType.PhysicsMaterial, guid)
        {
        }

        float GetStaticFriction()
        {
            return GetStaticFriction_Native(m_GUID);
        }

        float GetDynamicFriction()
        {
            return GetDynamicFriction_Native(m_GUID);
        }

        float GetBounciness()
        {
            return GetBounciness_Native(m_GUID);
        }

        void SetStaticFriction(float value)
        {
            SetStaticFriction_Native(m_GUID, value);
        }

        void SetDynamicFriction(float value)
        {
            SetDynamicFriction_Native(m_GUID, value);
        }

        void SetBounciness(float value)
        {
            SetBounciness_Native(m_GUID, value);
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetDynamicFriction_Native(in GUID entityID, float value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetBounciness_Native(in GUID entityID, float value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetStaticFriction_Native(in GUID entityID, float value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetStaticFriction_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetDynamicFriction_Native(in GUID entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetBounciness_Native(in GUID entityID);
    }
}
