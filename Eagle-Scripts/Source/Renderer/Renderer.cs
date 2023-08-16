using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

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
        public float Sensetivity;
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
        public bool bEnable;
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
        public bool bEnable;
    }

public static class Renderer
    {
        public static FogSettings Fog
        {
            set { SetFogSettings_Native(ref value.Color, value.MinDistance, value.MaxDistance, value.Density, value.Equation, value.bEnabled); }
            get
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
        }

        public static BloomSettings Bloom
        {
            set { SetBloomSettings_Native(value.Dirt.ID, value.Threshold, value.Intensity, value.DirtIntensity, value.Knee, value.bEnable); }
            get
            {
                GetBloomSettings_Native(out GUID dirtTexture, out float threashold, out float intensity, out float dirtIntensity, out float knee, out bool bEnable);
                BloomSettings settings = new BloomSettings();
                settings.Dirt.ID = dirtTexture;
                settings.Threshold = threashold;
                settings.Intensity = intensity;
                settings.DirtIntensity = dirtIntensity;
                settings.Knee = knee;
                settings.bEnable = bEnable;
                return settings;
            }
        }

        public static SSAOSettings SSAO
        {
            set { SetSSAOSettings_Native(value.Samples, value.Radius, value.Bias); }
            get
            {
                GetSSAOSettings_Native(out uint samples, out float radius, out float bias);
                SSAOSettings settings = new SSAOSettings();
                settings.Samples = samples;
                settings.Radius = radius;
                settings.Bias = bias;
                return settings;
            }
        }

        public static GTAOSettings GTAO
        {
            set { SetGTAOSettings_Native(value.Samples, value.Radius); }
            get
            {
                GetGTAOSettings_Native(out uint samples, out float radius);
                GTAOSettings settings = new GTAOSettings();
                settings.Samples = samples;
                settings.Radius = radius;
                return settings;
            }
        }

        public static PhotoLinearTonemappingSettings PhotoLinearTonemapping
        {
            set { SetPhotoLinearTonemappingSettings_Native(value.Sensetivity, value.ExposureTime, value.FStop); }
            get
            {
                GetPhotoLinearTonemappingSettings_Native(out float sensetivity, out float exposureTime, out float fstop);
                PhotoLinearTonemappingSettings settings = new PhotoLinearTonemappingSettings();
                settings.Sensetivity = sensetivity;
                settings.ExposureTime = exposureTime;
                settings.FStop = fstop;
                return settings;
            }
        }

        public static FilmicTonemappingSettings FilmicTonemapping
        {
            set { SetFilmicTonemappingSettings_Native(value.WhitePoint); }
            get
            {
                GetFilmicTonemappingSettings_Native(out float whitePoint);
                FilmicTonemappingSettings settings = new FilmicTonemappingSettings();
                settings.WhitePoint = whitePoint;
                return settings;
            }
        }

        public static SkySettings Sky
        {
            set { SetSkySettings_Native(ref value.SunPos, ref value.CloudsColor, value.SkyIntensity, value.CloudsIntensity, value.Scattering, value.Cirrus, value.Cumulus, value.CumulusLayers, value.bEnableCirrusClouds, value.bEnableCumulusClouds); }
            get
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
                settings.CumulusLayers= cumulusLayers;
                settings.bEnableCirrusClouds = bCirrus;
                settings.bEnableCumulusClouds = bCumulus;
                return settings;
            }
        }

        public static VolumetricLightsSettings VolumetricLights
        {
            set { SetVolumetricLightsSettings_Native(value.Samples, value.MaxScatteringDistance, value.bEnable); }
            get
            {
                GetVolumetricLightsSettings_Native(out uint samples, out float maxScatteringDistance, out bool bEnable);
                VolumetricLightsSettings settings = new VolumetricLightsSettings();
                settings.Samples = samples;
                settings.MaxScatteringDistance = maxScatteringDistance;
                settings.bEnable = bEnable;
                return settings;
            }
        }

        public static bool bUseSkyAsBackground
        {
            set { SetUseSkyAsBackground_Native(value); }
            get { return GetUseSkyAsBackground_Native(); }
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

        public static bool bEnableSoftShadows
        {
            set { SetSoftShadowsEnabled_Native(value); }
            get { return GetSoftShadowsEnabled_Native(); }
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
        private static extern void SetSoftShadowsEnabled_Native(bool value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool GetSoftShadowsEnabled_Native();

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetUseSkyAsBackground_Native(bool value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool GetUseSkyAsBackground_Native();

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetCSMSmoothTransitionEnabled_Native(bool value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool GetCSMSmoothTransitionEnabled_Native();

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetVisualizeCascades_Native(bool value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool GetVisualizeCascades_Native();

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetTransparencyLayers_Native(uint value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern uint GetTransparencyLayers_Native();

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void GetSkySettings_Native(out Vector3 sunPos, out Color3 cloudsColor, out float skyIntensity, out float cloudsIntensity, out float scattering, out float cirrus, out float cumulus, out uint cumulusLayers, out bool bCirrus, out bool bCumulus);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetSkySettings_Native(ref Vector3 SunPos, ref Color3 CloudsColor, float skyIntensity, float cloudsIntensity, float scattering, float cirrus, float cumulus, uint cumulusLayers, bool bEnableCirrusClouds, bool bEnableCumulusClouds);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetVolumetricLightsSettings_Native(uint samples, float maxScatteringDist, bool bEnable);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void GetVolumetricLightsSettings_Native(out uint samples, out float maxScatteringDist, out bool bEnable);
    }
}
