.. _csharp_material_guide:

Material API
============

This API represents materials that can be applied to different renderables such as meshes, sprites, text, etc...

.. code-block:: csharp

    public enum MaterialBlendMode
    {
        Opaque, Translucent, Masked
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
        
        public Color4 TintColor = new Color4(1f); // HDR value
        public Color3 EmissiveIntensity = new Color3(1f);
        public float TilingFactor = 1f; // UV tiling
        public MaterialBlendMode BlendMode = MaterialBlendMode.Opaque;
    }

``MaterialBlendMode`` enum allows you to specify a material type.

.. note::

    Translucent materials do not cast shadows! Use translucent materials with caution because rendering them might be expensive.

.. note::

    `Masked` mode is basically the same as `Opaque` but it additionally allows you to `punch` holes in an object.
    It us more computationally expensive to generate shadows that are cast by `Masked` materials since `Mask` must be taken into account when generating a shadow map.
