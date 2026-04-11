namespace Eagle
{
    public enum MaterialBlendMode
    {
        Opaque, Translucent, Masked
    }

    public enum TextureChannel
    {
        R = 0, G = 1, B = 2, A = 3
    }

    public class Material
    {
        public AssetTexture2D AlbedoAsset;
        public AssetTexture2D MetalnessAsset;
        public AssetTexture2D NormalAsset;
        public AssetTexture2D RoughnessAsset;
        public AssetTexture2D AOAsset;
        public AssetTexture2D EmissiveAsset;
        public AssetTexture2D OpacityAsset;
        public AssetTexture2D OpacityMaskAsset;

        public TextureChannel MetalnessTextureChannel = TextureChannel.R;
        public TextureChannel RoughnessTextureChannel = TextureChannel.R;
        public TextureChannel AOTextureChannel = TextureChannel.R;
        public TextureChannel OpacityTextureChannel = TextureChannel.R;
        public TextureChannel OpacityMaskTextureChannel = TextureChannel.R;

        public Color3 Albedo = new Color3(0.0f);
        public float Metalness = 0.0f;
        public float Roughness = 0.5f;
        public float AO = 1.0f;
        public Color3 Emissive = new Color3(0.0f);
        public float Opacity = 0.5f;
        public float OpacityMask = 1.0f;

        public bool bUseAlbedoTexture = false;
        public bool bUseMetalnessTexture = false;
        public bool bUseRoughnessTexture = false;
        public bool bUseAOTexture = false;
        public bool bUseEmissiveTexture = false;
        public bool bUseOpacityTexture = false;
        public bool bUseOpacityMaskTexture = false;
        
        public Color4 TintColor = new Color4(1f);
        public Color3 EmissiveIntensity = new Color3(1f);
        public float TilingFactor = 1f;
        public MaterialBlendMode BlendMode = MaterialBlendMode.Opaque;
        public bool bDoubleSided = false;
    }
}
