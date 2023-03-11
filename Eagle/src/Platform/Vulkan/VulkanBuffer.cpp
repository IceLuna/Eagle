#include "egpch.h"
#include "VulkanBuffer.h"
#include "VulkanContext.h"
#include "VulkanUtils.h"

namespace Eagle
{
	VulkanBuffer::VulkanBuffer(const BufferSpecifications& specs, const std::string& debugName)
		: Buffer(specs, debugName)
	{
		Create();
	}

	void VulkanBuffer::Resize(size_t size)
	{
		m_Specs.Size = size;
		Release();
		Create();
	}

	void VulkanBuffer::Create()
	{
		EG_CORE_ASSERT(m_Buffer == VK_NULL_HANDLE);

		VkBufferCreateInfo info{};
		info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		info.size = m_Specs.Size;
		info.usage = BufferUsageToVulkan(m_Specs.Usage);

		m_Allocation = VulkanAllocator::AllocateBuffer(&info, m_Specs.MemoryType, false, m_DebugName, &m_Buffer);

		if (!m_DebugName.empty())
			VulkanContext::AddResourceDebugName(m_Buffer, m_DebugName, VK_OBJECT_TYPE_BUFFER);
	}

	void VulkanBuffer::Release()
	{
		RenderManager::SubmitResourceFree([buffer = m_Buffer, debugName = m_DebugName, allocation = m_Allocation]()
		{
			VulkanAllocator::DestroyBuffer(buffer, allocation);
			if (!debugName.empty())
				VulkanContext::RemoveResourceDebugName(buffer);
		});

		m_Buffer = VK_NULL_HANDLE;
		m_Allocation = VK_NULL_HANDLE;
	}
}
