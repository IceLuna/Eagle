#pragma once

#include "Eagle/Renderer/Image.h"
#include "Vulkan.h"
#include "VulkanAllocator.h"
#include "VulkanContext.h"

namespace Eagle
{
    class VulkanImage : public Image
    {
    public:
        VulkanImage(const ImageSpecifications& specs, const std::string& debugName = "");
        VulkanImage(VkImage vulkanImage, const ImageSpecifications& specs, bool bOwns, const std::string& debugName = "");
        virtual ~VulkanImage();

        void* GetHandle() const override { return m_Image; }

        void* GetImageViewHandle() const override { return m_DefaultImageView; }
        void* GetImageViewHandle(const ImageView& viewInfo) const override;

        void Resize(const glm::uvec3& size) override;
        [[nodiscard]] void* Map() override;
        void Unmap() override;

        void Read(void* data, ImageLayout initialLayout, ImageLayout finalLayout) override;
        void Read(void* data, size_t size, const glm::ivec3& position, const glm::uvec3& extent, ImageLayout initialLayout, ImageLayout finalLayout) override;

        VkImageAspectFlags GetDefaultAspectMask() const { return m_AspectMask; }
        VkImageAspectFlags GetTransitionAspectMask(ImageLayout oldLayout, ImageLayout newLayout) const;
        VkFormat GetVulkanFormat() const { return m_VulkanFormat; }

    private:
        void CreateImage();
        void CreateImageView();

        void Release();

    private:
        mutable std::unordered_map<ImageView, VkImageView> m_Views; // Mutable by `GetVulkanImageView(const ImageView&)`

        VkDevice m_Device = VK_NULL_HANDLE;
        VkImage m_Image = VK_NULL_HANDLE;
        VkImageView m_DefaultImageView = VK_NULL_HANDLE;
        VmaAllocation m_Allocation = VK_NULL_HANDLE;
        VkFormat m_VulkanFormat = VK_FORMAT_UNDEFINED;
        VkImageAspectFlags m_AspectMask;
        bool m_bOwns = true;

        friend class VulkanCommandBuffer;
    };
}
