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

        public static Texture2D Black
        {
            get
            {
                Texture2D result = new Texture2D();
                result.ID = GetBlackTexture_Native();
                return result;
            }
        }

        public static Texture2D White
        {
            get
            {
                Texture2D result = new Texture2D();
                result.ID = GetWhiteTexture_Native();
                return result;
            }
        }

        public static Texture2D Gray
        {
            get
            {
                Texture2D result = new Texture2D();
                result.ID = GetGrayTexture_Native();
                return result;
            }
        }

        public static Texture2D Red
        {
            get
            {
                Texture2D result = new Texture2D();
                result.ID = GetRedTexture_Native();
                return result;
            }
        }

        public static Texture2D Green
        {
            get
            {
                Texture2D result = new Texture2D();
                result.ID = GetGreenTexture_Native();
                return result;
            }
        }

        public static Texture2D Blue
        {
            get
            {
                Texture2D result = new Texture2D();
                result.ID = GetBlueTexture_Native();
                return result;
            }
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern GUID Create_Native(string filepath);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern GUID GetBlackTexture_Native();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern GUID GetWhiteTexture_Native();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern GUID GetGrayTexture_Native();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern GUID GetRedTexture_Native();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern GUID GetGreenTexture_Native();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern GUID GetBlueTexture_Native();
    }
}
