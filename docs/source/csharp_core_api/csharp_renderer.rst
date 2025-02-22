.. _csharp_rendering_guide:

C# Renderer
===========

C# Renderer API allows you to change renderer settings and draw debug primitives.
To learn more about renderer settings themselves and what they do, read the :ref:`renderer <rendering_guide>` documentation.

``Renderer`` class is a static class that can be used for changing renderer settings.
There are special `enums` and `structs` that ``Renderer`` class uses. They are listed below:

.. code-block:: csharp

    public struct RendererVertex
    {
        public Vector3 Location;
        public Color3 Color;
    }

    public struct RendererLine
    {
        public RendererVertex Start;
        public RendererVertex End;
    }

    public struct RendererTriangle
    {
        public RendererVertex V0, V1, V2;
    }

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
    };

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
    };

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
        public float MinDistance;
        public float MaxDistance;
        public float Density;
        public FogEquation Equation;
        public bool bEnabled;
    }

    public struct BloomSettings
    {
        public AssetTexture2D Dirt;
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
        public Vector3 Albedo;
        public float Anisotropy;
        public uint Samples;
        public float MaxScatteringDistance;
        public float FogSpeed;
        public bool bFogEnabled;
        public bool bEnabled;
    }

    public struct ShadowMapsSettings
    {
        public uint PointLightShadowMapSize;
        public uint SpotLightShadowMapSize;
        public uint[] DirLightShadowMapSizes;

        public const uint MinPointLightShadowMapSize = 64u;
        public const uint MinSpotLightShadowMapSize = 64u;
        public const uint MinDirLightShadowMapSize = 64u;
    }

    public struct DepthOfFieldSettings
    {
        public Vector2 ApertureShape;
        public float ApertureSize;
        public float FocalLength;
        public float COCScale;
        public float MaxCOC;
    }

    public struct MotionBlurSettings
    {
        public bool bEnabled;
        public uint NumSamples;
        public float Strength;
    }

    public struct AutoExposureSettings
    {
        public float MinLogLum;
        public float MaxLogLum;
        public float AdaptationSpeed;
        public float AdaptationKey;

        public bool bEnabled;
        public bool bHalfResolution;
    }

    public struct ScreenSpaceReflectionsSettings
    {
        public float RoughnessThreshold;
        public uint SamplesPerQuad;
        public uint MaxTraversalIterations;
        public bool bEnabled;
    }

    public enum EmitterEmissionShapeType
    {
        Point, Sphere, SphereSurface, Box, Ring, Mesh
    }

    public enum EmitterCollisionModeType
    {
        None, DestroyOnHit, Bounce,
	}

    public struct ParticleEmitter
	{
		// ---------------- Particle properties ----------------
		public AssetTexture2D TextureAsset;

		public Color4 ColorStart;
        public Color4 ColorEnd;
		
		public Vector3 VelocityMin;
        public Vector3 VelocityMax;

		public Vector3 VelocityCoefStart;
        public Vector3 VelocityCoefEnd;

		public float RotationZStart;
        public float RotationZEnd;

		public Vector2 SizeStart;
		public Vector2 SizeEnd;
        public Vector2 ColliderSizeRatio; // Can be used to increase the size of a collider to prevent small and fast-moving particles from clipping through

		// In seconds
		public float LifetimeMin;
        public float LifetimeMax;

		public float BouncinessMin;
        public float BouncinessMax;

		// ---------------- Emitter properties ----------------
		public string Name;
		public Transform RelativeTransform; // Relative to the particle system
		public AABB VisibilityAABB; // If not visible by the camera, it's not rendered to improve perf
		public uint LoopCount; // 0 - infinity
		public uint NumParticles;
		public float NumParticlesRatio; // Can be used to control `NumParticles`
		public float RadialAcceleration; // If it's negative, particles will move towards the center of the emitter. If positive, they move away from the center
		public float TangentialAcceleration; // If it's negative, particles will move towards the center of the emitter in a spiral way. If positive, they move away from the center.
        public float NormalVelocityFactor; // If not 0, particle's initial velocity will be affected by `EmissionShapeType` normal direction

        public EmitterEmissionShapeType EmissionShape;
        // Sphere emission shape
        public Vector3 SphereRadius;
        // Box emission shape
        public Vector3 BoxMin;
        public Vector3 BoxMax;
        // Ring emission shape
        public Vector3 RingRadius;
        public Vector3 RingThickness;
        // Mesh emission shape
        public AssetStaticMesh MeshAsset;

        public EmitterCollisionModeType CollisionMode;

        // Animation
        public UVector2 AnimationImagesNum; // Horizontal & Vertical images count
        public float AnimationSpeed;

		public bool bDestroyImmediately; // If set to true, particles will be destroyed immediately when emitter is disabled/destroyed (instead of following their lifetime)
		public bool bEmit;
		public bool bExplode; // If set to true, all particles will be emitted at once. Otherwise, they're emitted sequentially throughout the lifetime
		public bool bApplyGravity;
		public bool bAlphaBlending;
		public bool bAdditive;
        public bool bBlendAnimation;
	}

