namespace Eagle
{
    public class Material
    {
        public Texture DiffuseTexture = new Texture();
        public Texture SpecularTexture = new Texture();
        public Texture NormalTexture = new Texture();
        
        public Vector4 TintColor = new Vector4();
        public float TilingFactor;
        public float Shininess;
    }
}
