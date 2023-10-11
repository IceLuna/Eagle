namespace Eagle
{
    public enum MaterialBlendMode
    {
        Opaque, Translucent, Masked
    }

    public class Material
    {
        public Texture2D AlbedoTexture;
        public Texture2D MetallnessTexture;
        public Texture2D NormalTexture;
        public Texture2D RoughnessTexture;
        public Texture2D AOTexture;
        public Texture2D EmissiveTexture;
        public Texture2D OpacityTexture;
        public Texture2D OpacityMaskTexture;
        
        public Color4 TintColor = new Color4();
        public Vector3 EmissiveIntensity = new Vector3();
        public float TilingFactor;
        public MaterialBlendMode BlendMode = MaterialBlendMode.Opaque;
    }
}
