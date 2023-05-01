#include "egpch.h"
#include "VulkanImage.h"

#include "VulkanUtils.h"
#include "VulkanFence.h"
#include "VulkanSemaphore.h"
#include "VulkanCommandManager.h"

#include "Eagle/Renderer/VidWrappers/StagingManager.h"

namespace Eagle
{
	VulkanImage::VulkanImage(const ImageSpecifications& specs, const std::string& debugName)
		: Image(specs, debugName)
	{
		assert(specs.Size.x > 0 && specs.Size.y > 0);

		m_Device = VulkanContext::GetDevice()->GetVulkanDevice();
		CreateImage();
		CreateImageView();
	}

	VulkanImage::VulkanImage(VkImage vulkanImage, const ImageSpecifications& specs, bool bOwns, const std::string& debugName)
		: Image(specs, debugName)
		, m_Image(vulkanImage)
		, m_bOwns(bOwns)
	{
		m_Device = VulkanContext::GetDevice()->GetVulkanDevice();
		m_VulkanFormat = ImageFormatToVulkan(m_Specs.Format);
		m_AspectMask = GetImageAspectFlags(m_VulkanFormat);
		CreateImageView();
	}

	VulkanImage::~VulkanImage()
	{
		Release();
	}

	VkImageAspectFlags VulkanImage::GetTransitionAspectMask(ImageLayout oldLayout, ImageLayout newLayout) const
	{
		VkImageLayout vkOldLayout = ImageLayoutToVulkan(oldLayout);
		VkImageLayout vkNewLayout = ImageLayoutToVulkan(newLayout);
		if (vkOldLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL && (vkNewLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL || vkNewLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL))
			return VK_IMAGE_ASPECT_DEPTH_BIT;

		return m_AspectMask;
	}

	void VulkanImage::CreateImage()
	{
		assert(m_bOwns);
		assert(m_Image == VK_NULL_HANDLE);

		if (bCalculateMipsCountInternally)
		{
			m_Specs.MipsCount = CalculateMipCount(m_Specs.Size);
		}

		m_VulkanFormat = ImageFormatToVulkan(m_Specs.Format);
		m_AspectMask = GetImageAspectFlags(m_VulkanFormat);
		VkImageCreateInfo info{};
		info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		info.imageType = ImageTypeToVulkan(m_Specs.Type);
		info.format = m_VulkanFormat;
		info.arrayLayers = m_Specs.bIsCube ? 6 : 1;
		info.extent = { m_Specs.Size.x, m_Specs.Size.y, m_Specs.Size.z };
		info.mipLevels = m_Specs.MipsCount;
		info.samples = GetVulkanSamplesCount(m_Specs.SamplesCount);
		info.tiling = m_Specs.MemoryType == MemoryType::Gpu ? VK_IMAGE_TILING_OPTIMAL : VK_IMAGE_TILING_LINEAR;
		info.usage = ImageUsageToVulkan(m_Specs.Usage);
		info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		info.flags |= m_Specs.bIsCube ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;

		// This is a workaround for Vulkan Memory Allocator issue
		// The problem here is when all MemoryType::kGpu memory is allocated, the Allocator begins to put images in memory of other MemoryTypes.
		// It leads to Image and and mappable Buffers to be bound to the same VkDeviceMemory.
		// When vmaMapMemory() is called, vkMapMemory() with called for VkDeviceMemory with size = VK_WHOLE_SIZE. 
		// It could lead to device lost if VkImage is not in layout VK_IMAGE_LAYOUT_GENERAL.
		// Workarounding the issue by allocating each VkImage in its own VkDeviceMemory, so that mapping buffers can't result in mapping memory bound to VkImage.
		constexpr bool separateAllocation = true;
		m_Allocation = VulkanAllocator::AllocateImage(&info, m_Specs.MemoryType, separateAllocation, m_DebugName, &m_Image);

		if (!m_DebugName.empty())
			VulkanContext::AddResourceDebugName(m_Image, m_DebugName, VK_OBJECT_TYPE_IMAGE);
	}

	void VulkanImage::CreateImageView()
	{
		assert(m_DefaultImageView == VK_NULL_HANDLE);
		m_DefaultImageView = (VkImageView)GetImageViewHandle({ 0, m_Specs.MipsCount, 0 });
	}

	void VulkanImage::Release()
	{
		RenderManager::SubmitResourceFree([views = std::move(m_Views), debugName = m_DebugName, device = m_Device, image = m_Image, allocation = m_Allocation, bOwns = m_bOwns]()
		{
			for (auto& view : views)
				vkDestroyImageView(device, view.second, nullptr);

			if (image)
			{
				if (!debugName.empty())
					VulkanContext::RemoveResourceDebugName(image);

				if (bOwns)
					VulkanAllocator::DestroyImage(image, allocation);
			}
		});

		m_Views.clear();
		m_DefaultImageView = VK_NULL_HANDLE;
		m_Image = VK_NULL_HANDLE;
	}

