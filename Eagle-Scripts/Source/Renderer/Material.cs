namespace Eagle
{
    public class Material
    {
        public Texture AlbedoTexture = new Texture();
        public Texture MetallnessTexture = new Texture();
        public Texture NormalTexture = new Texture();
        public Texture RoughnessTexture = new Texture();
        public Texture AOTexture = new Texture();
        
        public Vector4 TintColor = new Vector4();
        public float TilingFactor;
        public float Shininess;
    }
}
