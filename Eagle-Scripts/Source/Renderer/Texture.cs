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

    abstract public class Texture
    {
        public GUID ID { get; protected set; }

        public bool IsValid() { return IsValid_Native(ID); }

        public string GetPath() { return GetPath_Native(ID); }

        public Vector3 GetSize()
        {
            GetSize_Native(ID, out Vector3 result);
            return result;
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool IsValid_Native(in GUID id);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern string GetPath_Native(in GUID id);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetSize_Native(in GUID id, out Vector3 size);
    }

    public class Texture2D : Texture
    {
        public Texture2D() {}
        public Texture2D(GUID guid) { ID = guid; }
        public Texture2D(string filepath)
        {
            ID = Create_Native(filepath);
        }

        // Note that changing these values affects the whole asset, meaning it will affect the editor
        public void SetAnisotropy(float anisotropy) { SetAnisotropy_Native(ID, anisotropy); }

        public void SetFilterMode(FilterMode filterMode) { SetFilterMode_Native(ID, filterMode); }

        public void SetAddressMode(AddressMode addressMode) { SetAddressMode_Native(ID, addressMode); }

        public void SetMipsCount(uint mips) { SetMipsCount_Native(ID, mips); }

        public float GetAnisotropy() { return GetAnisotropy_Native(ID); }

        public FilterMode GetFilterMode() { return GetFilterMode_Native(ID); }

        public AddressMode GetAddressMode() { return GetAddressMode_Native(ID); }

        public uint GetMipsCount() { return GetMipsCount_Native(ID); }

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

    public class TextureCube : Texture
    {
        public TextureCube() { }
        public TextureCube(GUID guid) { ID = guid; }

        public TextureCube(string filepath, uint layerSize = 1024u)
        {
            ID = Create_Native(filepath, layerSize);
        }

        public TextureCube(Texture2D texture, uint layerSize = 1024u)
        {
            ID = CreateFromTexture2D_Native(texture.ID, layerSize);
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern GUID Create_Native(string filepath, uint layerSize);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern GUID CreateFromTexture2D_Native(GUID texture2D, uint layerSize);
    }
}
