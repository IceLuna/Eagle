#include "egpch.h"
#include "SortTask.h"

#include "Eagle/Renderer/VidWrappers/PipelineCompute.h"
#include "Eagle/Renderer/VidWrappers/RenderCommandManager.h"
#include "Eagle/Renderer/VidWrappers/Buffer.h"

#include "../../Eagle-Editor/assets/shaders/sort/common.h"

namespace Eagle
{
    static void CalculateScratchResourceSize(uint32_t maxNumKeys, uint32_t& scratchBufferSize, uint32_t& reduceScratchBufferSize)
    {
        uint32_t blockSize = FFX_PARALLELSORT_ELEMENTS_PER_THREAD * FFX_PARALLELSORT_THREADGROUP_SIZE;
        uint32_t numBlocks = FFX_DIVIDE_ROUNDING_UP(maxNumKeys, blockSize);
        uint32_t numReducedBlocks = FFX_DIVIDE_ROUNDING_UP(numBlocks, blockSize);

        scratchBufferSize = FFX_PARALLELSORT_SORT_BIN_COUNT * numBlocks * sizeof(uint32_t);
        reduceScratchBufferSize = FFX_PARALLELSORT_SORT_BIN_COUNT * numReducedBlocks * sizeof(uint32_t);
    }

    static void SetConstantAndDispatchData(uint32_t numKeys, uint32_t maxThreadGroups, SortData& constantBuffer, uint32_t& numThreadGroupsToRun, uint32_t& numReducedThreadGroupsToRun)
    {
        constantBuffer.NumKeys = numKeys;

        uint32_t BlockSize = FFX_PARALLELSORT_ELEMENTS_PER_THREAD * FFX_PARALLELSORT_THREADGROUP_SIZE;
        uint32_t NumBlocks = FFX_DIVIDE_ROUNDING_UP(numKeys, BlockSize);

        // Figure out data distribution
        numThreadGroupsToRun = maxThreadGroups;
        uint32_t BlocksPerThreadGroup = (NumBlocks / numThreadGroupsToRun);
        constantBuffer.NumThreadGroupsWithAdditionalBlocks = NumBlocks % numThreadGroupsToRun;

        if (NumBlocks < numThreadGroupsToRun)
        {
            BlocksPerThreadGroup = 1;
            numThreadGroupsToRun = NumBlocks;
            constantBuffer.NumThreadGroupsWithAdditionalBlocks = 0;
        }

        constantBuffer.NumThreadGroups = numThreadGroupsToRun;
        constantBuffer.NumBlocksPerThreadGroup = BlocksPerThreadGroup;

        // Calculate the number of thread groups to run for reduction (each thread group can process BlockSize number of entries)
        numReducedThreadGroupsToRun = FFX_PARALLELSORT_SORT_BIN_COUNT * ((BlockSize > numThreadGroupsToRun) ? 1 : (numThreadGroupsToRun + BlockSize - 1) / BlockSize);
        constantBuffer.NumReduceThreadgroupPerBin = numReducedThreadGroupsToRun / FFX_PARALLELSORT_SORT_BIN_COUNT;
        constantBuffer.NumScanValues = numReducedThreadGroupsToRun;	// The number of reduce thread groups becomes our scan count (as each thread group writes out 1 value that needs scan prefix)
    }

	SortTask::SortTask(uint32_t maxEntires, bool bHasPayload, bool bIndirect)
		: m_MaxEntries(maxEntires)
		, m_bHasPayload(bHasPayload)
		, m_bIndirect(bIndirect)
	{
        InitBuffers();
        InitPipelines();
	}

