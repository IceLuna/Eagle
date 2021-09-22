using System.Runtime.CompilerServices;

namespace Eagle
{
    public class Texture
    {
        public GUID ID;
        public bool IsValid() { return IsValid_Native(ID); }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool IsValid_Native(in GUID id);
    }

    public class Texture2D : Texture
    {
        public Texture2D() {}
        public Texture2D(string filepath)
        {
            ID = Create_Native(filepath);
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern GUID Create_Native(string filepath);
    }
}
