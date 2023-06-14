#pragma once

#include "Eagle/Core/DataBuffer.h"
#include "Eagle/Renderer/RendererUtils.h"

namespace Eagle
{
    struct ImageSpecifications
    {
        glm::uvec3 Size;
        ImageFormat Format = ImageFormat::Unknown;
        ImageUsage Usage = ImageUsage::None;
        mutable ImageLayout Layout = ImageLayout(); // Layout of Mip #0
        ImageType Type = ImageType::Type2D;
        SamplesCount SamplesCount = SamplesCount::Samples1;
        MemoryType MemoryType = MemoryType::Gpu;
        uint32_t MipsCount = 1; // Can be set to UINT_MAX to internally calculate mips count based on image size
        bool bIsCube = false;
    };

    class Image : virtual public std::enable_shared_from_this<Image>
    {
    protected:
        Image(const ImageSpecifications& specs, const std::string& debugName = "")
            : m_Specs(specs)
            , m_DebugName(debugName)
        {
            if (m_Specs.MipsCount == UINT_MAX)
                bCalculateMipsCountInternally = true;
        }

    public:
        virtual ~Image() = default;

        bool HasUsage(ImageUsage usage) const { return HasFlags(m_Specs.Usage, usage); }
        ImageView GetImageView() const { return ImageView{ 0, m_Specs.MipsCount, 0 }; }

        const glm::uvec3& GetSize() const { return m_Specs.Size; }
        ImageFormat GetFormat() const { return m_Specs.Format; }
        ImageUsage GetUsage() const { return m_Specs.Usage; }
        ImageLayout GetLayout() const { return m_Specs.Layout; }
        ImageType GetType() const { return m_Specs.Type; }
        SamplesCount GetSamplesCount() const { return m_Specs.SamplesCount; }
        MemoryType GetMemoryType() const { return m_Specs.MemoryType; }
        uint32_t GetMipsCount() const { return m_Specs.MipsCount; }
        uint32_t GetLayersCount() const { return m_Specs.bIsCube ? 6 : 1; }
        bool IsCube() const { return m_Specs.bIsCube; }
        const std::string& GetDebugName() const { return m_DebugName; }

        virtual void Resize(const glm::uvec3& size) = 0;
        [[nodiscard]] virtual void* Map() = 0;
        virtual void Unmap() = 0;

        virtual void Read(void* data, ImageLayout initialLayout, ImageLayout finalLayout) = 0;
        virtual void Read(void* data, size_t size, const glm::ivec3& position, const glm::uvec3& extent, ImageLayout initialLayout, ImageLayout finalLayout) = 0;

        virtual void* GetHandle() const = 0;
        virtual void* GetImageViewHandle() const = 0;
        virtual void* GetImageViewHandle(const ImageView& viewInfo, bool bForce2D = false) const = 0;

        static Ref<Image> Create(ImageSpecifications specs, const std::string& debugName = "");

    private:
        void SetImageLayout(ImageLayout layout) { m_Specs.Layout = layout; }

    protected:
        ImageSpecifications m_Specs;
        std::string m_DebugName;
        bool bCalculateMipsCountInternally = false;

        friend class VulkanCommandManager;
        friend class VulkanCommandBuffer;
    };
}