    void SortTask::RecordCommandBuffer(const Ref<CommandBuffer>& cmd, const Ref<Buffer>& keysBuffer, const Ref<Buffer>& numKeysToSortBuffer, uint32_t numKeysOffset, uint32_t numKeysToSort, const Ref<Buffer>& payloadBuffer)
    {
        const std::vector keys = { keysBuffer, m_SortScratchBuffer };
        const std::vector payloads = { payloadBuffer, m_PayloadScratchBuffer };

        SortData sortData;
        memset(&sortData, 0, sizeof(sortData));

        uint32_t numThreadGroupsToRun;
        uint32_t numReducedThreadGroupsToRun;
        if (!m_bIndirect)
        {
            SetConstantAndDispatchData(numKeysToSort, FFX_PARALLELSORT_MAX_THREADGROUPS_TO_RUN, sortData, numThreadGroupsToRun, numReducedThreadGroupsToRun);
        }

        // Set descriptors
        {
            // Sort - SetupIndirectArgs Pass
            if (m_bIndirect)
            {
                m_SetupIndirectArgsPass->SetBuffer(numKeysToSortBuffer, 0, 0);
                m_SetupIndirectArgsPass->SetBuffer(m_IndirectCountScatterArgsBuffer, 0, 1);
                m_SetupIndirectArgsPass->SetBuffer(m_IndirectReduceScanArgsBuffer, 0, 2);
                m_SetupIndirectArgsPass->SetBuffer(m_SortDataBuffer, 0, 3);
            }

            // Sort - Sum Pass
            m_SumPass->SetBuffer(m_ScratchBuffer, 0, 0);
            m_SumPass->SetBuffer(m_SortDataBuffer, 0, 1);
            m_SumPass->SetBufferArray(keys, 1, 0);

            // Sort - Reduce Pass
            m_ReducePass->SetBuffer(m_ScratchBuffer, 0, 0);
            m_ReducePass->SetBuffer(m_ReducedScratchBuffer, 0, 1);
            m_ReducePass->SetBuffer(m_SortDataBuffer, 0, 2);

            // Sort - Scan
            m_ScanPass->SetBuffer(m_ReducedScratchBuffer, 0, 0);
            m_ScanPass->SetBuffer(m_SortDataBuffer, 0, 1);

            // Sort - Scan Add
            m_ScanAddPass->SetBuffer(m_ScratchBuffer, 0, 0);
            m_ScanAddPass->SetBuffer(m_ReducedScratchBuffer, 0, 1);
            m_ScanAddPass->SetBuffer(m_SortDataBuffer, 0, 2);

            // Sort - Scatter
            m_ScatterPass->SetBuffer(m_ScratchBuffer, 0, 0);
            m_ScatterPass->SetBuffer(m_SortDataBuffer, 0, 1);
            m_ScatterPass->SetBufferArray(keys, 1, 0);
            if (m_bHasPayload)
            {
                m_ScatterPass->SetBufferArray(payloads, 2, 0);
            }
        }

        uint32_t pingpong = 0u;
        for (uint32_t i = 0; sortData.Shift < 32; sortData.Shift += FFX_PARALLELSORT_SORT_BITS_PER_PASS, ++i)
        {
            // Update the sort data buffer
            if (m_bIndirect)
            {
                struct PushData
                {
                    uint32_t MaxThreadGroups;
                    uint32_t Shift;
                    uint32_t NumKeysOffset;
                } pushData;
                pushData.MaxThreadGroups = FFX_PARALLELSORT_MAX_THREADGROUPS_TO_RUN;
                pushData.Shift = sortData.Shift;
                pushData.NumKeysOffset = numKeysOffset;

                cmd->Dispatch(m_SetupIndirectArgsPass, 1, 1, 1, &pushData);

                cmd->TransitionLayout(m_IndirectCountScatterArgsBuffer, m_IndirectCountScatterArgsBuffer->GetLayout(), BufferReadAccess::IndirectArgument);
                cmd->TransitionLayout(m_IndirectReduceScanArgsBuffer, m_IndirectReduceScanArgsBuffer->GetLayout(), BufferReadAccess::IndirectArgument);
                cmd->Barrier(m_SortDataBuffer);
            }
            else
            {
                cmd->Write(m_SortDataBuffer, &sortData, sizeof(sortData), 0, BufferLayoutType::Unknown, BufferLayoutType::StorageBuffer);
            }

            cmd->Barrier(m_ScratchBuffer);

            // Sort - Sum Pass
            if (m_bIndirect)
                cmd->DispatchIndirect(m_SumPass, m_IndirectCountScatterArgsBuffer, 0, &pingpong);
            else
                cmd->Dispatch(m_SumPass, numThreadGroupsToRun, 1, 1, &pingpong);

            cmd->Barrier(m_ScratchBuffer);
            cmd->Barrier(m_ReducedScratchBuffer);

            // Sort - Reduce Pass
            if (m_bIndirect)
                cmd->DispatchIndirect(m_ReducePass, m_IndirectReduceScanArgsBuffer, 0);
            else
                cmd->Dispatch(m_ReducePass, numReducedThreadGroupsToRun, 1, 1);

            cmd->Barrier(m_ReducedScratchBuffer);

            // Sort - Scan
            cmd->Dispatch(m_ScanPass, 1, 1, 1);
            cmd->Barrier(m_ScratchBuffer);
            cmd->Barrier(m_ReducedScratchBuffer);

            // Sort - Scan Add
            if (m_bIndirect)
                cmd->DispatchIndirect(m_ScanAddPass, m_IndirectReduceScanArgsBuffer, 0);
            else
                cmd->Dispatch(m_ScanAddPass, numReducedThreadGroupsToRun, 1, 1);

            cmd->Barrier(keys[0]);
            cmd->Barrier(keys[1]);
            cmd->Barrier(m_ScratchBuffer);
            if (m_bHasPayload)
            {
                cmd->Barrier(payloads[0]);
                cmd->Barrier(payloads[1]);
            }

            // Sort - Scatter
            if (m_bIndirect)
                cmd->DispatchIndirect(m_ScatterPass, m_IndirectCountScatterArgsBuffer, 0, &pingpong);
            else
                cmd->Dispatch(m_ScatterPass, numThreadGroupsToRun, 1, 1, &pingpong);

            cmd->Barrier(keys[1u - pingpong]);
            if (m_bHasPayload)
                cmd->Barrier(payloads[1u - pingpong]);

            if (m_bIndirect)
            {
                cmd->TransitionLayout(m_IndirectCountScatterArgsBuffer, BufferReadAccess::IndirectArgument, BufferLayoutType::StorageBuffer);
                cmd->TransitionLayout(m_IndirectReduceScanArgsBuffer, BufferReadAccess::IndirectArgument, BufferLayoutType::StorageBuffer);
                cmd->Barrier(m_SortDataBuffer);
            }

            pingpong = 1u - pingpong;
        }
    }

