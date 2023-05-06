namespace Eagle
{
    public class Material
    {
        public Texture AlbedoTexture = new Texture();
        public Texture MetallnessTexture = new Texture();
        public Texture NormalTexture = new Texture();
        public Texture RoughnessTexture = new Texture();
        public Texture AOTexture = new Texture();
        public Texture EmissiveTexture = new Texture();
        
        public Color4 TintColor = new Color4();
        public Vector3 EmissiveIntensity = new Vector3();
        public float TilingFactor;
    }
}
