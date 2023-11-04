using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Security.Permissions;

namespace Eagle
{
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
        None,
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
        public Texture2D Dirt;
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
    }

    public static class Renderer
    {
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
            SetBloomSettings_Native(value.Dirt.ID, value.Threshold, value.Intensity, value.DirtIntensity, value.Knee, value.bEnabled);
        }

        public static BloomSettings GetBloomSettings()
        {
            GetBloomSettings_Native(out GUID dirtTexture, out float threashold, out float intensity, out float dirtIntensity, out float knee, out bool bEnabled);
            BloomSettings settings = new BloomSettings();
            settings.Dirt = new Texture2D(dirtTexture);
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

        public static void SetGTAOSettings(SSAOSettings value)
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

        public static void SetCubemap(TextureCube cubemap)
        {
            SetSkybox_Native(cubemap.ID);
        }

        public static TextureCube GetCubemap()
        {
            GUID id = GetSkybox_Native();
            return new TextureCube(id);
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
            SetVolumetricLightsSettings_Native(value.Samples, value.MaxScatteringDistance, value.FogSpeed, value.bFogEnabled, value.bEnabled);
        }

        public static VolumetricLightsSettings GetVolumetricLightsSettings()
        {
            GetVolumetricLightsSettings_Native(out uint samples, out float maxScatteringDistance, out float fogSpeed, out bool bFogEnabled, out bool bEnabled);
            VolumetricLightsSettings settings = new VolumetricLightsSettings();
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

        public static uint TransparencyLayers
        {
            set { SetTransparencyLayers_Native(value); }
            get { return GetTransparencyLayers_Native(); }
        }

        // Native calls
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
        private static extern void SetTransparencyLayers_Native(uint value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern uint GetTransparencyLayers_Native();

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void GetSkySettings_Native(out Vector3 sunPos, out Color3 cloudsColor, out float skyIntensity, out float cloudsIntensity, out float scattering, out float cirrus, out float cumulus, out uint cumulusLayers, out bool bCirrus, out bool bCumulus);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetSkySettings_Native(ref Vector3 SunPos, ref Color3 CloudsColor, float skyIntensity, float cloudsIntensity, float scattering, float cirrus, float cumulus, uint cumulusLayers, bool bEnableCirrusClouds, bool bEnableCumulusClouds);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetVolumetricLightsSettings_Native(uint samples, float maxScatteringDist, float fogSpeed, bool bFogEnable, bool bEnable);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void GetVolumetricLightsSettings_Native(out uint samples, out float maxScatteringDist, out float fogSpeed, out bool bFogEnable, out bool bEnable);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern uint[] GetShadowMapsSettings_Native(out uint pointLightSize, out uint spotLightSize);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetShadowMapsSettings_Native(uint pointLightSize, uint spotLightSize, uint[] dirLightSizes);

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
