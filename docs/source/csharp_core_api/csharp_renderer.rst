.. _csharp_rendering_guide:

C# Renderer
===========

C# Renderer API allows you to change renderer settings; import textures and meshes; create materials and many more.

To learn more about settings themselves and what they do, read the :ref:`renderer <rendering_guide>` documentation.

Textures
--------
Textures API can be used for importing textures or getting already imported ones.

There are two main classes: ``Texture2D`` and ``TextureCube`` which are derived from ``Texture`` class.
Texture classes have constructors that expect a filepath. Additionally, ``TextureCube`` expects a layer size and it can be created from an existing 2D texture.

``Texture2D`` class also contains static members of ``Texture2D`` type that represent built-in textures: ``Black``, ``White``, ``Gray``, ``Red``, ``Green``, ``Blue``.

.. note::

	Currently, there is no way to unload a texture from memory and it will be unloaded only on the engine exit.

.. note::

	The engine will not import a texture again if a texture with a specified filepath already exists. It will just return an existing one.

.. code-block:: csharp

    public enum FilterMode
    {
        Point,
        Bilinear,
        Trilinear
    }

    public enum AddressMode
    {
        Wrap,
        Mirror,
        Clamp,
        ClampToOpaqueBlack,
        ClampToOpaqueWhite
    }

    abstract public class Texture
    {
        public GUID ID { get; protected set; }
        public bool IsValid() { return IsValid_Native(ID); }
        public string GetPath() { return GetPath_Native(ID); }
        public Vector3 GetSize();
    }

    public class Texture2D : Texture
    {
        public Texture2D() {}
        public Texture2D(GUID guid) { ID = guid; }
        public Texture2D(string filepath);

        // Note that changing these values affects the whole asset, meaning it will affect the editor as well
        public void SetAnisotropy(float anisotropy);
        public void SetFilterMode(FilterMode filterMode);
        public void SetAddressMode(AddressMode addressMode);
        public void SetMipsCount(uint mips); // Minimum is `1`, which means that there's only one mip level - base level (original texture)

        public float GetAnisotropy();
        public FilterMode GetFilterMode();
        public AddressMode GetAddressMode();
        public uint GetMipsCount();

        public static Texture2D Black;
        public static Texture2D White;
        public static Texture2D Gray;
        public static Texture2D Red;
        public static Texture2D Green;
        public static Texture2D Blue;
    }

    public class TextureCube : Texture
    {
        public TextureCube() { }
        public TextureCube(GUID guid) { ID = guid; }
        public TextureCube(string filepath, uint layerSize = 1024u);
        public TextureCube(Texture2D texture, uint layerSize = 1024u);
    }

.. note::

    ``TextureCube`` constructor that takes ``Texture2D`` always creates a cube texture and allocates memory. It doesn't check if it was already created.

`Static Mesh` class
-------------------
It can be used for importing meshes or getting already imported ones.

.. note::

	Currently, there is no way to unload a static mesh from memory and it will be unloaded only on the engine exit.

.. note::

	The engine will not import a mesh again if a mesh with a specified filepath already exists. It will just return an existing one.

.. code-block:: csharp

    public class StaticMesh
    {
        public GUID ID { get; internal set; }

        public StaticMesh() {}
        public StaticMesh(string filepath);

        public bool IsValid();
    }

Material API
------------
This API allows you to create materials that can be assigned to Static Meshes and/or Sprites.

.. code-block:: csharp

    public enum MaterialBlendMode
    {
        Opaque, Translucent, Masked
    }

    public class Material
    {
        public Texture2D AlbedoTexture;
        public Texture2D MetallnessTexture; // Controls how `metal-like` surface looks like. Default is 0.
        public Texture2D NormalTexture;
        public Texture2D RoughnessTexture; // Controls how rough surface looks like. Roughness of 0 is a mirror reflection and 1 is completely matte. Default is 0.5.
        public Texture2D AOTexture; // Can be used to affect how ambient lighting is applied to an object. If it is 0, ambient lighting wont affect it. Default is 1.0.
        public Texture2D EmissiveTexture;
        public Texture2D OpacityTexture; // When in `Translucent` mode, controls the translucency of the material. 0 - fully transparent, 1 - fully opaque. Default is 0.5.
        public Texture2D OpacityMaskTexture; // When in `Masked` mode, a material is either completely visible or completely invisible. Values below 0.5 are invisible.
        
        public Color4 TintColor = new Color4(); // HDR value
        public Vector3 EmissiveIntensity = new Vector3();
        public float TilingFactor; // UV tiling
        public MaterialBlendMode BlendMode = MaterialBlendMode.Opaque;
    }

``MaterialBlendMode`` enum allows you to specify a material type.

.. note::

    Translucent materials do not cast shadows! Use translucent materials with caution because rendering them might be expensive.

.. note::

    `Masked` mode is basically the same as `Opaque` but it additionally allows you to `punch` holes in an object.
    It us more computationally expensive to generate shadows that are cast by `Masked` materials since `Mask` must be taken into account when generating a shadow map.

Renderer API
------------
``Renderer`` class is a static class that can be used for changing renderer settings.

