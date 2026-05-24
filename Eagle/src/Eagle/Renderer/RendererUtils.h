#pragma once

#include "Eagle/Core/Core.h"

#include <glm/glm.hpp>
#include <array>
#include <magic_enum.hpp>

// If changed, vertex shaders for skeletal meshes should also be updated
#define EG_MAX_BONES_PER_VERTEX 4

namespace Eagle
{
    using Index = uint32_t;
    constexpr uint32_t s_JitterSize = 16u;

    enum class MaterialBlendMode
    {
        Opaque, Translucent, Masked
    };
    // It's outside instead of being a part of `MaterialBlendMode` enum is to avoid it showing up in UI.
    // TODO: Fix it when reflection is introduced
    static constexpr uint32_t s_MaxBlendModes = (uint32_t)magic_enum::enum_count<MaterialBlendMode>();

    enum class ShaderType
    {
        Vertex,
        Fragment,
        Geometry,
        Compute
    };

	enum class MemoryType
    {
        Gpu,
        Cpu,
        CpuToGpu,
        GpuToCpu
    };

    enum class ImageFormat
    {
        Unknown,
        R32G32B32A32_Float,
        R32G32B32A32_UInt,
        R32G32B32A32_SInt,
        R32G32B32_Float,
        R32G32B32_UInt,
        R32G32B32_SInt,
        R16G16B16A16_Float,
        R16G16B16A16_UNorm,
        R16G16B16A16_UInt,
        R16G16B16A16_SNorm,
        R16G16B16A16_SInt,
        R32G32_Float,
        R32G32_UInt,
        R32G32_SInt,
        D32_Float_S8X24_UInt,
        R10G10B10A2_UNorm,
        R10G10B10A2_UInt,
        R11G11B10_Float,
        R8G8B8A8_UNorm,
        R8G8B8A8_UNorm_SRGB,
        R8G8B8A8_UInt,
        R8G8B8A8_SNorm,
        R8G8B8A8_SInt,
        R8G8B8_UNorm,
        R8G8B8_UNorm_SRGB,
        R8G8B8_UInt,
        R8G8B8_SNorm,
        R8G8B8_SInt,
        R16G16_Float,
        R16G16_UNorm,
        R16G16_UInt,
        R16G16_SNorm,
        R16G16_SInt,
        D32_Float,
        R32_Float,
        R32_UInt,
        R32_SInt,
        D24_UNorm_S8_UInt,
        R8G8_UNorm,
        R8G8_UNorm_SRGB,
        R8G8_UInt,
        R8G8_SNorm,
        R8G8_SInt,
        R16_Float,
        D16_UNorm,
        R16_UNorm,
        R16_UInt,
        R16_SNorm,
        R16_SInt,
        R8_UNorm_SRGB,
        R8_UNorm,
        R8_UInt,
        R8_SNorm,
        R8_SInt,
        R9G9B9E5_SharedExp,
        R8G8_B8G8_UNorm,
        G8R8_G8B8_UNorm,
        BC1_RGBA_UNorm,
        BC1_RGB_UNorm,
        BC1_UNorm_SRGB,
        BC2_UNorm,
        BC2_UNorm_SRGB,
        BC3_UNorm,
        BC3_UNorm_SRGB,
        BC4_UNorm,
        BC4_SNorm,
        BC5_UNorm,
        BC5_SNorm,
        B5G6R5_UNorm,
        B5G5R5A1_UNorm,
        B8G8R8A8_UNorm,
        B8G8R8A8_UNorm_SRGB,
        BC6H_UFloat16,
        BC6H_SFloat16,
        BC7_UNorm,
        BC7_UNorm_SRGB,
        ETC2_RGB_UNorm,
        ETC2_RGBA_UNorm,
    };

    inline constexpr bool IsSRGBFormat(ImageFormat format)
    {
        switch (format)
        {
            case ImageFormat::R8G8B8A8_UNorm_SRGB:
            case ImageFormat::R8G8B8_UNorm_SRGB:
            case ImageFormat::R8G8_UNorm_SRGB:
            case ImageFormat::R8_UNorm_SRGB:
            case ImageFormat::BC1_UNorm_SRGB:
            case ImageFormat::BC2_UNorm_SRGB:
            case ImageFormat::BC3_UNorm_SRGB:
            case ImageFormat::B8G8R8A8_UNorm_SRGB:
            case ImageFormat::BC7_UNorm_SRGB: return true;
            default: return false;
        }
    }

    enum class ImageUsage
    {
        None                    = 0,
        TransferSrc             = 1 << 0,
        TransferDst             = 1 << 1,
        Sampled                 = 1 << 2,
        Storage                 = 1 << 3,
        ColorAttachment         = 1 << 4,
        DepthStencilAttachment  = 1 << 5,
        TransientAttachment     = 1 << 6,
        InputAttachment         = 1 << 7
    };
    DECLARE_FLAGS(ImageUsage);

    enum class ImageLayoutType
    {
        Unknown,
        ReadOnly,
        CopyDest,
        RenderTarget,
        StorageImage,
        DepthStencilWrite,
        Present
    };

    enum class ImageReadAccess
    {
        None               = 0,
        CopySource         = 1 << 0,
        DepthStencilRead   = 1 << 1,
        PixelShaderRead    = 1 << 2,
        NonPixelShaderRead = 1 << 3
    };
    DECLARE_FLAGS(ImageReadAccess);

    enum class ImageType
    {
        Type1D,
        Type2D,
        Type3D
    };

    enum class SamplesCount
    {
        Samples1  = 1,
        Samples2  = 2,
        Samples4  = 4,
        Samples8  = 8,
        Samples16 = 16,
        Samples32 = 32,
        Samples64 = 64
    };

    struct ImageLayout
    {
        ImageLayoutType LayoutType = ImageLayoutType::Unknown;
        ImageReadAccess ReadAccessFlags = ImageReadAccess::None;

        ImageLayout() = default;
        ImageLayout(ImageLayoutType type) : LayoutType(type) {}
        ImageLayout(ImageReadAccess readAccessFlags) : LayoutType(ImageLayoutType::ReadOnly), ReadAccessFlags(readAccessFlags) {}

        bool operator==(const ImageLayout& other) const { return LayoutType == other.LayoutType && ReadAccessFlags == other.ReadAccessFlags; }
        bool operator!=(const ImageLayout& other) const { return !(*this == other); }
    };

