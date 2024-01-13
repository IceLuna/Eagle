#include "egpch.h"
#include "VulkanAllocator.h"
#include "VulkanDevice.h"
#include "VulkanContext.h"
#include "VulkanImage.h"

namespace Eagle
{
    namespace Utils
    {
        static VmaMemoryUsage MemoryTypeToVmaUsage(MemoryType memory_type)
        {
            switch (memory_type)
            {
                case MemoryType::Gpu: return VMA_MEMORY_USAGE_GPU_ONLY;
                case MemoryType::Cpu: return VMA_MEMORY_USAGE_CPU_ONLY;
                case MemoryType::CpuToGpu: return VMA_MEMORY_USAGE_CPU_TO_GPU;
                case MemoryType::GpuToCpu: return VMA_MEMORY_USAGE_GPU_TO_CPU;
                default:
                    EG_CORE_ASSERT(!"Unknown memory type");
                    return VMA_MEMORY_USAGE_UNKNOWN;
            }
        }
    }

    struct VulkanAllocatorData
    {
        const VulkanDevice* Device;
        VmaAllocator Allocator;
        uint64_t TotalAllocatedBytes = 0;
        uint64_t TotalFreedBytes = 0;
    };

    static VulkanAllocatorData* s_AllocatorData = nullptr;
	static std::unordered_map<VmaAllocation, GPUResourceDebugData> s_Allocations;

#ifdef EG_WITH_EDITOR
	static std::mutex s_Mutex;
#endif

	void VulkanAllocator::Init()
	{
		const VulkanDevice* device = VulkanContext::GetDevice();
		s_AllocatorData = new VulkanAllocatorData;
		s_AllocatorData->Device = device;

		VmaAllocatorCreateInfo ci{};
		ci.vulkanApiVersion = VulkanContext::GetVulkanAPIVersion();
		ci.physicalDevice = device->GetPhysicalDevice()->GetVulkanPhysicalDevice();
		ci.device = device->GetVulkanDevice();
		ci.instance = VulkanContext::GetInstance();

		vmaCreateAllocator(&ci, &s_AllocatorData->Allocator);
	}

	void VulkanAllocator::Shutdown()
	{
		for (auto& [allocation, data] : s_Allocations)
			EG_CORE_ERROR("Vulkan: Memory leak: {}", data.Name);

		vmaDestroyAllocator(s_AllocatorData->Allocator);

		if (s_AllocatorData->TotalAllocatedBytes != s_AllocatorData->TotalFreedBytes)
		{
			EG_RENDERER_CRITICAL("[Vulkan allocator] Memory leak detected! Totally allocated bytes = {0}; Totally freed bytes = {1}",
				s_AllocatorData->TotalAllocatedBytes, s_AllocatorData->TotalFreedBytes);
		}

		delete s_AllocatorData;
		s_AllocatorData = nullptr;
	}

	VmaAllocation VulkanAllocator::AllocateBuffer(const VkBufferCreateInfo* bufferCI, MemoryType usage, bool bSeparateAllocation, const std::string& debugName, VkBuffer* outBuffer)
	{
		VmaAllocationCreateInfo ci{};
		ci.usage = Utils::MemoryTypeToVmaUsage(usage);
		ci.flags = bSeparateAllocation ? VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT : 0;

		VmaAllocation allocation;
		vmaCreateBuffer(s_AllocatorData->Allocator, bufferCI, &ci, outBuffer, &allocation, nullptr);

		VmaAllocationInfo allocationInfo{};
		vmaGetAllocationInfo(s_AllocatorData->Allocator, allocation, &allocationInfo);
		s_AllocatorData->TotalAllocatedBytes += allocationInfo.size;

		{
#ifdef EG_WITH_EDITOR
			std::scoped_lock lock(s_Mutex);
#endif
			s_Allocations[allocation] = { debugName, allocationInfo.size };
		}

		return allocation;
	}

