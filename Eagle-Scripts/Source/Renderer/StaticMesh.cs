using System.Runtime.CompilerServices;

namespace Eagle
{
    public class StaticMesh
    {
        public GUID ID { get; internal set; }

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

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern GUID Create_Native(string filepath);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool IsValid_Native(in GUID guid);
    }
}
