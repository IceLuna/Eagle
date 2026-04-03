using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Security.Permissions;

namespace Eagle
{
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
        [UIName("Exponential squared")] Exponential2 = 2,
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

    public enum TextureCompressionQuality
    {
        Disabled,
		Medium, // BC1, BC3
		High,   // BC7
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

    public struct LensSettings
    {
        public bool bEnableChromaticAberration;
        public bool bEnableVignette;
        public bool bEnableFilmGrain;
        public float ChromaticIntensity;
        public float VignetteIntensity;
        public float FilmGrainScale;
        public float FilmGrainAmount;
        public float FilmGrainSeedUpdateRate; // every FilmGrainSeedUpdateRate seconds
    };

    public enum EmitterEmissionShapeType
    {
        Point,
        Sphere,
        [UIName("Sphere Surface")] SphereSurface,
        Box,
        [UIName("Box Surface")] BoxSurface,
        Ring,
        Mesh,
    }

    public enum EmitterCollisionModeType
    {
        None, [UIName("Destroy on Hit")] DestroyOnHit, Bounce,
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
		public float LoopDuration;
        public uint SpawnRate; // How many particles to spawn in a second
		public float RadialAcceleration; // If it's negative, particles will move towards the center of the emitter. If positive, they move away from the center
		public float TangentialAcceleration; // Particles will move away from the center of the emitter in a spiral way.
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
		public bool bFaceDirection; // When set to true, particles will face the velocity direction
    }

    public static class Renderer
    {
        public static void DrawLine(RendererLine line)
        {
            DrawLine_Native(ref line.Start.Color, ref line.End.Color, ref line.Start.Location, ref line.End.Location);
        }

        public static void DrawTriangle(RendererTriangle triangle)
        {
            DrawTriangle_Native(ref triangle.V0.Location, ref triangle.V0.Color,
                ref triangle.V1.Location, ref triangle.V1.Color,
                ref triangle.V2.Location, ref triangle.V2.Color);
        }

        public static void DrawArrow(Vector3 start, Vector3 end, Vector3 up)
        {
            DrawArrow_Native(ref start, ref end, ref up);
        }

        public static void DrawAABB(AABB aabb, Transform worldTransform)
        {
            DrawAABB_Native(ref aabb, ref worldTransform);
        }

        public static void DrawCone(Vector3 location, Rotator rotation, float distance, float angleRad)
        {
            DrawCone_Native(ref location, ref rotation.Rotation, distance, angleRad);
        }

        public const uint CascadesCount = 4u;

        public static void SetFogSettings(FogSettings value)
        {
            SetFogSettings_Native(ref value.Color, value.MinDistance, value.MaxDistance, value.Density, value.Equation, value.bEnabled);
        }

        public static FogSettings GetFogSettings()
        {
            GetFogSettings_Native(out Color3 color, out float minDistance, out float maxDistance, out float density, out FogEquation equation, out bool bEnabled);
            FogSettings settings = new FogSettings();
            settings.Color = color;
            settings.MinDistance = minDistance;
            settings.MaxDistance = maxDistance;
            settings.Density = density;
            settings.Equation = equation;
            settings.bEnabled = bEnabled;
            return settings;
        }

        public static void SetBloomSettings(BloomSettings value)
        {
            SetBloomSettings_Native(value.Dirt != null ? value.Dirt.GetGUID() : GUID.Null(), value.Threshold, value.Intensity, value.DirtIntensity, value.Knee, value.bEnabled);
        }

        public static BloomSettings GetBloomSettings()
        {
            GetBloomSettings_Native(out GUID dirtTexture, out float threashold, out float intensity, out float dirtIntensity, out float knee, out bool bEnabled);
            BloomSettings settings = new BloomSettings();
            settings.Dirt = dirtTexture.IsNull() ? null : new AssetTexture2D(dirtTexture);
            settings.Threshold = threashold;
            settings.Intensity = intensity;
            settings.DirtIntensity = dirtIntensity;
            settings.Knee = knee;
            settings.bEnabled = bEnabled;
            return settings;
        }