    struct ImageView
    {
        // MipLevel Index of start mip level. Base mip level is 0.
        // MipLevelsCount - Number of mip levels starting from start_mip.

        uint32_t MipLevel = 0;
        uint32_t MipLevels = 1;
        uint32_t Layer = 0;
        uint32_t LayersCount = (~0u);

        bool operator== (const ImageView& other) const noexcept
        {
            return MipLevel == other.MipLevel && Layer == other.Layer && MipLevels == other.MipLevels;
        }
        bool operator!= (const ImageView& other) const noexcept
        {
            return !((*this) == other);
        }
    };

    enum class BufferUsage
    {
        None                            = 0,
        TransferSrc                     = 1 << 0,
        TransferDst                     = 1 << 1,
        UniformTexelBuffer              = 1 << 2,
        StorageTexelBuffer              = 1 << 3,
        UniformBuffer                   = 1 << 4,
        StorageBuffer                   = 1 << 5,
        IndexBuffer                     = 1 << 6,
        VertexBuffer                    = 1 << 7,
        IndirectBuffer                  = 1 << 8,
        RayTracing                      = 1 << 9,
        AccelerationStructure           = 1 << 10,
        AccelerationStructureBuildInput = 1 << 11
    };
    DECLARE_FLAGS(BufferUsage);

    enum class BufferLayoutType
    {
        Unknown,
        ReadOnly,
        CopyDest,
        StorageBuffer,
        AccelerationStructure ///< For DXR only.
    };

    enum class BufferReadAccess
    {
        None                = 0,
        CopySource          = 1 << 0,
        Vertex              = 1 << 1,
        Index               = 1 << 2,
        Uniform             = 1 << 3,
        IndirectArgument    = 1 << 4,
        PixelShaderRead     = 1 << 5,
        NonPixelShaderRead  = 1 << 6
    };
    DECLARE_FLAGS(BufferReadAccess);

    struct BufferLayout
    {
        BufferLayoutType LayoutType;
        BufferReadAccess ReadAccessFlags;

        BufferLayout() : LayoutType(BufferLayoutType::Unknown), ReadAccessFlags(BufferReadAccess::None) {}
        BufferLayout(BufferLayoutType layoutType) : LayoutType(layoutType), ReadAccessFlags(BufferReadAccess::None) {}
        BufferLayout(BufferReadAccess readAccessFlags) : LayoutType(BufferLayoutType::ReadOnly), ReadAccessFlags(readAccessFlags) {}

        bool operator== (const BufferLayout& other) const
        {
            return LayoutType == other.LayoutType && ReadAccessFlags == other.ReadAccessFlags;
        }

        bool operator!= (const BufferLayout& other) const
        {
            return !(*this == other);
        }
    };

    struct RenderStats
    {
        uint64_t DrawCalls = 0;
        uint64_t Dispatches = 0;
    };

    struct DispatchIndirectArgs
    {
        glm::uvec4 ThreadGroupCount = glm::uvec4(0); // It's `uvec4` because of padding issues on GPU side
    };

    struct DrawIndirectArgs
    {
        uint32_t VertexCount = 0;
        uint32_t InstanceCount = 0;
        uint32_t FirstVertex = 0;
        uint32_t FirstInstance = 0;
    };

    struct DrawIndexedIndirectCommand
    {
        uint32_t IndexCount = 0;
        uint32_t InstanceCount = 0;
        uint32_t FirstIndex = 0;
        int32_t  VertexOffset = 0;
        uint32_t FirstInstance = 0;
    };

    struct CullingFrustum
    {
        float NearRight = 0;
        float NearTop = 0;
        float NearPlane = 0;
        float FarPlane = 0;
    };

    static CullingFrustum CalculateFrustum(float nearPlane, float farPlane, float fovY, float aspectRatio)
    {
        const float tanFov = std::tan(0.5f * fovY);
        return CullingFrustum
            {
                aspectRatio * nearPlane * tanFov,
                nearPlane * tanFov,
                -nearPlane,
                -farPlane,
            };
    }

    struct CullingFrustumData
    {
        CullingFrustum Frustum;
        glm::mat4 View = glm::mat4(1);
        glm::mat4 Proj = glm::mat4(1);
        glm::mat4 InvProj = glm::mat4(1);
        glm::vec3 Position = glm::vec3(0);
    };

    struct PostprocessTileStatistics
    {
        DispatchIndirectArgs EarlyExit;
        DispatchIndirectArgs Cheap;
        DispatchIndirectArgs Expensive;
    };

    enum class BlendOperation
    {
        Add,
        Substract,
        ReverseSubstract,
        Min,
        Max
    };

    enum class BlendFactor
    {
        Zero,
        One,
        SrcColor,
        OneMinusSrcColor,
        DstColor,
        OneMinusDstColor,
        SrcAlpha,
        OneMinusSrcAlpha,
        DstAlpha,
        OneMinusDstAlpha,
        ConstantColor,
        OneMinusConstantColor,
        ConstantAlpha,
        OneMinusConstantAlpha,
        AlphaSaturate,
        Src1Color,
        OneMinusSrc1Color,
        Src1Alpha,
        OneMinusSrc1Alpha
    };

    struct BlendState
    {
        BlendState(BlendOperation blendOp = BlendOperation::Add, BlendFactor blendSrc = BlendFactor::One, BlendFactor blendDst = BlendFactor::Zero,
            BlendOperation blendOpAlpha = BlendOperation::Add, BlendFactor blendSrcAlpha = BlendFactor::One, BlendFactor blendDstAlpha = BlendFactor::Zero)
            : BlendOp(blendOp)
            , BlendSrc(blendSrc)
            , BlendDst(blendDst)
            , BlendOpAlpha(blendOpAlpha)
            , BlendSrcAlpha(blendSrcAlpha)
            , BlendDstAlpha(blendDstAlpha) {}

        BlendOperation BlendOp;
        BlendFactor BlendSrc;
        BlendFactor BlendDst;

        BlendOperation BlendOpAlpha;
        BlendFactor BlendSrcAlpha;
        BlendFactor BlendDstAlpha;
    };

    enum class FilterMode
    {
        Point,
        Bilinear,
        Trilinear
    };

