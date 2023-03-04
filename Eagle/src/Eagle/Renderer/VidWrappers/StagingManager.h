#pragma once

#include "Fence.h"
#include "Buffer.h"

namespace Eagle
{
	enum class StagingBufferState
	{
		Free, // Not used
		Pending, // Used in cmd buffer but wasn't submitted yet
		InFlight, // Submitted
	};

	class StagingBuffer
	{
	protected:
		StagingBuffer(size_t size, bool bIsCPURead);

	public:
		virtual ~StagingBuffer() = default;

		[[nodiscard]] void* Map() { return m_Buffer->Map(); }
		void Unmap() { m_Buffer->Unmap(); }

		void SetState(StagingBufferState state) { m_State = state; }
		void SetFence(const Ref<Fence>& fence) { m_Fence = fence; }
		Ref<Fence>& GetFence() { return m_Fence; }
		const Ref<Fence>& GetFence() const { return m_Fence; }

		StagingBufferState GetState() const { return m_State; }
		size_t GetSize() const { return m_Buffer->GetSize(); }
		bool IsCPURead() const { return m_bIsCPURead; }

		Ref<Buffer>& GetBuffer() { return m_Buffer; }
		const Ref<Buffer>& GetBuffer() const { return m_Buffer; }

	protected:
		Ref<Buffer> m_Buffer = nullptr;
		Ref<Fence> m_Fence = nullptr;
		uint64_t m_FrameNumberUsed = 0;
		StagingBufferState m_State = StagingBufferState::Free;
		bool m_bIsCPURead = false;

		friend class StagingManager;
	};

	class StagingManager
	{
	public:
		StagingManager() = delete;

		static Ref<StagingBuffer>& AcquireBuffer(size_t size, bool bIsCPURead);
		static void ReleaseBuffers();
		static void NextFrame();

	private:
		static constexpr uint64_t s_ReleaseAfterNFrames = 1024;
	};
}
