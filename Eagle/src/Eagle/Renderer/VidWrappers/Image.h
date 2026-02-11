#pragma once

#include "Eagle/Core/DataBuffer.h"
#include "Eagle/Renderer/RendererUtils.h"

namespace Eagle
{
    struct ImageSpecifications
    {
        glm::uvec3 Size = glm::uvec3(0);
        ImageFormat Format = ImageFormat::Unknown;
        ImageUsage Usage = ImageUsage::None;
        mutable ImageLayout Layout = ImageLayout(); // Layout of Mip #0
        ImageType Type = ImageType::Type2D;
        SamplesCount SamplesCount = SamplesCount::Samples1;
        MemoryType MemoryType = MemoryType::Gpu;
        uint32_t MipsCount = 1; // Can be set to UINT_MAX to internally calculate mips count based on image size
        bool bIsCube = false;
    };

    struct ImageSubresourceLayout
    {
        size_t Offset = 0ull; // The byte offset from the start of the image or the plane where the image subresource begins.
        size_t Size = 0ull; // The size in bytes of the image subresource. size includes any extra memory that is required based on rowPitch.
        size_t RowPitch = 0ull; // Describes the number of bytes between each row of texels in an image.
        size_t ArrayPitch = 0ull; // Describes the number of bytes between each array layer of an image
        size_t DepthPitch = 0ull; // Describes the number of bytes between each slice of 3D image
    };

    class Image : virtual public std::enable_shared_from_this<Image>
    {
    protected:
        Image(const ImageSpecifications& specs, const std::string& debugName = "")
            : m_Specs(specs)
            , m_DebugName(debugName)
            , m_Revision(0)
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

        const ImageSpecifications& GetSpecs() const { return m_Specs; }

        uint8_t GetRevision() const { return m_Revision; }

        virtual void Resize(const glm::uvec3& size) = 0;
        [[nodiscard]] virtual void* Map() = 0;
        virtual void Unmap() = 0;

        virtual void Read(void* data, ImageLayout initialLayout, ImageLayout finalLayout) = 0;
        virtual void Read(void* data, size_t size, const glm::ivec3& position, const glm::uvec3& extent, ImageLayout initialLayout, ImageLayout finalLayout) = 0;

        virtual void* GetHandle() const = 0;
        virtual void* GetImageViewHandle() const = 0;
        virtual void* GetImageViewHandle(const ImageView& viewInfo, bool bForce2D = false) const = 0;

        // @view. You can use it to select the mipmap level and the array layer
        virtual ImageSubresourceLayout GetImageSubresourceLayout(ImageView view = {}) const = 0;

        // Returns the GPU memory usage
        size_t GetMemoryUsage() const;

        static Ref<Image> Create(ImageSpecifications specs, const std::string& debugName = "");
        static Ref<Image> Create(const Ref<Image>& image, const std::string& debugName = "") { Create(image->GetSpecs(), debugName); }

    private:
        void SetImageLayout(ImageLayout layout) { m_Specs.Layout = layout; }

    protected:
        ImageSpecifications m_Specs;
        std::string m_DebugName;
        // Revision is changed on each image recreation to update descriptors when an image is recreated.
        // It should work without it because internal handles are updated, but for some reason RenderDoc is not happy without it.
        // And it thinks textures are not bound to the pipeline. So, this exists purely to make RenderDoc happy and correctly display pass inputs
        // RenderDoc version: 1.42
        uint8_t m_Revision;
        bool bCalculateMipsCountInternally = false;

        friend class VulkanCommandManager;
        friend class VulkanCommandBuffer;
    };
}
