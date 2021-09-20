using System.Runtime.CompilerServices;

namespace Eagle
{
    public class StaticMesh
    {
        protected Material m_Material = new Material();
        protected bool m_Valid = false;
        protected string m_Filepath = "";

        StaticMesh()
        { }

        StaticMesh(in string filepath)
        {
            m_Valid = Create_Native(filepath);
            m_Filepath = filepath;
        }

        public bool IsValid() => m_Valid;
        public ref string GetPath() => ref m_Filepath;

        public ref Material GetMaterial() { return ref m_Material; }
        public void SetMaterial(ref Material material)
        {
            m_Material = material;
            if (m_Material.DiffuseTexture.IsValid())
                SetDiffuseTexture_Native(in m_Filepath, in m_Material.DiffuseTexture.GetPath());
            if (m_Material.SpecularTexture.IsValid())
                SetSpecularTexture_Native(in m_Filepath, in m_Material.SpecularTexture.GetPath());
            if (m_Material.NormalTexture.IsValid())
                SetNormalTexture_Native(in m_Filepath, in m_Material.NormalTexture.GetPath());
            SetScalarMaterialParams_Native(in m_Filepath, in m_Material.TintColor, m_Material.TilingFactor, m_Material.Shininess);
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool Create_Native(in string filepath);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetDiffuseTexture_Native(in string meshPath, in string texturePath);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetSpecularTexture_Native(in string meshPath, in string texturePath);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetNormalTexture_Native(in string meshPath, in string texturePath);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetScalarMaterialParams_Native(in string meshPath, in Vector4 tintColor, float tilingFactor, float shininess);
    }

}
