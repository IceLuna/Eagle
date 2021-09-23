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
            GetMaterial_Native(ParentID, ID, out GUID diffuse, out GUID specular, out GUID normal, out Vector4 tint, out float tilingFactor, out float shininess);
            result.DiffuseTexture.ID = diffuse;
            result.SpecularTexture.ID = specular;
            result.NormalTexture.ID = normal;
            result.TintColor = tint;
            result.TilingFactor = tilingFactor;
            result.Shininess = shininess;

            return result;
        }
        public void SetMaterial(Material material)
        {
            if (material.DiffuseTexture != null)
                SetDiffuseTexture_Native(ParentID, ID, material.DiffuseTexture.ID);
            if (material.SpecularTexture != null)
                SetSpecularTexture_Native(ParentID, ID, material.SpecularTexture.ID);
            if (material.NormalTexture != null)
                SetNormalTexture_Native(ParentID, ID, material.NormalTexture.ID);
            SetScalarMaterialParams_Native(ParentID, ID, in material.TintColor, material.TilingFactor, material.Shininess);
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern GUID Create_Native(string filepath);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool IsValid_Native(in GUID guid);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetDiffuseTexture_Native(GUID parentID, GUID meshID, GUID textureID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetSpecularTexture_Native(GUID parentID, GUID meshID, GUID textureID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetNormalTexture_Native(GUID parentID, GUID meshID, GUID textureID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetScalarMaterialParams_Native(GUID parentID, GUID meshID, in Vector4 tintColor, float tilingFactor, float shininess);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetMaterial_Native(GUID parentID, GUID meshID, out GUID diffuse, out GUID specular, out GUID normal, out Vector4 tint, out float tilingFactor, out float shininess);
    }

}
