#include "egpch.h"
#include "RenderCommandQueue.h"

namespace Eagle
{
	static constexpr size_t s_Mb = 10;
	static constexpr size_t s_TotalBytes = s_Mb * 1024 * 1024;

	RenderCommandQueue::RenderCommandQueue()
	{
		m_CommandBuffer = new uint8_t[s_TotalBytes];
		m_CommandBufferPtr = m_CommandBuffer;
		memset(m_CommandBuffer, 0, s_TotalBytes);
	}

	RenderCommandQueue::~RenderCommandQueue()
	{
		delete[] m_CommandBuffer;
	}

	void* RenderCommandQueue::Allocate(RenderCommandFn func, uint32_t size)
	{
		*(RenderCommandFn*)m_CommandBufferPtr = func;
		m_CommandBufferPtr += sizeof(func);

		*(uint32_t*)m_CommandBufferPtr = size;
		m_CommandBufferPtr += sizeof(uint32_t);

		void* mem = m_CommandBufferPtr;
		m_CommandBufferPtr += size;
		++m_CommandCount;

		EG_CORE_ASSERT(m_CommandBufferPtr - m_CommandBuffer < s_TotalBytes, "Buffer overflow");

		return mem;
	}

	void RenderCommandQueue::Execute()
	{
		uint8_t* buf = m_CommandBuffer;

		for (uint32_t i = 0; i < m_CommandCount; ++i)
		{
			RenderCommandFn func = *(RenderCommandFn*)buf;
			buf += sizeof(RenderCommandFn);

			uint32_t size = *(uint32_t*)buf;
			buf += sizeof(uint32_t);

			func(buf);
			buf += size;
		}

		m_CommandCount = 0;
		m_CommandBufferPtr = m_CommandBuffer;
	}
}
