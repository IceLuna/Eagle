using System.Runtime.CompilerServices;

namespace Eagle
{
    public class Texture
    {
        protected string m_Filepath = "";
        protected bool m_Valid = false;

        public bool IsValid() { return m_Valid; }
        public ref string GetPath() { return ref m_Filepath; }
    }

    public class Texture2D : Texture
    {
        public Texture2D() {}
        public Texture2D(in string filepath)
        {
            m_Valid = Create_Native(filepath);
            m_Filepath = filepath;
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool Create_Native(in string filepath);
    }
}