    enum class AddressMode
    {
        Wrap,
        Mirror,
        Clamp,
        ClampToOpaqueBlack,
        ClampToOpaqueWhite
    };

    enum class CompareOperation
    {
        Never,
        Less,
        Equal,
        LessEqual,
        Greater,
        NotEqual,
        GreaterEqual,
        Always
    };

    enum class ClearOperation
    {
        Load,
        Clear,
        DontCare
    };

    enum class Topology
    {
        Triangles,
        Lines,
        Points
    };

    enum class CullMode
    {
        None,
        Front,
        Back,
        FrontAndBack,
        Dynamic, // Can be used during pipeline creation to indicate that cull mode is set dynamically before rendering
    };

    enum class FrontFaceMode
    {
        CounterClockwise,
        Clockwise
    };

    struct BufferImageCopy
    {
        // Buffer offset, in bytes.
        size_t BufferOffset = 0;

        /**
         * Buffer row length, in texels. Used to interpret the buffer as an image.
         * If value is 0, image rows should be tightly packed.
         * If value is non-zero, it specifies the width of image buffer is interpreted as.
         */
        uint32_t BufferRowLength = 0;

        /**
         * Buffer image height, in texels. Used to interpret the buffer as an image.
         * If value is 0, image 2D layers should be tightly packed.
         * If value is non-zero, it specifies the height of 2D layer image buffer is interpreted as.
         * Must be 0 for non-3D images.
         */
        uint32_t BufferImageHeight = 0;

        // Specifies image mip level.
        uint32_t ImageMipLevel = 0;

        // Specifies image first array layer.
        uint32_t ImageArrayLayer = 0;

        // Specifies number of image array layers.
        uint32_t ImageArrayLayers = 1;

        // Specifies offset of image region in pixels.
        glm::ivec3 ImageOffset = glm::ivec3(0);

        // Specifies extent of image region in pixels.
        glm::uvec3 ImageExtent = glm::uvec3(0);
    };

    enum class TonemappingMethod
    {
        Reinhard,
        Filmic,
        ACES,
        PhotoLinear,
        AgX,
        PBRNeutral, // From Khronos
        GT7, // From Gran Turismo 7
    };

    enum class AAMethod
    {
        None,
        FXAA,
        MSAA,
        TAA,
    };

    enum class MSAASamples
    {
        x2 = 2,
        x4 = 4,
        x8 = 8,
    };

    struct MSAASettings
    {
        MSAASamples Samples = MSAASamples::x4;
        float EdgeThreshold = 0.1f;
        bool bVisualizeEdges = false;

        bool operator== (const MSAASettings& other) const
        {
            return Samples == other.Samples && EdgeThreshold == other.EdgeThreshold && bVisualizeEdges == other.bVisualizeEdges;
        }
        bool operator!= (const MSAASettings& other) const { return !(*this == other); }
    };

    struct PhotoLinearTonemappingSettings
    {
        float Sensitivity = 0.4f;
        float ExposureTime = 0.06f;
        float FStop = 1.f;

        bool operator== (const PhotoLinearTonemappingSettings& other) const
        {
            return Sensitivity == other.Sensitivity &&
                ExposureTime == other.ExposureTime &&
                FStop == other.FStop;
        }
        bool operator!= (const PhotoLinearTonemappingSettings& other) const { return !(*this == other); }
    };

    struct FilmicTonemappingSettings
    {
        float WhitePoint = 1.f;

        bool operator== (const FilmicTonemappingSettings& other) const
        {
            return WhitePoint == other.WhitePoint;
        }
        bool operator!= (const FilmicTonemappingSettings& other) const { return !(*this == other); }
    };

    struct AgXTonemappingSettings
    {
        glm::vec3 Slope = glm::vec3(1);
        glm::vec3 Power = glm::vec3(1);
        glm::vec3 Offset = glm::vec3(0);
        float Saturation = 1.f;

        static AgXTonemappingSettings GetDefaultLook()
        {
            return AgXTonemappingSettings{};
        }

        static AgXTonemappingSettings GetGoldenLook()
        {
            AgXTonemappingSettings result{};
            result.Slope = glm::vec3(1.0f, 0.9f, 0.5f);
            result.Power = glm::vec3(0.8f);
            result.Offset = glm::vec3(0.0f);
            result.Saturation = 0.8f;

            return result;
        }

        static AgXTonemappingSettings GetPunchyLook()
        {
            AgXTonemappingSettings result{};
            result.Slope = glm::vec3(1.0f);
            result.Power = glm::vec3(1.35f);
            result.Offset = glm::vec3(0.0f);
            result.Saturation = 1.4f;

            return result;
        }

        bool operator== (const AgXTonemappingSettings& other) const
        {
            return Slope == other.Slope &&
                Power == other.Power &&
                Offset == other.Offset &&
                Saturation == other.Saturation;
        }
        bool operator!= (const AgXTonemappingSettings& other) const { return !(*this == other); }
    };

    struct GPUResourceDebugData
    {
        std::string Name;
        size_t Size = 0; // in bytes
    };
    
    struct GPUMemoryStats
    {
        std::vector<GPUResourceDebugData> Resources;
        uint64_t Used = 0;
        uint64_t Free = 0;
    };

    struct RendererConfig
    {
        static constexpr uint32_t FramesInFlight = 3;
        static constexpr uint32_t ReleaseFramesInFlight = 3;
        static constexpr uint32_t BRDFLUTSize = 128;
        static constexpr uint32_t CascadesCount = 4; // Changing it won't change it everywhere. So changing this means changing shaders code

        // TODO: Remove it by implementing a better descriptors system, that will allow them to be reused and created separetely from pipelines (currently, descriptors are created per pipeline).
        // Max Textures that can be imported. It also applies to fonts and shadow maps.
        static constexpr uint32_t MaxTextures = 1024;
    };

    class AssetTexture2D;
    struct BloomSettings
    {
        Ref<AssetTexture2D> Dirt;
        float Threshold = 1.5f;
        float Intensity = 1.f;
        float DirtIntensity = 1.f;
        float Knee = 0.1f;
        bool bEnable = true;

        bool operator== (const BloomSettings& other) const
        {
            return Dirt == other.Dirt &&
                Threshold == other.Threshold &&
                Intensity == other.Intensity &&
                DirtIntensity == other.DirtIntensity &&
                Knee == other.Knee &&
                bEnable == other.bEnable;
        }