There are special `enums` and `structs` that ``Renderer`` class uses. They are listed below:

.. code-block:: csharp

    public enum FogEquation
    {
        Linear = 0,
        Exponential = 1,
        Exponential2 = 2, // Square exponential
    }

    public enum AmbientOcclusion
    {
        None,
        SSAO,
        GTAO
    }

    public enum TonemappingMethod
    {
        Reinhard,
        Filmic,
        ACES,
        PhotoLinear
    }

    public enum AAMethod
    {
        None,
        TAA
    }

    public struct PhotoLinearTonemappingSettings
    {
        public float Sensitivity;
        public float ExposureTime;
        public float FStop;
    }

    public struct FilmicTonemappingSettings
    {
        public float WhitePoint;
    }

    public struct SkySettings
    {
        public Vector3 SunPos;
        public Color3 CloudsColor;
        public float SkyIntensity;
        public float CloudsIntensity;
        public float Scattering;
        public float Cirrus;
        public float Cumulus;
        public uint CumulusLayers;
        public bool bEnableCirrusClouds;
        public bool bEnableCumulusClouds;
    }

    public struct FogSettings
    {
        public Color3 Color;
        public float MinDistance; // Everything closer won't be affected by the fog. Used by `Linear` equation.
        public float MaxDistance; // Everything after this distance is fog. Used by `Linear` equation.
        public float Density;     // Used by Exponential equations
        public FogEquation Equation;
        public bool bEnabled;
    }

    public struct BloomSettings
    {
        public Texture2D Dirt; // Can be used to create different types of screen effects
        public float Threshold;
        public float Intensity;
        public float DirtIntensity;
        public float Knee;
        public bool bEnabled;
    }

    public struct SSAOSettings
    {
        public uint Samples;
        public float Radius;
        public float Bias;
    }
    
    public struct GTAOSettings
    {
        public uint Samples;
        public float Radius;
    }

    public struct VolumetricLightsSettings
    {
        public uint Samples; // Use with caution! Making it to high might kill the performance. Especially if the light casts shadows.
        public float MaxScatteringDistance;
        public float FogSpeed;
        public bool bFogEnabled;
        public bool bEnabled; // This just notifies the engine that volumetric lights can be used
    }

    public struct ShadowMapsSettings
    {
        public uint PointLightShadowMapSize;
        public uint SpotLightShadowMapSize;
        public uint[] DirLightShadowMapSizes; // It is an array of sizes for each cascade. Currently, it has the size of `4`
    }

``Renderer`` class has the following functionality

.. code-block:: csharp

    public static class Renderer
    {
        public const uint CascadesCount = 4u; // The size of `DirLightShadowMapSizes` array

        public static void SetFogSettings(FogSettings value);
        public static FogSettings GetFogSettings();

        public static void SetBloomSettings(BloomSettings value);
        public static BloomSettings GetBloomSettings();

        public static void SetSSAOSettings(SSAOSettings value);
        public static SSAOSettings GetSSAOSettings();

        public static void SetGTAOSettings(SSAOSettings value);
        public static GTAOSettings GetGTAOSettings();

        public static void SetPhotoLinearTonemappingSettings(PhotoLinearTonemappingSettings value);
        public static PhotoLinearTonemappingSettings GetPhotoLinearTonemappingSettings();

        public static void SetFilmicTonemappingSettings(FilmicTonemappingSettings value);
        public static FilmicTonemappingSettings GetFilmicTonemappingSettings();

        public static void SetCubemap(TextureCube cubemap);
        public static TextureCube GetCubemap();

        public static void SetCubemapIntensity(float intensity);
        public static float GetCubemapIntensity();

        public static void SetSkySettings(SkySettings value);
        public static SkySettings GetSkySettings();

        public static void SetVolumetricLightsSettings(VolumetricLightsSettings value);
        public static VolumetricLightsSettings GetVolumetricLightsSettings();

        public static void SetShadowMapsSettings(ShadowMapsSettings value);
        public static ShadowMapsSettings GetShadowMapsSettings();

        public static Vector2 GetViewportSize();

        public static bool bUseSkyAsBackground; // Allows you to render sky while keeping IBL enabled.
        public static bool bSkyboxEnabled; // Affects Sky and Cubemap (IBL)
        public static float Gamma;
        public static float Exposure;
        public static float LineWidth;
        public static TonemappingMethod Tonemapping;
        public static AmbientOcclusion AO;
        public static AAMethod AA;
        public static bool bVSync;
        public static bool bEnableSoftShadows; // Hard shadows are still filtered using 3x3 PCF filter.
        public static bool bTranslucentShadows;
        public static bool bEnableCSMSmoothTransition;
        public static bool bVisualizeCascades;
        public static bool bStutterlessShaders;
        public static bool bEnableObjectPicking; // You can disable it when it is not needed to improve performance and reduce memory usage.
        public static bool bEnable2DObjectPicking; // If set to true, 2D objects will be ignored. This value is ignored, if `bEnableObjectPicking` is disabled
        public static uint TransparencyLayers;
    }