	VmaAllocation VulkanAllocator::AllocateImage(const VkImageCreateInfo* imageCI, MemoryType usage, bool bSeparateAllocation, const std::string& debugName, VkImage* outImage)
	{
		VmaAllocationCreateInfo ci{};
		ci.usage = Utils::MemoryTypeToVmaUsage(usage);
		ci.flags = bSeparateAllocation ? VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT : 0;

		VmaAllocation allocation;
		vmaCreateImage(s_AllocatorData->Allocator, imageCI, &ci, outImage, &allocation, nullptr);

		VmaAllocationInfo allocationInfo{};
		vmaGetAllocationInfo(s_AllocatorData->Allocator, allocation, &allocationInfo);
		s_AllocatorData->TotalAllocatedBytes += allocationInfo.size;
		
		{
#ifdef EG_WITH_EDITOR
			std::scoped_lock lock(s_Mutex);
#endif
			s_Allocations[allocation] = { debugName, allocationInfo.size };
		}

		return allocation;
	}

	void VulkanAllocator::DestroyImage(VkImage image, VmaAllocation allocation)
	{
		VmaAllocationInfo allocationInfo{};
		vmaGetAllocationInfo(s_AllocatorData->Allocator, allocation, &allocationInfo);
		s_AllocatorData->TotalFreedBytes += allocationInfo.size;

		vmaDestroyImage(s_AllocatorData->Allocator, image, allocation);

		{
#ifdef EG_WITH_EDITOR
			std::scoped_lock lock(s_Mutex);
#endif
			s_Allocations.erase(allocation);
		}
	}

	void VulkanAllocator::DestroyBuffer(VkBuffer buffer, VmaAllocation allocation)
	{
		VmaAllocationInfo allocationInfo{};
		vmaGetAllocationInfo(s_AllocatorData->Allocator, allocation, &allocationInfo);
		s_AllocatorData->TotalFreedBytes += allocationInfo.size;

		vmaDestroyBuffer(s_AllocatorData->Allocator, buffer, allocation);

		{
#ifdef EG_WITH_EDITOR
			std::scoped_lock lock(s_Mutex);
#endif
			s_Allocations.erase(allocation);
		}
	}

	bool VulkanAllocator::IsHostVisible(VmaAllocation allocation)
	{
		VmaAllocationInfo allocationInfo = {};
		vmaGetAllocationInfo(s_AllocatorData->Allocator, allocation, &allocationInfo);

		VkMemoryPropertyFlags memFlags = 0;
		vmaGetMemoryTypeProperties(s_AllocatorData->Allocator, allocationInfo.memoryType, &memFlags);
		return (memFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0;
	}

	void* VulkanAllocator::MapMemory(VmaAllocation allocation)
	{
		void* mappedMemory;
		vmaMapMemory(s_AllocatorData->Allocator, allocation, (void**)&mappedMemory);
		return mappedMemory;
	}

	void VulkanAllocator::UnmapMemory(VmaAllocation allocation)
	{
		vmaUnmapMemory(s_AllocatorData->Allocator, allocation);
	}

	void VulkanAllocator::FlushMemory(VmaAllocation allocation)
	{
		vmaFlushAllocation(s_AllocatorData->Allocator, allocation, 0, VK_WHOLE_SIZE);
	}

	GPUMemoryStats VulkanAllocator::GetStats()
	{
#ifndef EG_WITH_EDITOR
		return {};
#else
		const auto& memoryProps = s_AllocatorData->Device->GetPhysicalDevice()->GetMemoryProperties();
		std::vector<VmaBudget> budgets(memoryProps.memoryHeapCount);
		vmaGetHeapBudgets(s_AllocatorData->Allocator, budgets.data());

		uint64_t usage = 0;
		uint64_t budget = 0;

		for (VmaBudget& b : budgets)
		{
			usage += b.usage;
			budget += b.budget;
		}

		GPUMemoryStats result;
		result.Used = usage;
		result.Free = budget - usage;
		result.Resources.reserve(s_Allocations.size());

		{
			std::scoped_lock lock(s_Mutex);
			for (auto& [vma, data] : s_Allocations)
				result.Resources.push_back(data);
		}

		return result;
#endif
	}
}
