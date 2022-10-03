#include "egpch.h"
#include "VulkanBuffer.h"
#include "VulkanContext.h"
#include "VulkanUtils.h"

namespace Eagle
{
	VulkanBuffer::VulkanBuffer(const BufferSpecifications& specs, const std::string& debugName)
		: Buffer(specs, debugName)
	{
		VkBufferCreateInfo info{};
		info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		info.size = specs.Size;
		info.usage = BufferUsageToVulkan(specs.Usage);

		m_Allocation = VulkanAllocator::AllocateBuffer(&info, specs.MemoryType, false, debugName, &m_Buffer);

		if (!m_DebugName.empty())
			VulkanContext::AddResourceDebugName(m_Buffer, m_DebugName, VK_OBJECT_TYPE_BUFFER);
	}

	void VulkanBuffer::Release()
	{
		Renderer::SubmitResourceFree([buffer = m_Buffer, debugName = m_DebugName, allocation = m_Allocation]()
		{
			VulkanAllocator::DestroyBuffer(buffer, allocation);
			if (!debugName.empty())
				VulkanContext::RemoveResourceDebugName(buffer);
		});

		m_Buffer = VK_NULL_HANDLE;
		m_Allocation = VK_NULL_HANDLE;
	}
}