	void* VulkanImage::GetImageViewHandle(const ImageView& viewInfo, bool bForce2D) const
	{
		auto it = m_Views.find(viewInfo);
		if (it != m_Views.end())
			return it->second;

		if (!m_Image)
			return VK_NULL_HANDLE;
		
		// If force2D, set to 1, otherwise check if cube
		uint32_t layerCount = bForce2D ? 1 : (m_Specs.bIsCube ? VK_REMAINING_ARRAY_LAYERS : 1);

		VkImageView& imageView = m_Views[viewInfo];
		VkImageViewCreateInfo viewCI{};
		viewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewCI.image = m_Image;
		viewCI.format = m_VulkanFormat;
		viewCI.viewType = ImageTypeToVulkanImageViewType(m_Specs.Type, bForce2D ? false : m_Specs.bIsCube);
		viewCI.subresourceRange.aspectMask = m_AspectMask;
		viewCI.subresourceRange.baseMipLevel = viewInfo.MipLevel;
		viewCI.subresourceRange.baseArrayLayer = viewInfo.Layer;
		viewCI.subresourceRange.levelCount = viewInfo.MipLevels;
		viewCI.subresourceRange.layerCount = layerCount;
		VK_CHECK(vkCreateImageView(m_Device, &viewCI, nullptr, &imageView));

		return imageView;
	}

	void VulkanImage::Resize(const glm::uvec3& size)
	{
		assert(m_bOwns);

		if (m_Specs.Size == size)
			return;

		m_Specs.Size = size;

		Release();

		CreateImage();
		CreateImageView();

		if (m_Specs.Layout != ImageLayoutType::Unknown)
		{
			Ref<Image> image = shared_from_this();
			RenderManager::Submit([image, layout = m_Specs.Layout](Ref<CommandBuffer>& cmd) mutable
			{
				cmd->TransitionLayout(image, ImageLayoutType::Unknown, layout);
			});
		}
	}

	void* VulkanImage::Map()
	{
		assert(VulkanAllocator::IsHostVisible(m_Allocation));
		return VulkanAllocator::MapMemory(m_Allocation);
	}

	void VulkanImage::Unmap()
	{
		VulkanAllocator::UnmapMemory(m_Allocation);
	}
	
	void VulkanImage::Read(void* data, ImageLayout initialLayout, ImageLayout finalLayout)
	{
		Read(data, CalculateImageMemorySize(m_Specs.Format, m_Specs.Size), glm::ivec3{ 0 }, m_Specs.Size, initialLayout, finalLayout);
	}
	
	void VulkanImage::Read(void* data, size_t size, const glm::ivec3& position, const glm::uvec3& extent, ImageLayout initialLayout, ImageLayout finalLayout)
	{
		EG_CORE_ASSERT(HasUsage(ImageUsage::TransferSrc));

		Ref<CommandBuffer> cmd = RenderManager::AllocateCommandBuffer(true);
		Ref<VulkanCommandBuffer> vkCmd = Cast<VulkanCommandBuffer>(cmd);
		Ref<StagingBuffer> stagingBuffer = StagingManager::AcquireBuffer(size, true);
		vkCmd->m_UsedStagingBuffers.insert(stagingBuffer.get());

		Ref<Image> thisShared = shared_from_this();
		if (initialLayout != ImageReadAccess::CopySource)
		{
			cmd->TransitionLayout(thisShared, initialLayout, ImageReadAccess::CopySource);
		}

		VkBufferImageCopy copyRegion = {};
		copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copyRegion.imageSubresource.mipLevel = 0;
		copyRegion.imageSubresource.baseArrayLayer = 0;
		copyRegion.imageSubresource.layerCount = 1;
		copyRegion.imageOffset = VkOffset3D{ position.x, position.y, position.z };
		copyRegion.imageExtent = VkExtent3D{ std::uint32_t(extent.x), std::uint32_t(extent.y), std::uint32_t(extent.z) };

		VkBuffer vkStagingBuffer = (VkBuffer)stagingBuffer->GetBuffer()->GetHandle();

		vkCmdCopyImageToBuffer((VkCommandBuffer)cmd->GetHandle(), m_Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, vkStagingBuffer, 1u, &copyRegion);

		if (finalLayout != ImageReadAccess::CopySource)
		{
			cmd->TransitionLayout(thisShared, ImageReadAccess::CopySource, finalLayout);
		}

		cmd->End();
		RenderManager::SubmitCommandBuffer(cmd, true);

		void* mapped = stagingBuffer->Map();
		memcpy(data, mapped, size);
		stagingBuffer->Unmap();
	}
}