        public static void SetSSAOSettings(SSAOSettings value)
        {
            SetSSAOSettings_Native(value.Samples, value.Radius, value.Bias);
        }

        public static SSAOSettings GetSSAOSettings()
        {
            GetSSAOSettings_Native(out uint samples, out float radius, out float bias);
            SSAOSettings settings = new SSAOSettings();
            settings.Samples = samples;
            settings.Radius = radius;
            settings.Bias = bias;
            return settings;
        }

        public static void SetGTAOSettings(GTAOSettings value)
        {
            SetGTAOSettings_Native(value.Samples, value.Radius);
        }

        public static GTAOSettings GetGTAOSettings()
        {
            GetGTAOSettings_Native(out uint samples, out float radius);
            GTAOSettings settings = new GTAOSettings();
            settings.Samples = samples;
            settings.Radius = radius;
            return settings;
        }

        public static void SetPhotoLinearTonemappingSettings(PhotoLinearTonemappingSettings value)
        {
            SetPhotoLinearTonemappingSettings_Native(value.Sensitivity, value.ExposureTime, value.FStop);
        }

        public static PhotoLinearTonemappingSettings GetPhotoLinearTonemappingSettings()
        {
            GetPhotoLinearTonemappingSettings_Native(out float sensitivity, out float exposureTime, out float fstop);
            PhotoLinearTonemappingSettings settings = new PhotoLinearTonemappingSettings();
            settings.Sensitivity = sensitivity;
            settings.ExposureTime = exposureTime;
            settings.FStop = fstop;
            return settings;
        }

        public static void SetFilmicTonemappingSettings(FilmicTonemappingSettings value)
        {
            SetFilmicTonemappingSettings_Native(value.WhitePoint);
        }

        public static FilmicTonemappingSettings GetFilmicTonemappingSettings()
        {
            GetFilmicTonemappingSettings_Native(out float whitePoint);
            FilmicTonemappingSettings settings = new FilmicTonemappingSettings();
            settings.WhitePoint = whitePoint;
            return settings;
        }

        public static void SetCubemap(AssetTextureCube cubemap)
        {
            SetSkybox_Native(cubemap == null ? GUID.Null() : cubemap.GetGUID());
        }

        public static AssetTextureCube GetCubemap()
        {
            GUID id = GetSkybox_Native();
            return id.IsNull() ? null : new AssetTextureCube(id);
        }

        public static void SetCubemapIntensity(float intensity)
        {
            SetCubemapIntensity_Native(intensity);
        }

        public static float GetCubemapIntensity()
        {
            return GetCubemapIntensity_Native();
        }

        public static void SetSkySettings(SkySettings value)
        {
            SetSkySettings_Native(ref value.SunPos, ref value.CloudsColor, value.SkyIntensity, value.CloudsIntensity, value.Scattering, value.Cirrus, value.Cumulus, value.CumulusLayers, value.bEnableCirrusClouds, value.bEnableCumulusClouds);
        }

        public static SkySettings GetSkySettings()
        {
            GetSkySettings_Native(out Vector3 sunPos, out Color3 cloudsColor, out float skyIntensity, out float cloudsIntensity, out float scattering, out float cirrus, out float cumulus, out uint cumulusLayers, out bool bCirrus, out bool bCumulus);

            SkySettings settings = new SkySettings();
            settings.SunPos = sunPos;
            settings.CloudsColor = cloudsColor;
            settings.SkyIntensity = skyIntensity;
            settings.CloudsIntensity = cloudsIntensity;
            settings.Scattering = scattering;
            settings.Cirrus = cirrus;
            settings.Cumulus = cumulus;
            settings.CumulusLayers = cumulusLayers;
            settings.bEnableCirrusClouds = bCirrus;
            settings.bEnableCumulusClouds = bCumulus;
            return settings;
        }