        bool operator!= (const BloomSettings& other) const { return !(*this == other); }
    };

    enum class AmbientOcclusion
    {
        None,
        SSAO,
        GTAO
    };

    struct SSAOSettings
    {
        void SetNumberOfSamples(uint32_t number)
        {
            constexpr uint32_t mask = uint32_t(-1) - 1u;

            m_NumberOfSamples = glm::max(2u, number);
            m_NumberOfSamples = m_NumberOfSamples & mask;
        }
        uint32_t GetNumberOfSamples() const { return m_NumberOfSamples; }

        void SetRadius(float radius)
        {
            m_Radius = glm::max(0.f, radius);
        }
        float GetRadius() const { return m_Radius; }

        void SetBias(float bias)
        {
            m_Bias = glm::max(0.f, bias);
        }
        float GetBias() const { return m_Bias; }


        bool operator== (const SSAOSettings& other) const
        {
            return m_NumberOfSamples == other.m_NumberOfSamples &&
                m_Radius == other.m_Radius &&
                m_Bias == other.m_Bias;
        }

        bool operator!= (const SSAOSettings& other) const { return !(*this == other); }

    private:
        // Must be more than 1, also must be even
        uint32_t m_NumberOfSamples = 64;

        float m_Radius = 0.3f;
        float m_Bias = 0.025f;
    };

    struct GTAOSettings
    {
        void SetNumberOfSamples(uint32_t number)
        {
            m_NumberOfSamples = glm::max(1u, number);
        }
        uint32_t GetNumberOfSamples() const { return m_NumberOfSamples; }

        void SetRadius(float radius)
        {
            m_Radius = glm::max(0.f, radius);
        }
        float GetRadius() const { return m_Radius; }

        bool operator== (const GTAOSettings& other) const
        {
            return m_NumberOfSamples == other.m_NumberOfSamples &&
                m_Radius == other.m_Radius;
        }

        bool operator!= (const GTAOSettings& other) const { return !(*this == other); }

    private:
        uint32_t m_NumberOfSamples = 8; // For each direction
        float m_Radius = 1.f;
    };

    enum class FogEquation
    {
        Linear,
        Exp,
        Exp2
    };

    struct FogSettings
    {
        glm::vec3 Color = glm::vec3(1.f);
        float MinDistance = 5.f; // If anything is closer, no fog
        float MaxDistance = 50.f; // Everything after is fog
        float Density = 0.05f; // Used for exp equation
        FogEquation Equation = FogEquation::Linear;
        bool bEnable = false;

        bool operator== (const FogSettings& other) const
        {
            return Color == other.Color &&
                MinDistance == other.MinDistance &&
                MaxDistance == other.MaxDistance &&
                Density == other.Density &&
                Equation == other.Equation &&
                bEnable == other.bEnable;
        }

        bool operator!= (const FogSettings& other) const { return !(*this == other); }
    };

    struct SceneRendererInternalState
    {
        float CascadesSmoothTransitionAlpha = 3.5f / 100.f;
        bool bJitter = false;
        bool bMotionBuffer = false;
        bool bDepthHistory = false;
        bool bNormalHistory = false;
    };

    struct SkySettings
    {
        glm::vec3 SunPos = glm::vec3(0.f, 0.f, -1.f);
        float SkyIntensity = 11.f;
        
        glm::vec3 CloudsColor = glm::vec3(0.650f, 0.570f, 0.475f);
        float Scattering = 0.995f;
        
        float Cirrus = 0.4f;
        float CloudsIntensity = 1.f;
        
        float Cumulus = 0.8f;
        uint32_t CumulusLayers = 3u;
        
        bool bEnableCirrusClouds = false;
        bool bEnableCumulusClouds = false;
    };

    struct VolumetricLightsSettings
    {
        glm::vec3 Albedo = glm::vec3(1.f);
        uint32_t Samples = 20;
        float MaxScatteringDistance = 100.f;
        float FogSpeed = 1.f;
        float Anisotropy = 0.f;
        bool bFogEnable = true;
        bool bEnable = false;

        bool operator== (const VolumetricLightsSettings& other) const
        {
            return Albedo == other.Albedo &&
                Anisotropy == other.Anisotropy &&
                Samples == other.Samples &&
                MaxScatteringDistance == other.MaxScatteringDistance &&
                FogSpeed == other.FogSpeed &&
                bFogEnable == other.bFogEnable &&
                bEnable == other.bEnable;
        }

        bool operator!= (const VolumetricLightsSettings& other) const
        {
            return !((*this) == other);
        }
    };

    struct ShadowMapsSettings
    {
        uint32_t PointLightShadowMapSize = 1024u;
        uint32_t SpotLightShadowMapSize = 1024u;
        std::vector<uint32_t> DirLightShadowMapSizes = { 4096u, 2048u, 2048u, 2048u };

        static constexpr uint32_t MinPointLightShadowMapSize = 64u;
        static constexpr uint32_t MinSpotLightShadowMapSize = 64u;
        static constexpr uint32_t MinDirLightShadowMapSize = 64u;

        bool operator== (const ShadowMapsSettings& other) const
        {
            bool bEqual = PointLightShadowMapSize == other.PointLightShadowMapSize &&
                SpotLightShadowMapSize == other.SpotLightShadowMapSize;

            if (bEqual)
                return DirLightsEqual(other);

            return bEqual;
        }

        bool operator!= (const ShadowMapsSettings& other) const
        {
            return !((*this) == other);
        }

        bool DirLightsEqual(const ShadowMapsSettings& other) const
        {
            for (uint32_t i = 0; i < RendererConfig::CascadesCount; ++i)
                if (DirLightShadowMapSizes[i] != other.DirLightShadowMapSizes[i])
                    return false;

            return true;
        }
    };

    struct DepthOfFieldSettings
    {
        glm::vec2 ApertureShape = glm::vec2(1.f); // [0; 2]
        float ApertureSize = 0.f;
        float FocalLength = 1.0f;
        float COCScale = 10.f;
        float MaxCOC = 18.f;
        bool bDebugOutput = false;

