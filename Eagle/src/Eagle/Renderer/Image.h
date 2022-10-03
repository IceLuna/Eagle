#pragma once

#include "Eagle/Core/DataBuffer.h"
#include "RendererUtils.h"

namespace Eagle
{
    struct ImageSpecifications
    {
        glm::uvec3 Size;
        ImageFormat Format = ImageFormat::Unknown;
        ImageUsage Usage = ImageUsage::None;
        ImageLayout Layout = ImageLayout();
        ImageType Type = ImageType::Type2D;
        SamplesCount SamplesCount = SamplesCount::Samples1;
        MemoryType MemoryType = MemoryType::Gpu;
        uint32_t MipsCount = 1;
        bool bIsCube = false;
    };

    class Image : virtual public std::enable_shared_from_this<Image>
    {
    protected:
        Image(const ImageSpecifications& specs, const std::string& debugName = "")
            : m_Specs(specs)
            , m_DebugName(debugName) {}

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
        virtual void* GetImageViewHandle(const ImageView& viewInfo) const = 0;

        void SetImageLayout(ImageLayout layout) { m_Specs.Layout = layout; }

        static Ref<Image> Create(const ImageSpecifications& specs, const std::string& debugName = "");

    protected:
        ImageSpecifications m_Specs;
        std::string m_DebugName;
    };
}