        public static void SetVolumetricLightsSettings(VolumetricLightsSettings value)
        {
            SetVolumetricLightsSettings_Native(ref value.Albedo, value.Anisotropy, value.Samples, value.MaxScatteringDistance, value.FogSpeed, value.bFogEnabled, value.bEnabled);
        }

        public static VolumetricLightsSettings GetVolumetricLightsSettings()
        {
            GetVolumetricLightsSettings_Native(out Vector3 albedo, out float anisotropy, out uint samples, out float maxScatteringDistance, out float fogSpeed, out bool bFogEnabled, out bool bEnabled);
            VolumetricLightsSettings settings = new VolumetricLightsSettings();
            settings.Albedo = albedo;
            settings.Anisotropy = anisotropy;
            settings.Samples = samples;
            settings.MaxScatteringDistance = maxScatteringDistance;
            settings.FogSpeed = fogSpeed;
            settings.bFogEnabled = bFogEnabled;
            settings.bEnabled = bEnabled;
            return settings;
        }

        public static void SetShadowMapsSettings(ShadowMapsSettings value)
        {
            SetShadowMapsSettings_Native(value.PointLightShadowMapSize, value.SpotLightShadowMapSize, value.DirLightShadowMapSizes);
        }

        public static ShadowMapsSettings GetShadowMapsSettings()
        {
            ShadowMapsSettings result = new ShadowMapsSettings();
            result.DirLightShadowMapSizes = GetShadowMapsSettings_Native(out uint pointLightSize, out uint spotLightSize);
            result.PointLightShadowMapSize = pointLightSize;
            result.SpotLightShadowMapSize = spotLightSize;
            return result;
        }

        public static void SetDepthOfFieldSettings(DepthOfFieldSettings value)
        {
            SetDepthOfFieldSettings_Native(ref value.ApertureShape, value.ApertureSize, value.FocalLength, value.COCScale, value.MaxCOC);
        }

        public static DepthOfFieldSettings GetDepthOfFieldSettings()
        {
            DepthOfFieldSettings result = new DepthOfFieldSettings();
            GetDepthOfFieldSettings_Native(out result.ApertureShape, out result.ApertureSize, out result.FocalLength, out result.COCScale, out result.MaxCOC);
            return result;
        }

        public static void SetMotionBlurSettings(MotionBlurSettings value)
        {
            SetMotionBlurSettings_Native(value.bEnabled, value.NumSamples, value.Strength);
        }

        public static MotionBlurSettings GetMotionBlurSettings()
        {
            MotionBlurSettings result = new MotionBlurSettings();
            GetMotionBlurSettings_Native(out result.bEnabled, out result.NumSamples, out result.Strength);
            return result;
        }

        public static void SetAutoExposureSettings(AutoExposureSettings value)
        {
            SetAutoExposureSettings_Native(value.MinLogLum, value.MaxLogLum, value.AdaptationSpeed, value.AdaptationKey, value.bEnabled, value.bHalfResolution);
        }

        public static AutoExposureSettings GetAutoExposureSettings()
        {
            AutoExposureSettings result = new AutoExposureSettings();
            GetAutoExposureSettings_Native(out result.MinLogLum, out result.MaxLogLum, out result.AdaptationSpeed, out result.AdaptationKey, out result.bEnabled, out result.bHalfResolution);
            return result;
        }

        public static void SetScreenSpaceReflectionsSettings(ScreenSpaceReflectionsSettings value)
        {
            SetScreenSpaceReflectionsSettings_Native(value.RoughnessThreshold, value.SamplesPerQuad, value.MaxTraversalIterations, value.bEnabled);
        }

