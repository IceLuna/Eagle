namespace Eagle
{
    public enum MaterialBlendMode
    {
        Opaque, Translucent, Masked
    }

    public class Material
    {
        public AssetTexture2D AlbedoAsset;
        public AssetTexture2D MetallnessAsset;
        public AssetTexture2D NormalAsset;
        public AssetTexture2D RoughnessAsset;
        public AssetTexture2D AOAsset;
        public AssetTexture2D EmissiveAsset;
        public AssetTexture2D OpacityAsset;
        public AssetTexture2D OpacityMaskAsset;
        
        public Color4 TintColor = new Color4();
        public Vector3 EmissiveIntensity = new Vector3();
        public float TilingFactor = 1f;
        public MaterialBlendMode BlendMode = MaterialBlendMode.Opaque;
    }
}
