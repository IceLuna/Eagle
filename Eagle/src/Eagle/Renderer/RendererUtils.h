#pragma once

#include "Eagle/Core/Core.h"
#include <glm/glm.hpp>
#include <array>

namespace Eagle
{
    using Index = uint32_t;

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
        BC1_UNorm,
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
        BC7_UNorm_SRGB
    };

    inline bool IsSRGBFormat(ImageFormat format)
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
        Trilinear,
        Anisotropic
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
        FrontAndBack
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
         * Buffer row length, in texels. Used to interpret the buffer as an image.\n
         * If value is 0, image rows should be tightly packed.\n
         * If value is non-zero, it specifies the width of image buffer is interpreted as.
         */
        uint32_t BufferRowLength = 0;

        /**
         * Buffer image height, in texels. Used to interpret the buffer as an image.\n
         * If value is 0, image 2D layers should be tightly packed.\n
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
        glm::ivec3 ImageOffset;

        // Specifies extent of image region in pixels.
        glm::uvec3 ImageExtent;
    };

    enum class TonemappingMethod
    {
        None,
        Reinhard,
        Filmic,
        ACES,
        PhotoLinear
    };

    struct PhotoLinearTonemappingSettings
    {
        float Sensetivity = 1.f;
        float ExposureTime = 0.12f;
        float FStop = 1.f;

        bool operator== (const PhotoLinearTonemappingSettings& other) const
        {
            return Sensetivity == other.Sensetivity &&
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
        static constexpr uint32_t ReleaseFramesInFlight = FramesInFlight * 2; // For releasing resources
        static constexpr uint32_t BRDFLUTSize = 512;
        static constexpr uint32_t DirLightShadowMapSize = 2048;
        static constexpr glm::uvec3 PointLightSMSize = glm::uvec3(2048, 2048, 1);
        static constexpr glm::uvec3 SpotLightSMSize = glm::uvec3(2048, 2048, 1);
    };

    class Texture2D;
    struct BloomSettings
    {
        Ref<Texture2D> Dirt;
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
        float m_Radius = 0.5f;
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

    struct OptionalGBuffers
    {
        bool bMotion = true;

        bool operator== (const OptionalGBuffers& other) const
        {
            return bMotion == other.bMotion;
        }

        bool operator!= (const OptionalGBuffers& other) const { return !(*this == other); }
    };

    struct SceneRendererSettings
    {
        BloomSettings BloomSettings;
        SSAOSettings SSAOSettings;
        GTAOSettings GTAOSettings;
        FogSettings FogSettings;
        PhotoLinearTonemappingSettings PhotoLinearTonemappingParams;
        FilmicTonemappingSettings FilmicTonemappingParams;
        float Gamma = 2.2f;
        float Exposure = 1.f;
        float LineWidth = 2.5f;
        TonemappingMethod Tonemapping = TonemappingMethod::ACES;
        AmbientOcclusion AO = AmbientOcclusion::None;
        bool bEnableSoftShadows = true;
        bool bVisualizeCascades = false;
        OptionalGBuffers OptionalGBuffers;

        bool operator== (const SceneRendererSettings& other) const
        {
            return PhotoLinearTonemappingParams == other.PhotoLinearTonemappingParams &&
                FilmicTonemappingParams == other.FilmicTonemappingParams &&
                FogSettings == other.FogSettings &&
                Gamma == other.Gamma &&
                Exposure == other.Exposure &&
                LineWidth == other.LineWidth &&
                Tonemapping == other.Tonemapping &&
                AO == other.AO &&
                bEnableSoftShadows == other.bEnableSoftShadows &&
                bVisualizeCascades == other.bVisualizeCascades &&
                SSAOSettings == other.SSAOSettings &&
                GTAOSettings == other.GTAOSettings &&
                OptionalGBuffers == other.OptionalGBuffers &&
                BloomSettings == other.BloomSettings;
        }

        bool operator!= (const SceneRendererSettings& other) const { return !(*this == other); }
    };

    struct RendererLine
    {
        glm::vec3 Color = glm::vec3(0, 1, 0);
        glm::vec3 Start;
        glm::vec3 End;
    };

    // Returns bits
    static uint32_t GetImageFormatBPP(ImageFormat format)
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
            case ImageFormat::BC1_UNorm :               return 4;
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
	    }
        assert(!"Unknown format");
	    return 0;
    }

    static uint32_t CalculateMipCount(uint32_t width, uint32_t height)
    {
        return (uint32_t)::std::floor(::std::log2(glm::max(width, height))) + 1;
    }

    static uint32_t CalculateMipCount(glm::uvec2 size)
    {
        return (uint32_t)::std::floor(::std::log2(glm::max(size.x, size.y))) + 1;
    }

    static uint32_t CalculateMipCount(const glm::uvec3& size)
    {
        uint32_t maxSide = glm::max(size.x, size.y);
        maxSide = glm::max(maxSide, size.z);
        return (uint32_t)::std::floor(::std::log2(maxSide)) + 1;
    }

    static size_t CalculateImageMemorySize(ImageFormat format, uint32_t width, uint32_t height)
    {
        return ((size_t)GetImageFormatBPP(format) / 8) * (size_t)width * (size_t)height;
    }

    static size_t CalculateImageMemorySize(ImageFormat format, glm::uvec2 size)
    {
        return ((size_t)GetImageFormatBPP(format) / 8) * (size_t)size.x * (size_t)size.y;
    }

    static size_t CalculateImageMemorySize(ImageFormat format, const glm::uvec3& size)
    {
        return ((size_t)GetImageFormatBPP(format) / 8) * (size_t)size.x * (size_t)size.y * (size_t)size.z;
    }

    static ImageFormat ChannelsToFormat(int channels)
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

    static ImageFormat HDRChannelsToFormat(int channels)
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

    static std::array<glm::vec3, 8> GetFrustumCornersWorldSpace(const glm::mat4& view, const glm::mat4& proj)
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

    static glm::vec3 GetFrustumCenter(const std::array<glm::vec3, 8>& frustumCorners)
    {
        glm::vec3 result(0.f);
        for (int i = 0; i < frustumCorners.size(); ++i)
        {
            result += frustumCorners[i];
        }
        result /= float(frustumCorners.size());

        return result;
    }

    static float CalculatePhotoLinearScale(const PhotoLinearTonemappingSettings& params, float gamma)
    {
        // H = q L t / N^2
            //
            // where:
            //  q has a typical value is q = 0.65
            //  L is the luminance of the scene in candela per m^2 (sensitivity)
            //  t is the exposure time in seconds (exposure)
            //  N is the aperture f-number (fstop)
        const float result = 0.65f * params.ExposureTime * params.Sensetivity /
            (params.FStop * params.FStop) * 10.f /
            pow(118.f / 255.f, gamma);
        return result;
    }

    static float GetRadRotationTemporal(const uint64_t frameNumber)
    {
        const float aRotation[] = { 60.f, 300.f, 180.f, 240.f, 120.f, 0.f };
        return aRotation[frameNumber % 6] / 360.f * 2.f * 3.14159265358979323846f;
    };
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