``Renderer`` class has the following functionality

.. code-block:: csharp

    public static class Renderer
    {
        public const uint CascadesCount = 4u; // The size of `DirLightShadowMapSizes` array

        public static void DrawLine(RendererLine line);
        public static void DrawTriangle(RendererTriangle triangle);
        public static void DrawAABB(AABB aabb, Transform worldTransform);

        public static void SetFogSettings(FogSettings value);
        public static FogSettings GetFogSettings();

        public static void SetBloomSettings(BloomSettings value);
        public static BloomSettings GetBloomSettings();

        public static void SetSSAOSettings(SSAOSettings value);
        public static SSAOSettings GetSSAOSettings();

        public static void SetGTAOSettings(GTAOSettings value);
        public static GTAOSettings GetGTAOSettings();

        public static void SetPhotoLinearTonemappingSettings(PhotoLinearTonemappingSettings value);
        public static PhotoLinearTonemappingSettings GetPhotoLinearTonemappingSettings();

        public static void SetFilmicTonemappingSettings(FilmicTonemappingSettings value);
        public static FilmicTonemappingSettings GetFilmicTonemappingSettings();

        public static void SetCubemap(AssetTextureCube cubemap);
        public static AssetTextureCube GetCubemap();

        public static void SetCubemapIntensity(float intensity);
        public static float GetCubemapIntensity();

        public static void SetSkySettings(SkySettings value);
        public static SkySettings GetSkySettings();

        public static void SetVolumetricLightsSettings(VolumetricLightsSettings value);
        public static VolumetricLightsSettings GetVolumetricLightsSettings();

        public static void SetShadowMapsSettings(ShadowMapsSettings value);
        public static ShadowMapsSettings GetShadowMapsSettings();

        public static void SetDepthOfFieldSettings(DepthOfFieldSettings value);
        public static DepthOfFieldSettings GetDepthOfFieldSettings();

        public static void SetMotionBlurSettings(MotionBlurSettings value);
        public static MotionBlurSettings GetMotionBlurSettings();

        public static void SetAutoExposureSettings(AutoExposureSettings value);
        public static AutoExposureSettings GetAutoExposureSettings();

        public static void SetScreenSpaceReflectionsSettings(ScreenSpaceReflectionsSettings value);
        public static ScreenSpaceReflectionsSettings GetScreenSpaceReflectionsSettings();

        public static Vector2 GetViewportSize();

        public static bool bUseSkyAsBackground; // Allows you to render sky while keeping IBL enabled.
        public static bool bRenderSkyboxEnabled; // Allows you to disable just the skybox rendering (If disabled, IBL will still light the scene)
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
        public static bool bSortOpaqueParticles;
        public static bool bEnableDebugLinesDepthTest;
        public static uint TransparencyLayers;
    }