    void SortTask::InitBuffers()
    {
        uint32_t scratchBufferSize;
        uint32_t reducedScratchBufferSize;
        CalculateScratchResourceSize(m_MaxEntries, scratchBufferSize, reducedScratchBufferSize);

        {
            BufferSpecifications specs{};
            specs.Size = sizeof(uint32_t) * m_MaxEntries;
            specs.Usage = BufferUsage::StorageBuffer;
            specs.Layout = BufferLayoutType::StorageBuffer;
            m_SortScratchBuffer = Buffer::Create(specs, "Sort_SortScratchBuffer");
        }

        if (m_bHasPayload)
        {
            BufferSpecifications specs{};
            specs.Size = sizeof(uint32_t) * m_MaxEntries;
            specs.Usage = BufferUsage::StorageBuffer;
            specs.Layout = BufferLayoutType::StorageBuffer;
            m_PayloadScratchBuffer = Buffer::Create(specs, "Sort_PayloadScratchBuffer");
        }

        {
            BufferSpecifications specs{};
            specs.Size = scratchBufferSize;
            specs.Usage = BufferUsage::StorageBuffer;
            specs.Layout = BufferLayoutType::StorageBuffer;
            m_ScratchBuffer = Buffer::Create(specs, "Sort_ScratchBuffer");
        }

        {
            BufferSpecifications specs{};
            specs.Size = reducedScratchBufferSize;
            specs.Usage = BufferUsage::StorageBuffer;
            specs.Layout = BufferLayoutType::StorageBuffer;
            m_ReducedScratchBuffer = Buffer::Create(specs, "Sort_ReducedScratchBuffer");
        }

        if (m_bIndirect)
        {
            BufferSpecifications specs{};
            specs.Size = sizeof(uint32_t) * 3;
            specs.Usage = BufferUsage::StorageBuffer | BufferUsage::IndirectBuffer;
            specs.Layout = BufferLayoutType::StorageBuffer;
            m_IndirectCountScatterArgsBuffer = Buffer::Create(specs, "Sort_IndirectCountScatterArgsBuffer");
            m_IndirectReduceScanArgsBuffer = Buffer::Create(specs, "Sort_IndirectReduceScanArgsBuffer");
        }

        {
            BufferSpecifications specs{};
            specs.Size = sizeof(SortData);
            specs.Usage = BufferUsage::StorageBuffer | (m_bIndirect ? BufferUsage::None : BufferUsage::TransferDst);
            specs.Layout = BufferLayoutType::StorageBuffer;
            m_SortDataBuffer = Buffer::Create(specs, "Sort_IndirectDataBuffer");
        }
    }

    void SortTask::InitPipelines()
    {
        // Sort - SetupIndirectArgs Pass
        if (m_bIndirect)
        {
            PipelineComputeState state{};
            state.ComputeShader = Shader::Create("sort/setup_indirect_args.comp", ShaderType::Compute);
            m_SetupIndirectArgsPass = PipelineCompute::Create(state);
        }

        // Sort - Sum Pass
        {
            PipelineComputeState state{};
            state.ComputeShader = Shader::Create("sort/sum_pass.comp", ShaderType::Compute);
            m_SumPass = PipelineCompute::Create(state);
        }

        // Sort - Reduce Pass
        {
            PipelineComputeState state{};
            state.ComputeShader = Shader::Create("sort/reduce_pass.comp", ShaderType::Compute);
            m_ReducePass = PipelineCompute::Create(state);
        }

        // Sort - Scan
        {
            PipelineComputeState state{};
            state.ComputeShader = Shader::Create("sort/scan_pass.comp", ShaderType::Compute);
            m_ScanPass = PipelineCompute::Create(state);
        }

        // Sort - Scan Add
        {
            PipelineComputeState state{};
            state.ComputeShader = Shader::Create("sort/scan_add_pass.comp", ShaderType::Compute);
            m_ScanAddPass = PipelineCompute::Create(state);
        }

        // Sort - Scatter
        {
            ShaderDefines defines;
            if (m_bHasPayload)
                defines["FFX_PARALLELSORT_OPTION_HAS_PAYLOAD"] = "1";

            PipelineComputeState state{};
            state.ComputeShader = Shader::Create("sort/scatter_pass.comp", ShaderType::Compute, defines);
            m_ScatterPass = PipelineCompute::Create(state);
        }
    }
}