        public static ScreenSpaceReflectionsSettings GetScreenSpaceReflectionsSettings()
        {
            ScreenSpaceReflectionsSettings result = new ScreenSpaceReflectionsSettings();
            GetScreenSpaceReflectionsSettings_Native(out result.RoughnessThreshold, out result.SamplesPerQuad, out result.MaxTraversalIterations, out result.bEnabled);
            return result;
        }

        public static void SetLensSettings(LensSettings value)
        {
            SetLensSettings_Native(value.bEnableChromaticAberration, value.bEnableVignette, value.bEnableFilmGrain, value.ChromaticIntensity, value.VignetteIntensity, value.FilmGrainScale, value.FilmGrainAmount, value.FilmGrainSeedUpdateRate);
        }

        public static LensSettings GetLensSettings()
        {
            LensSettings value = new LensSettings();
            GetLensSettings_Native(out value.bEnableChromaticAberration, out value.bEnableVignette, out value.bEnableFilmGrain, out value.ChromaticIntensity, out value.VignetteIntensity, out value.FilmGrainScale, out value.FilmGrainAmount, out value.FilmGrainSeedUpdateRate);
            return value;
        }

        // Returns active camera transform
        public static Transform GetCameraTransform()
        {
            GetCameraTransform_Native(out Transform result);
            return result;
        }

        public static Vector2 GetViewportSize()
        {
            GetViewportSize_Native(out Vector2 result);
            return result;
        }

        public static bool bUseSkyAsBackground
        {
            set { SetUseSkyAsBackground_Native(value); }
            get { return GetUseSkyAsBackground_Native(); }
        }

        public static bool bRenderSkyboxEnabled
        {
            set { SetRenderSkyboxEnabled_Native(value); }
            get { return IsRenderSkyboxEnabled_Native(); }
        }

        public static bool bSkyboxEnabled
        {
            set { SetSkyboxEnabled_Native(value); }
            get { return IsSkyboxEnabled_Native(); }
        }

        public static float Gamma
        {
            set { SetGamma_Native(value); }
            get { return GetGamma_Native(); }
        }

        public static float Exposure
        {
            set { SetExposure_Native(value); }
            get { return GetExposure_Native(); }
        }
        
        public static float LineWidth
        {
            set { SetLineWidth_Native(value); }
            get { return GetLineWidth_Native(); }
        }

        public static TonemappingMethod Tonemapping
        {
            set { SetTonemappingMethod_Native(value); }
            get { return GetTonemappingMethod_Native(); }
        }

        public static AmbientOcclusion AO
        {
            set { SetAO_Native(value); }
            get { return GetAO_Native(); }
        }

        public static AAMethod AA
        {
            set { SetAAMethod_Native(value); }
            get { return GetAAMethod_Native(); }
        }

        public static bool bVSync
        {
            set { SetVSyncEnabled_Native(value); }
            get { return GetVSyncEnabled_Native(); }
        }

        public static bool bEnableSoftShadows
        {
            set { SetSoftShadowsEnabled_Native(value); }
            get { return GetSoftShadowsEnabled_Native(); }
        }

        public static bool bDepthPrepass
        {
            set { SetDepthPrepassEnabled_Native(value); }
            get { return GetDepthPrepassEnabled_Native(); }
        }

        public static bool bTranslucentShadows
        {
            set { SetTranslucentShadowsEnabled_Native(value); }
            get { return GetTranslucentShadowsEnabled_Native(); }
        }

        public static bool bEnableCSMSmoothTransition
        {
            set { SetCSMSmoothTransitionEnabled_Native(value); }
            get { return GetCSMSmoothTransitionEnabled_Native(); }
        }

        public static bool bVisualizeCascades
        {
            set { SetVisualizeCascades_Native(value); }
            get { return GetVisualizeCascades_Native(); }
        }

        public static bool bStutterlessShaders
        {
            set { SetStutterlessShaders_Native(value); }
            get { return GetStutterlessShaders_Native(); }
        }

        public static bool bEnableObjectPicking
        {
            set { SetObjectPickingEnabled_Native(value); }
            get { return IsObjectPickingEnabled_Native(); }
        }

