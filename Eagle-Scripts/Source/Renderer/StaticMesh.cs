using System.Runtime.CompilerServices;

namespace Eagle
{
    public class StaticMesh
    {
        //Entity ID. If valid, functions get params from staticmesh component
        public GUID ParentID;

        public GUID ID;

        public StaticMesh()
        { }

        public StaticMesh(string filepath)
        {
            ID = Create_Native(filepath);
        }

        public bool IsValid()
        {
            return IsValid_Native(ID);
        }

        public Material GetMaterial() 
        {
            Material result = new Material();
            GetMaterial_Native(ParentID, ID, out GUID albedo, out GUID metallness, out GUID normal, out GUID roughness, out GUID ao, out GUID emissive, out Vector4 tint, out float tilingFactor);
            result.AlbedoTexture.ID = albedo;
            result.MetallnessTexture.ID = metallness;
            result.NormalTexture.ID = normal;
            result.RoughnessTexture.ID = roughness;
            result.AOTexture.ID = ao;
            result.EmissiveTexture.ID = emissive;
            result.TintColor = tint;
            result.TilingFactor = tilingFactor;

            return result;
        }
        public void SetMaterial(Material material)
        {
            if (material.AlbedoTexture != null)
                SetAlbedoTexture_Native(ParentID, ID, material.AlbedoTexture.ID);
            if (material.MetallnessTexture != null)
                SetMetallnessTexture_Native(ParentID, ID, material.MetallnessTexture.ID);
            if (material.NormalTexture != null)
                SetNormalTexture_Native(ParentID, ID, material.NormalTexture.ID);
            if (material.RoughnessTexture != null)
                SetRoughnessTexture_Native(ParentID, ID, material.RoughnessTexture.ID);
            if (material.AOTexture != null)
                SetAOTexture_Native(ParentID, ID, material.AOTexture.ID);
            if (material.EmissiveTexture != null)
                SetEmissiveTexture_Native(ParentID, ID, material.EmissiveTexture.ID);

            SetScalarMaterialParams_Native(ParentID, ID, in material.TintColor, material.TilingFactor);
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern GUID Create_Native(string filepath);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool IsValid_Native(in GUID guid);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetAlbedoTexture_Native(GUID parentID, GUID meshID, GUID textureID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetMetallnessTexture_Native(GUID parentID, GUID meshID, GUID textureID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetNormalTexture_Native(GUID parentID, GUID meshID, GUID textureID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetRoughnessTexture_Native(GUID parentID, GUID meshID, GUID textureID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetAOTexture_Native(GUID parentID, GUID meshID, GUID textureID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetEmissiveTexture_Native(GUID parentID, GUID meshID, GUID textureID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetScalarMaterialParams_Native(GUID parentID, GUID meshID, in Vector4 tintColor, float tilingFactor);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetMaterial_Native(GUID parentID, GUID meshID, out GUID albedo, out GUID metallness, out GUID normal, out GUID roughness, out GUID ao, out GUID emissive, out Vector4 tint, out float tilingFactor);
    }

}