        bool operator== (const DepthOfFieldSettings& other) const
        {
            bool bEqual =
                ApertureShape == other.ApertureShape &&
                ApertureSize == other.ApertureSize &&
                FocalLength == other.FocalLength &&
                COCScale == other.COCScale &&
                MaxCOC == other.MaxCOC &&
                bDebugOutput == other.bDebugOutput;

            return bEqual;
        }

        bool operator!= (const DepthOfFieldSettings& other) const
        {
            return !((*this) == other);
        }
    };

    struct MotionBlurSettings
    {
        float Strength = 1.f; // [0; 1]
        uint32_t NumSamples = 16;
        bool bEnable = false;
        bool bDebugOutput = false;

        bool operator== (const MotionBlurSettings& other) const
        {
            bool bEqual =
                Strength == other.Strength &&
                NumSamples == other.NumSamples &&
                bEnable == other.bEnable &&
                bDebugOutput == other.bDebugOutput;

            return bEqual;
        }

        bool operator!= (const MotionBlurSettings& other) const
        {
            return !((*this) == other);
        }
    };

    struct AutoExposureSettings
    {
        float MinLogLum = -10.0f;
        float MaxLogLum = 2.0f;
        float AdaptationSpeed = 1.0f;
        float AdaptationKey = 0.1f;

        bool bEnable = false;
        bool bHalfResolution = true;

        bool operator== (const AutoExposureSettings& other) const
        {
            bool bEqual =
                MinLogLum == other.MinLogLum &&
                MaxLogLum == other.MaxLogLum &&
                AdaptationSpeed == other.AdaptationSpeed &&
                AdaptationKey == other.AdaptationKey &&
                bEnable == other.bEnable &&
                bHalfResolution == other.bHalfResolution;

            return bEqual;
        }

        bool operator!= (const AutoExposureSettings& other) const
        {
            return !((*this) == other);
        }
    };

    struct ScreenSpaceReflectionsSettings
    {
        float RoughnessThreshold = 0.7f;
        uint32_t SamplesPerQuad = 1;
        uint32_t MaxTraversalIterations = 128;
        bool bEnable = true;

        bool operator== (const ScreenSpaceReflectionsSettings& other) const
        {
            bool bEqual =
                RoughnessThreshold == other.RoughnessThreshold &&
                SamplesPerQuad == other.SamplesPerQuad &&
                MaxTraversalIterations == other.MaxTraversalIterations &&
                bEnable == other.bEnable;

            return bEqual;
        }

        bool operator!= (const ScreenSpaceReflectionsSettings& other) const
        {
            return !((*this) == other);
        }
    };

    struct LensSettings
    {
        bool bEnableChromaticAberration = false;
        bool bEnableVignette = false;
        bool bEnableFilmGrain = false;
        float ChromaticIntensity = 0.4f;
        float VignetteIntensity = 0.3f;
        float FilmGrainScale = 0.01f;
        float FilmGrainAmount = 0.25f;
        float FilmGrainSeedUpdateRate = 0.02f; // every FilmGrainSeedUpdateRate seconds

        bool operator== (const LensSettings& other) const
        {
            bool bEqual =
                bEnableChromaticAberration == other.bEnableChromaticAberration &&
                bEnableVignette == other.bEnableVignette &&
                bEnableFilmGrain == other.bEnableFilmGrain &&
                ChromaticIntensity == other.ChromaticIntensity &&
                VignetteIntensity == other.VignetteIntensity &&
                FilmGrainScale == other.FilmGrainScale &&
                FilmGrainAmount == other.FilmGrainAmount &&
                FilmGrainSeedUpdateRate == other.FilmGrainSeedUpdateRate;

            return bEqual;
        }

        bool operator!= (const LensSettings& other) const
        {
            return !((*this) == other);
        }
    };

    struct SceneRendererSettings
    {
        BloomSettings BloomSettings;
        SSAOSettings SSAOSettings;
        GTAOSettings GTAOSettings;
        FogSettings FogSettings;
        ShadowMapsSettings ShadowsSettings;
        VolumetricLightsSettings VolumetricSettings;
        PhotoLinearTonemappingSettings PhotoLinearTonemappingParams;
        FilmicTonemappingSettings FilmicTonemappingParams;
        AgXTonemappingSettings AgXTonemappingParams = AgXTonemappingSettings::GetPunchyLook();
        DepthOfFieldSettings DOFSettings;
        MotionBlurSettings MotionBlur;
        AutoExposureSettings AutoExposure;
        ScreenSpaceReflectionsSettings ScreenSpaceReflections;
        LensSettings Lens;
        float Gamma = 2.2f;
        float Exposure = 1.f;
        float LineWidth = 2.5f;
        TonemappingMethod Tonemapping = TonemappingMethod::AgX;
        AmbientOcclusion AO = AmbientOcclusion::None;
        AAMethod AA = AAMethod::None;
        MSAASettings MSAAParams;
        bool bTranslucentShadows = true;
        bool bEnableSoftShadows = false;
        bool bEnableCSMSmoothTransition = true;
        bool bVisualizeCascades = false;
        bool bVisualizeLightTiles = false;
        bool bEnableObjectPicking = false;
        bool bEnable2DObjectPicking = false;
        bool bSortOpaqueParticles = false;
        bool bEnableDebugLinesDepthTest = true;
        float GridScale = 4.f; // Editor Only
        uint32_t TransparencyLayers = 4u;

        SceneRendererInternalState InternalState; // Internal

        bool operator== (const SceneRendererSettings& other) const
        {
            return PhotoLinearTonemappingParams == other.PhotoLinearTonemappingParams &&
                FilmicTonemappingParams == other.FilmicTonemappingParams &&
                AgXTonemappingParams == other.AgXTonemappingParams &&
                DOFSettings == other.DOFSettings &&
                MotionBlur == other.MotionBlur &&
                FogSettings == other.FogSettings &&
                ShadowsSettings == other.ShadowsSettings &&
                VolumetricSettings == other.VolumetricSettings &&
                Gamma == other.Gamma &&
                Exposure == other.Exposure &&
                LineWidth == other.LineWidth &&
                Tonemapping == other.Tonemapping &&
                AO == other.AO &&
                AA == other.AA &&
                MSAAParams == other.MSAAParams &&
                bTranslucentShadows == other.bTranslucentShadows &&
                bEnableSoftShadows == other.bEnableSoftShadows &&
                bEnableCSMSmoothTransition == other.bEnableCSMSmoothTransition &&
                bVisualizeCascades == other.bVisualizeCascades &&
                bVisualizeLightTiles == other.bVisualizeLightTiles &&
                bEnableObjectPicking == other.bEnableObjectPicking &&
                bEnable2DObjectPicking == other.bEnable2DObjectPicking &&
                bSortOpaqueParticles == other.bSortOpaqueParticles &&
                bEnableDebugLinesDepthTest == other.bEnableDebugLinesDepthTest &&
                AutoExposure == other.AutoExposure &&
                ScreenSpaceReflections == other.ScreenSpaceReflections &&
                Lens == other.Lens &&
                SSAOSettings == other.SSAOSettings &&
                GTAOSettings == other.GTAOSettings &&
                GridScale == other.GridScale &&
                TransparencyLayers == other.TransparencyLayers &&
                BloomSettings == other.BloomSettings;
        }

