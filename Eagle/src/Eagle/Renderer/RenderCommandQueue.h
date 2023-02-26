#pragma once

namespace Eagle
{
	class RenderCommandQueue
	{
	public:
		typedef void (*RenderCommandFn)(void*);

		RenderCommandQueue();
		~RenderCommandQueue();

		void* Allocate(RenderCommandFn func, uint32_t size);
		void Execute();

	private:
		uint8_t* m_CommandBuffer = nullptr;
		uint8_t* m_CommandBufferPtr = nullptr;
		uint32_t m_CommandCount = 0;
	};
}
