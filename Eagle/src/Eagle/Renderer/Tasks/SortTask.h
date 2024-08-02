#pragma once

namespace Eagle
{
	class Buffer;
	class PipelineCompute;
	class CommandBuffer;

	class SortTask
	{
	public:
		SortTask(uint32_t maxEntires, bool bHasPayload, bool bIndirect);

		// @numKeysToSortBuffer and @numKeysToSort. One of these values must be set.
		// If `bIndirect` is set, `numKeysToSortBuffer` is used, otherwise `numKeysToSort`
		// @payloadBuffer. Optional. Must be provided if `bHasPayload` is enabled
		void RecordCommandBuffer(const Ref<CommandBuffer>& cmd, const Ref<Buffer>& keysBuffer,
			const Ref<Buffer>& numKeysToSortBuffer = nullptr, uint32_t numKeysOffset = 0, uint32_t numKeysToSort = 0, const Ref<Buffer>& payloadBuffer = nullptr);

	private:
		void InitBuffers();
		void InitPipelines();

	private:
		Ref<PipelineCompute> m_SetupIndirectArgsPass;
		Ref<PipelineCompute> m_SumPass;
		Ref<PipelineCompute> m_ReducePass;
		Ref<PipelineCompute> m_ScanPass;
		Ref<PipelineCompute> m_ScanAddPass;
		Ref<PipelineCompute> m_ScatterPass;

		Ref<Buffer> m_SortScratchBuffer;
		Ref<Buffer> m_PayloadScratchBuffer;
		Ref<Buffer> m_ScratchBuffer;
		Ref<Buffer> m_ReducedScratchBuffer;
		Ref<Buffer> m_IndirectCountScatterArgsBuffer;
		Ref<Buffer> m_IndirectReduceScanArgsBuffer;
		Ref<Buffer> m_SortDataBuffer;

		uint32_t m_MaxEntries = 0u;
		bool m_bHasPayload = false;
		bool m_bIndirect = false;
	};
}