        bool operator!= (const SceneRendererSettings& other) const { return !(*this == other); }
    
        static SceneRendererSettings GetBasicSettings()
        {
            SceneRendererSettings settings;
            settings.VolumetricSettings.bEnable = false;
            settings.bTranslucentShadows = false;
            settings.bEnableCSMSmoothTransition = false;
            settings.bEnableObjectPicking = false;
            settings.ScreenSpaceReflections.bEnable = false;
            settings.TransparencyLayers = 2u;

            return settings;
        }
    };

    struct RendererDebugVertex
    {
        glm::vec3 Location = glm::vec3(0);
        glm::vec3 Color = glm::vec3(0, 1, 0);
    };

    struct RendererLine
    {
        RendererDebugVertex Start;
        RendererDebugVertex End;
    };

    struct RendererTriangle
    {
        std::array<RendererDebugVertex, 3> Vertices;
    };

    // Returns bits
    inline constexpr uint32_t GetImageFormatBPP(ImageFormat format)
    {
	    switch (format)
	    {
            case ImageFormat::R32G32B32A32_Float :      return 4 * 32;
            case ImageFormat::R32G32B32A32_UInt :       return 4 * 32;
            case ImageFormat::R32G32B32A32_SInt :       return 4 * 32;
            case ImageFormat::R32G32B32_Float :         return 3 * 32;
            case ImageFormat::R32G32B32_UInt :          return 3 * 32;
            case ImageFormat::R32G32B32_SInt :          return 3 * 32;
            case ImageFormat::R16G16B16A16_Float :      return 4 * 16;
            case ImageFormat::R16G16B16A16_UNorm :      return 4 * 16;
            case ImageFormat::R16G16B16A16_UInt :       return 4 * 16;
            case ImageFormat::R16G16B16A16_SNorm :      return 4 * 16;
            case ImageFormat::R16G16B16A16_SInt :       return 4 * 16;
            case ImageFormat::R32G32_Float :            return 2 * 32;
            case ImageFormat::R32G32_UInt :             return 2 * 32;
            case ImageFormat::R32G32_SInt :             return 2 * 32;
            case ImageFormat::D32_Float_S8X24_UInt :    return 64;
            case ImageFormat::R10G10B10A2_UNorm :       return 32;
            case ImageFormat::R10G10B10A2_UInt :        return 32;
            case ImageFormat::R11G11B10_Float :         return 32;
            case ImageFormat::R8G8B8A8_UNorm :          return 4 * 8;
            case ImageFormat::R8G8B8A8_UNorm_SRGB :     return 4 * 8;
            case ImageFormat::R8G8B8A8_UInt :           return 4 * 8;
            case ImageFormat::R8G8B8A8_SNorm :          return 4 * 8;
            case ImageFormat::R8G8B8A8_SInt :           return 4 * 8;
            case ImageFormat::R8G8B8_UNorm :            return 3 * 8;
            case ImageFormat::R8G8B8_UNorm_SRGB :       return 3 * 8;
            case ImageFormat::R8G8B8_UInt :             return 3 * 8;
            case ImageFormat::R8G8B8_SNorm :            return 3 * 8;
            case ImageFormat::R8G8B8_SInt :             return 3 * 8;
            case ImageFormat::R16G16_Float :            return 2 * 16;
            case ImageFormat::R16G16_UNorm :            return 2 * 16;
            case ImageFormat::R16G16_UInt :             return 2 * 16;
            case ImageFormat::R16G16_SNorm :            return 2 * 16;
            case ImageFormat::R16G16_SInt :             return 2 * 16;
            case ImageFormat::D32_Float :               return 32;
            case ImageFormat::R32_Float :               return 32;
            case ImageFormat::R32_UInt :                return 32;
            case ImageFormat::R32_SInt :                return 32;
            case ImageFormat::D24_UNorm_S8_UInt :       return 32;
            case ImageFormat::R8G8_UNorm :              return 2 * 8;
            case ImageFormat::R8G8_UNorm_SRGB :         return 2 * 8;
            case ImageFormat::R8G8_UInt :               return 2 * 8;
            case ImageFormat::R8G8_SNorm :              return 2 * 8;
            case ImageFormat::R8G8_SInt :               return 2 * 8;
            case ImageFormat::R16_Float :               return 16;
            case ImageFormat::D16_UNorm :               return 16;
            case ImageFormat::R16_UNorm :               return 16;
            case ImageFormat::R16_UInt :                return 16;
            case ImageFormat::R16_SNorm :               return 16;
            case ImageFormat::R16_SInt :                return 16;
            case ImageFormat::R8_UNorm_SRGB:            return 8;
            case ImageFormat::R8_UNorm :                return 8;
            case ImageFormat::R8_UInt :                 return 8;
            case ImageFormat::R8_SNorm :                return 8;
            case ImageFormat::R8_SInt :                 return 8;
            case ImageFormat::R9G9B9E5_SharedExp :      return 32;
            case ImageFormat::R8G8_B8G8_UNorm :         return 4 * 8;
            case ImageFormat::G8R8_G8B8_UNorm :         return 4 * 8;
            case ImageFormat::BC1_RGBA_UNorm :          return 4;
            case ImageFormat::BC1_RGB_UNorm :           return 4;
            case ImageFormat::BC1_UNorm_SRGB :          return 4;
            case ImageFormat::BC2_UNorm :               return 8;
            case ImageFormat::BC2_UNorm_SRGB :          return 8;
            case ImageFormat::BC3_UNorm :               return 8;
            case ImageFormat::BC3_UNorm_SRGB :          return 8;
            case ImageFormat::BC4_UNorm :               return 4;
            case ImageFormat::BC4_SNorm :               return 4;
            case ImageFormat::BC5_UNorm :               return 8;
            case ImageFormat::BC5_SNorm :               return 8;
            case ImageFormat::B5G6R5_UNorm :            return 16;
            case ImageFormat::B5G5R5A1_UNorm :          return 16;
            case ImageFormat::B8G8R8A8_UNorm :          return 4 * 8;
            case ImageFormat::B8G8R8A8_UNorm_SRGB :     return 4 * 8;
            case ImageFormat::BC6H_UFloat16 :           return 8;
            case ImageFormat::BC6H_SFloat16 :           return 8;
            case ImageFormat::BC7_UNorm :               return 8;
            case ImageFormat::BC7_UNorm_SRGB:           return 8;
            case ImageFormat::ETC2_RGB_UNorm:           return 4;
            case ImageFormat::ETC2_RGBA_UNorm:          return 8;
	    }
        assert(!"Unknown format");
	    return 0;
    }
    
