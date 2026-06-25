#include "egpch.h"
#include "HZBTask.h"

#include "Eagle/Renderer/SceneRenderer.h"
#include "Eagle/Renderer/VidWrappers/RenderCommandManager.h"
#include "Eagle/Renderer/VidWrappers/Image.h"

#include "Eagle/Debug/CPUTimings.h"
#include "Eagle/Debug/GPUTimings.h"

namespace Eagle
{
	HZBTask::HZBTask(SceneRenderer& renderer)
		: RendererTask(renderer)
	{
        const auto size = m_Renderer.GetViewportSize();

        PipelineComputeState state{};
        state.ComputeShader = Shader::Create("hzb.comp", ShaderType::Compute);
        m_Pipeline = PipelineCompute::Create(state);

        m_HZBMipViews.resize(16);
        std::fill(m_HZBMipViews.begin(), m_HZBMipViews.end(), ImageView{});

        const uint32_t mipsCount = m_Renderer.GetGBuffer().HZB->GetMipsCount();
        for (uint32_t mip = 0; mip < mipsCount; ++mip)
            m_HZBMipViews[mip] = ImageView{ mip };

        BufferSpecifications bufferSpecs{};
        bufferSpecs.Usage = BufferUsage::TransferDst | BufferUsage::TransferSrc;
        bufferSpecs.Size = CalculateImageMemorySize(m_Renderer.GetGBuffer().Depth->GetFormat(), size.x, size.y);
        m_TempDepthCopy = Buffer::Create(bufferSpecs, "HZB. Temp Depth Copy");
	}

	void HZBTask::RecordCommandBuffer(const Ref<CommandBuffer>& cmd)
	{
        EG_GPU_TIMING_SCOPED(cmd, "HZB generation");
        EG_CPU_TIMING_SCOPED("HZB generation");

        const auto& gbuffer = m_Renderer.GetGBuffer();
        const auto& hzb = gbuffer.HZB;
        const auto& hzbSampler = gbuffer.HZBSampler;

        const uint32_t mipCount = hzb->GetMipsCount();
        const glm::uvec2 inputSize = hzb->GetSize();
        glm::uvec2 mipSize = inputSize;

        struct PushConstants
        {
            glm::uvec2 Size;
            glm::uvec2 PrevSize;
            int PrevMipLevel;
        } pushData;

        const glm::ivec2 size = m_Renderer.GetViewportSize();
        {
            auto& depth = m_Renderer.GetGBuffer().Depth;
            std::vector<BufferImageCopy> copyRegion(1);
            copyRegion[0].ImageExtent = glm::uvec3(size, 1u);

            const ImageLayout srcOldLayout = depth->GetLayout();

            cmd->TransitionLayout(depth, srcOldLayout, ImageReadAccess::CopySource);
            cmd->TransitionLayout(m_TempDepthCopy, m_TempDepthCopy->GetLayout(), BufferLayoutType::CopyDest);
            cmd->CopyImageToBuffer(depth, m_TempDepthCopy, copyRegion);
            cmd->TransitionLayout(m_TempDepthCopy, BufferLayoutType::CopyDest, BufferReadAccess::CopySource);
            cmd->TransitionLayout(depth, ImageReadAccess::CopySource, srcOldLayout);

            cmd->TransitionLayout(hzb, hzb->GetLayout(), ImageLayoutType::CopyDest);
            cmd->CopyBufferToImage(m_TempDepthCopy, hzb, copyRegion);
            cmd->TransitionLayout(hzb, ImageLayoutType::CopyDest, ImageLayoutType::StorageImage);
        }

        m_Pipeline->SetImageSampler(hzb, hzbSampler, 0, 0);
        m_Pipeline->SetImageArray(hzb, m_HZBMipViews, 0, 1);

        auto& stats = m_Renderer.GetStats();
        for (uint32_t mip = 1; mip < mipCount - 1; ++mip)
        {
            pushData.PrevMipLevel = mip - 1;
            pushData.PrevSize = mipSize;
            mipSize >>= 1u;
            pushData.Size = mipSize;

            const glm::uvec3 groupSize = m_Pipeline->GetWorkGroupSize();
            const glm::uvec2 numGroups = CalcNumGroups(mipSize, groupSize);
            if (glm::min(numGroups.x, numGroups.y) == 0)
                break;

            cmd->Dispatch(m_Pipeline, numGroups.x, numGroups.y, 1, &pushData);
            cmd->TransitionLayout(hzb, m_HZBMipViews[mip - 1], ImageLayoutType::StorageImage, ImageLayoutType::StorageImage);
            cmd->TransitionLayout(hzb, m_HZBMipViews[mip], ImageLayoutType::StorageImage, ImageLayoutType::StorageImage);
            ++stats.Dispatches;
        }

        cmd->TransitionLayout(hzb, ImageLayoutType::StorageImage, ImageReadAccess::PixelShaderRead);
    }

	void HZBTask::OnResize(const glm::uvec2 size)
	{
        m_TempDepthCopy->Resize(CalculateImageMemorySize(m_Renderer.GetGBuffer().Depth->GetFormat(), size.x, size.y));
        m_HZBMipViews.resize(16);
        std::fill(m_HZBMipViews.begin(), m_HZBMipViews.end(), ImageView{});

        const uint32_t mipsCount = m_Renderer.GetGBuffer().HZB->GetMipsCount();
        for (uint32_t mip = 0; mip < mipsCount; ++mip)
            m_HZBMipViews[mip] = ImageView{ mip };
	}
}
