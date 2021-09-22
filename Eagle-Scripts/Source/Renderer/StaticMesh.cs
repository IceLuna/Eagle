using System.Runtime.CompilerServices;

namespace Eagle
{
    public class StaticMesh
    {
        public GUID ID;
        protected Material m_Material = new Material();

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

        public Material GetMaterial() { return m_Material; }
        public void SetMaterial(Material material)
        {
            m_Material = material;
            if (m_Material.DiffuseTexture != null)
                SetDiffuseTexture_Native(ID, m_Material.DiffuseTexture.ID);
            if (m_Material.SpecularTexture != null)
                SetSpecularTexture_Native(ID, m_Material.SpecularTexture.ID);
            if (m_Material.NormalTexture != null)
                SetNormalTexture_Native(ID, m_Material.NormalTexture.ID);
            SetScalarMaterialParams_Native(ID, in m_Material.TintColor, m_Material.TilingFactor, m_Material.Shininess);
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern GUID Create_Native(string filepath);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool IsValid_Native(in GUID guid);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetDiffuseTexture_Native(GUID meshID, GUID textureID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetSpecularTexture_Native(GUID meshID, GUID textureID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetNormalTexture_Native(GUID meshID, GUID textureID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetScalarMaterialParams_Native(GUID meshID, in Vector4 tintColor, float tilingFactor, float shininess);
    }

}