        public static bool bEnable2DObjectPicking
        {
            set { Set2DObjectPickingEnabled_Native(value); }
            get { return Is2DObjectPickingEnabled_Native(); }
        }

        public static bool bSortOpaqueParticles
        {
            set { SetSortOpaqueParticlesEnabled_Native(value); }
            get { return IsSortOpaqueParticlesEnabled_Native(); }
        }

        public static bool bEnableDebugLinesDepthTest
        {
            set { SetDebugLinesDepthTestEnabled_Native(value); }
            get { return IsDebugLinesDepthTestEnabled_Native(); }
        }

        public static uint TransparencyLayers
        {
            set { SetTransparencyLayers_Native(value); }
            get { return GetTransparencyLayers_Native(); }
        }

        // Native calls
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void DrawLine_Native(ref Color3 startColor, ref Color3 endColor, ref Vector3 start, ref Vector3 end);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void DrawTriangle_Native(ref Vector3 LocationV0, ref Color3 ColorV0, ref Vector3 LocationV1, ref Color3 ColorV1, ref Vector3 LocationV2, ref Color3 ColorV2);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void DrawArrow_Native(ref Vector3 start, ref Vector3 end, ref Vector3 up);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void DrawAABB_Native(ref AABB aabb, ref Transform worldTransform);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void DrawCone_Native(ref Vector3 location, ref Quat rotation, float distance, float angleRad);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetFogSettings_Native(ref Color3 color, float minDistance, float maxDistance, float density, FogEquation equation, bool bEnabled);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void GetFogSettings_Native(out Color3 color, out float minDistance, out float maxDistance, out float density, out FogEquation equation, out bool bEnabled);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetBloomSettings_Native(GUID dirt, float threashold, float intensity, float dirtIntensity, float knee, bool bEnabled);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void GetBloomSettings_Native(out GUID dirtTexture, out float threashold, out float intensity, out float dirtIntensity, out float knee, out bool bEnabled);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetSSAOSettings_Native(uint samples, float radius, float bias);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void GetSSAOSettings_Native(out uint samples, out float radius, out float bias);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetGTAOSettings_Native(uint samples, float radius);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void GetGTAOSettings_Native(out uint samples, out float radius);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetPhotoLinearTonemappingSettings_Native(float sensetivity, float exposureTime, float fStop);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void GetPhotoLinearTonemappingSettings_Native(out float sensetivity, out float exposureTime, out float fStop);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetFilmicTonemappingSettings_Native(float whitePoint);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void GetFilmicTonemappingSettings_Native(out float whitePoint);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern float GetGamma_Native();

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetGamma_Native(float value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern float GetExposure_Native();

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetExposure_Native(float value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern float GetLineWidth_Native();

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetLineWidth_Native(float value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern TonemappingMethod GetTonemappingMethod_Native();

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetTonemappingMethod_Native(TonemappingMethod value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern AAMethod GetAAMethod_Native();

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetAAMethod_Native(AAMethod value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern AmbientOcclusion GetAO_Native();

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetAO_Native(AmbientOcclusion value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetVSyncEnabled_Native(bool value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool GetVSyncEnabled_Native();

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetSoftShadowsEnabled_Native(bool value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool GetSoftShadowsEnabled_Native();

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetDepthPrepassEnabled_Native(bool value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool GetDepthPrepassEnabled_Native();

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetTranslucentShadowsEnabled_Native(bool value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool GetTranslucentShadowsEnabled_Native();

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetUseSkyAsBackground_Native(bool value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool GetUseSkyAsBackground_Native();

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetSkyboxEnabled_Native(bool value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool IsSkyboxEnabled_Native();

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetRenderSkyboxEnabled_Native(bool value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool IsRenderSkyboxEnabled_Native();

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetCSMSmoothTransitionEnabled_Native(bool value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool GetCSMSmoothTransitionEnabled_Native();

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetVisualizeCascades_Native(bool value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool GetVisualizeCascades_Native();

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetStutterlessShaders_Native(bool value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool GetStutterlessShaders_Native();

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetObjectPickingEnabled_Native(bool value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool IsObjectPickingEnabled_Native();

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Set2DObjectPickingEnabled_Native(bool value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool Is2DObjectPickingEnabled_Native();

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetSortOpaqueParticlesEnabled_Native(bool value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool IsSortOpaqueParticlesEnabled_Native();

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetDebugLinesDepthTestEnabled_Native(bool value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool IsDebugLinesDepthTestEnabled_Native();

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetTransparencyLayers_Native(uint value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern uint GetTransparencyLayers_Native();

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void GetSkySettings_Native(out Vector3 sunPos, out Color3 cloudsColor, out float skyIntensity, out float cloudsIntensity, out float scattering, out float cirrus, out float cumulus, out uint cumulusLayers, out bool bCirrus, out bool bCumulus);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetSkySettings_Native(ref Vector3 SunPos, ref Color3 CloudsColor, float skyIntensity, float cloudsIntensity, float scattering, float cirrus, float cumulus, uint cumulusLayers, bool bEnableCirrusClouds, bool bEnableCumulusClouds);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetVolumetricLightsSettings_Native(ref Vector3 albedo, float anisotropy, uint samples, float maxScatteringDist, float fogSpeed, bool bFogEnable, bool bEnable);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void GetVolumetricLightsSettings_Native(out Vector3 albedo, out float anisotropy, out uint samples, out float maxScatteringDist, out float fogSpeed, out bool bFogEnable, out bool bEnable);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern uint[] GetShadowMapsSettings_Native(out uint pointLightSize, out uint spotLightSize);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void GetDepthOfFieldSettings_Native(out Vector2 apertureShape, out float apertureSize, out float focalLength, out float COCScale, out float maxCOC);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void GetMotionBlurSettings_Native(out bool bEnabled, out uint numSamples, out float strength);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void GetAutoExposureSettings_Native(out float minLogLum, out float maxLogLum, out float adaptationSpeed, out float adaptationKey, out bool bEnabled, out bool bHalfResolution);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void GetScreenSpaceReflectionsSettings_Native(out float roughnessThreshold, out uint samplesPerQuad, out uint maxIters, out bool bEnabled);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void GetLensSettings_Native(out bool bChromaticAberration, out bool bVignette, out bool bFilmGrain, out float chromaticIntensity, out float vignetteIntensity, out float filmGrainScale, out float filmGrainAmount, out float filmGrainSeedUpdateRate);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetShadowMapsSettings_Native(uint pointLightSize, uint spotLightSize, uint[] dirLightSizes);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetDepthOfFieldSettings_Native(ref Vector2 apertureShape, float apertureSize, float focalLength, float COCScale, float maxCOC);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetMotionBlurSettings_Native(bool bEnabled, uint numSamples, float strength);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetAutoExposureSettings_Native(float minLogLum, float maxLogLum, float adaptationSpeed, float adaptationKey, bool bEnabled, bool bHalfResolution);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetScreenSpaceReflectionsSettings_Native(float roughnessThreshold, uint samplesPerQuad, uint maxIters, bool bEnabled);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetLensSettings_Native(bool bChromaticAberration, bool bVignette, bool bFilmGrain, float chromaticIntensity, float vignetteIntensity, float filmGrainScale, float filmGrainAmount, float filmGrainSeedUpdateRate);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void GetCameraTransform_Native(out Transform transform);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void GetViewportSize_Native(out Vector2 size);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetSkybox_Native(GUID cubemapID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern GUID GetSkybox_Native();

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetCubemapIntensity_Native(float intensity);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern float GetCubemapIntensity_Native();
    }
}