    inline constexpr bool IsCompressedFormat(ImageFormat format)
    {
	    switch (format)
	    {
            case ImageFormat::BC1_RGBA_UNorm :
            case ImageFormat::BC1_RGB_UNorm : 
            case ImageFormat::BC1_UNorm_SRGB :
            case ImageFormat::BC2_UNorm :     
            case ImageFormat::BC2_UNorm_SRGB :
            case ImageFormat::BC3_UNorm :     
            case ImageFormat::BC3_UNorm_SRGB :
            case ImageFormat::BC4_UNorm :     
            case ImageFormat::BC4_SNorm :     
            case ImageFormat::BC5_UNorm :     
            case ImageFormat::BC5_SNorm :     
            case ImageFormat::BC6H_UFloat16 : 
            case ImageFormat::BC6H_SFloat16 : 
            case ImageFormat::BC7_UNorm :     
            case ImageFormat::BC7_UNorm_SRGB:
            case ImageFormat::ETC2_RGB_UNorm:
            case ImageFormat::ETC2_RGBA_UNorm:
                return true;
	    }
	    return false;
    }

    inline uint32_t CalculateMipCount(uint32_t width, uint32_t height)
    {
        return (uint32_t)::std::floor(::std::log2(glm::max(width, height))) + 1;
    }

    inline uint32_t CalculateMipCount(glm::uvec2 size)
    {
        return (uint32_t)::std::floor(::std::log2(glm::max(size.x, size.y))) + 1;
    }

    inline uint32_t CalculateMipCount(const glm::uvec3& size)
    {
        uint32_t maxSide = glm::max(size.x, size.y);
        maxSide = glm::max(maxSide, size.z);
        return (uint32_t)::std::floor(::std::log2(maxSide)) + 1;
    }

    inline constexpr size_t CalculateImageMemorySize(ImageFormat format, uint32_t width, uint32_t height)
    {
        const size_t bits = (size_t)GetImageFormatBPP(format);
        const size_t size = (size_t)width * (size_t)height;
        if (bits < 8)
            return size / (8 / bits); // For example, BC1 is 4 bits
        return (bits / 8) * size;
    }

    inline constexpr size_t CalculateImageMemorySize(ImageFormat format, glm::uvec2 size)
    {
        return CalculateImageMemorySize(format, size.x, size.y);
    }

    inline constexpr size_t CalculateImageMemorySize(ImageFormat format, const glm::uvec3& size)
    {
        return CalculateImageMemorySize(format, size.x, size.y) * (size_t)size.z;
    }

