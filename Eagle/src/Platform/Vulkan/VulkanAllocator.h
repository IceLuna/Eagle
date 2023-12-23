#pragma once

#include "vk_mem_alloc.h"
#include "Eagle/Renderer/RendererUtils.h"

namespace Eagle
{
	enum class MemoryType;

	// TODO: Add pools
	class VulkanAllocator
	{
	public:
		static void Init();
		static void Shutdown();

		[[nodiscard]] static VmaAllocation AllocateBuffer(const VkBufferCreateInfo* bufferCI, MemoryType usage, bool bSeparateAllocation, const std::string& debugName, VkBuffer* outBuffer);
		[[nodiscard]] static VmaAllocation AllocateImage(const VkImageCreateInfo* imageCI, MemoryType usage, bool bSeparateAllocation, const std::string& debugName, VkImage* outImage);

		static void DestroyImage(VkImage image, VmaAllocation allocation);
		static void DestroyBuffer(VkBuffer buffer, VmaAllocation allocation);

		static bool IsHostVisible(VmaAllocation allocation);
		[[nodiscard]] static void* MapMemory(VmaAllocation allocation);
		static void UnmapMemory(VmaAllocation allocation);
		static void FlushMemory(VmaAllocation allocation);

		static GPUMemoryStats GetStats();
	};
}
