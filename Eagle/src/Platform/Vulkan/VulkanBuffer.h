#pragma once

#include "Eagle/Core/DataBuffer.h"
#include "Eagle/Renderer/VidWrappers/Buffer.h"
#include "VulkanAllocator.h"

namespace Eagle
{
	class VulkanBuffer : public Buffer
	{
	public:
		VulkanBuffer(const BufferSpecifications& specs, const std::string& debugName = "");

		virtual ~VulkanBuffer()
		{
			Release();
		}

		virtual void Resize(size_t size) override;

		[[nodiscard]] void* Map() override
		{
			assert(VulkanAllocator::IsHostVisible(m_Allocation));
			return VulkanAllocator::MapMemory(m_Allocation);
		}
		void Unmap() override
		{
			if (m_Specs.MemoryType == MemoryType::Cpu || m_Specs.MemoryType == MemoryType::CpuToGpu)
				VulkanAllocator::FlushMemory(m_Allocation);
			VulkanAllocator::UnmapMemory(m_Allocation);
		}

		void* GetHandle() const override { return m_Buffer; }

	private:
		void Create();
		void Release();

	private:
		VkBuffer m_Buffer = VK_NULL_HANDLE;
		VmaAllocation m_Allocation = VK_NULL_HANDLE;
	};
}