    // Returns bits
    inline constexpr uint32_t GetImageFormatChannels(ImageFormat format)
    {
        switch (format)
        {
        case ImageFormat::R32G32B32A32_Float:      return 4;
        case ImageFormat::R32G32B32A32_UInt:       return 4;
        case ImageFormat::R32G32B32A32_SInt:       return 4;
        case ImageFormat::R32G32B32_Float:         return 3;
        case ImageFormat::R32G32B32_UInt:          return 3;
        case ImageFormat::R32G32B32_SInt:          return 3;
        case ImageFormat::R16G16B16A16_Float:      return 4;
        case ImageFormat::R16G16B16A16_UNorm:      return 4;
        case ImageFormat::R16G16B16A16_UInt:       return 4;
        case ImageFormat::R16G16B16A16_SNorm:      return 4;
        case ImageFormat::R16G16B16A16_SInt:       return 4;
        case ImageFormat::R32G32_Float:            return 2;
        case ImageFormat::R32G32_UInt:             return 2;
        case ImageFormat::R32G32_SInt:             return 2;
        case ImageFormat::D32_Float_S8X24_UInt:    return 2;
        case ImageFormat::R10G10B10A2_UNorm:       return 4;
        case ImageFormat::R10G10B10A2_UInt:        return 4;
        case ImageFormat::R11G11B10_Float:         return 3;
        case ImageFormat::R8G8B8A8_UNorm:          return 4;
        case ImageFormat::R8G8B8A8_UNorm_SRGB:     return 4;
        case ImageFormat::R8G8B8A8_UInt:           return 4;
        case ImageFormat::R8G8B8A8_SNorm:          return 4;
        case ImageFormat::R8G8B8A8_SInt:           return 4;
        case ImageFormat::R8G8B8_UNorm:            return 3;
        case ImageFormat::R8G8B8_UNorm_SRGB:       return 3;
        case ImageFormat::R8G8B8_UInt:             return 3;
        case ImageFormat::R8G8B8_SNorm:            return 3;
        case ImageFormat::R8G8B8_SInt:             return 3;
        case ImageFormat::R16G16_Float:            return 2;
        case ImageFormat::R16G16_UNorm:            return 2;
        case ImageFormat::R16G16_UInt:             return 2;
        case ImageFormat::R16G16_SNorm:            return 2;
        case ImageFormat::R16G16_SInt:             return 2;
        case ImageFormat::D32_Float:               return 1;
        case ImageFormat::R32_Float:               return 1;
        case ImageFormat::R32_UInt:                return 1;
        case ImageFormat::R32_SInt:                return 1;
        case ImageFormat::D24_UNorm_S8_UInt:       return 1;
        case ImageFormat::R8G8_UNorm:              return 2;
        case ImageFormat::R8G8_UNorm_SRGB:         return 2;
        case ImageFormat::R8G8_UInt:               return 2;
        case ImageFormat::R8G8_SNorm:              return 2;
        case ImageFormat::R8G8_SInt:               return 2;
        case ImageFormat::R16_Float:               return 1;
        case ImageFormat::D16_UNorm:               return 1;
        case ImageFormat::R16_UNorm:               return 1;
        case ImageFormat::R16_UInt:                return 1;
        case ImageFormat::R16_SNorm:               return 1;
        case ImageFormat::R16_SInt:                return 1;
        case ImageFormat::R8_UNorm_SRGB:           return 1;
        case ImageFormat::R8_UNorm:                return 1;
        case ImageFormat::R8_UInt:                 return 1;
        case ImageFormat::R8_SNorm:                return 1;
        case ImageFormat::R8_SInt:                 return 1;
        case ImageFormat::R9G9B9E5_SharedExp:      return 4;
        case ImageFormat::R8G8_B8G8_UNorm:         return 4;
        case ImageFormat::G8R8_G8B8_UNorm:         return 4;
        case ImageFormat::BC1_RGBA_UNorm:          return 1;
        case ImageFormat::BC1_RGB_UNorm:           return 1;
        case ImageFormat::BC1_UNorm_SRGB:          return 1;
        case ImageFormat::BC2_UNorm:               return 1;
        case ImageFormat::BC2_UNorm_SRGB:          return 1;
        case ImageFormat::BC3_UNorm:               return 1;
        case ImageFormat::BC3_UNorm_SRGB:          return 1;
        case ImageFormat::BC4_UNorm:               return 1;
        case ImageFormat::BC4_SNorm:               return 1;
        case ImageFormat::BC5_UNorm:               return 1;
        case ImageFormat::BC5_SNorm:               return 1;
        case ImageFormat::B5G6R5_UNorm:            return 3;
        case ImageFormat::B5G5R5A1_UNorm:          return 4;
        case ImageFormat::B8G8R8A8_UNorm:          return 4;
        case ImageFormat::B8G8R8A8_UNorm_SRGB:     return 4;
        case ImageFormat::BC6H_UFloat16:           return 1;
        case ImageFormat::BC6H_SFloat16:           return 1;
        case ImageFormat::BC7_UNorm:               return 1;
        case ImageFormat::BC7_UNorm_SRGB:          return 1;
        case ImageFormat::ETC2_RGB_UNorm:          return 1;
        case ImageFormat::ETC2_RGBA_UNorm:         return 1;
        }
        assert(!"Unknown format");
        return 0;
    }

    inline constexpr ImageFormat ChannelsToFormat(int channels)
    {
        switch (channels)
        {
        case 1: return ImageFormat::R8_UNorm;
        case 2: return ImageFormat::R8G8_UNorm;
        case 3: return ImageFormat::R8G8B8_UNorm;
        case 4: return ImageFormat::R8G8B8A8_UNorm;
        }
        assert(!"Invalid channels count");
        return ImageFormat::Unknown;
    }

    inline constexpr ImageFormat HDRChannelsToFormat(int channels)
    {
        switch (channels)
        {
        case 1: return ImageFormat::R32_Float;
        case 2: return ImageFormat::R32G32_Float;
        case 3: return ImageFormat::R32G32B32_Float;
        case 4: return ImageFormat::R32G32B32A32_Float;
        }
        assert(!"Invalid channels count");
        return ImageFormat::Unknown;
    }

    inline std::array<glm::vec3, 8> GetFrustumCornersWorldSpace(const glm::mat4& view, const glm::mat4& proj)
    {
        constexpr glm::vec4 frustumCornersNDC[8] =
        {
            { -1.f, -1.f, +0.f, 1.f },
            { +1.f, -1.f, +0.f, 1.f },
            { +1.f, +1.f, +0.f, 1.f },
            { -1.f, +1.f, +0.f, 1.f },
            { -1.f, -1.f, +1.f, 1.f },
            { +1.f, -1.f, +1.f, 1.f },
            { +1.f, +1.f, +1.f, 1.f },
            { -1.f, +1.f, +1.f, 1.f },
        };

        const glm::mat4 invViewProj = glm::inverse(proj * view);
        std::array<glm::vec3, 8> frustumCornersWS;
        for (int i = 0; i < 8; ++i)
        {
            const glm::vec4 ws = invViewProj * frustumCornersNDC[i];
            frustumCornersWS[i] = glm::vec3(ws / ws.w);
        }
        return frustumCornersWS;
    }

    inline glm::vec3 GetFrustumCenter(const std::array<glm::vec3, 8>& frustumCorners)
    {
        glm::vec3 result(0.f);
        for (int i = 0; i < frustumCorners.size(); ++i)
        {
            result += frustumCorners[i];
        }
        result /= float(frustumCorners.size());

        return result;
    }

    inline float CalculatePhotoLinearScale(const PhotoLinearTonemappingSettings& params, float gamma)
    {
        // H = q L t / N^2
            //
            // where:
            //  q has a typical value is q = 0.65
            //  L is the luminance of the scene in candela per m^2 (sensitivity)
            //  t is the exposure time in seconds (exposure)
            //  N is the aperture f-number (fstop)
        const float result = 0.65f * params.ExposureTime * params.Sensitivity /
            (params.FStop * params.FStop) * 10.f /
            pow(118.f / 255.f, gamma);
        return result;
    }

    inline float CreateHaltonSequence(uint32_t index, uint32_t base)
    {
        float f = 1.f;
        float r = 0.f;
        uint32_t current = index;
        do
        {
            f = f / float(base);
            r += f * (current % base);
            current = (uint32_t)glm::floor(current / base);
        } while (current > 0u);
        return r;
    }
}

namespace std
{
    template<>
    struct hash<Eagle::ImageView>
    {
        std::size_t operator()(const Eagle::ImageView& view) const noexcept
        {
            std::size_t result = std::hash<uint32_t>()(view.MipLevel);
            ::Eagle::HashCombine(result, view.Layer);

            return result;
        }
    };
}
