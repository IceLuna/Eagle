#include "egpch.h"
#include "TAATask.h"

#include "Eagle/Renderer/SceneRenderer.h"
#include "Eagle/Renderer/VidWrappers/RenderCommandManager.h"

#include "Eagle/Debug/CPUTimings.h"
#include "Eagle/Debug/GPUTimings.h"

namespace Eagle
{
	TAATask::TAATask(SceneRenderer& renderer)
		: RendererTask(renderer)
	{
		m_FinalImage = m_Renderer.GetHDROutput();
		InitPipeline();
	}
	
	void TAATask::RecordCommandBuffer(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "TAA");
		EG_CPU_TIMING_SCOPED("TAA");

		struct PushData
		{
			glm::uvec2 Size;
			glm::vec2 TexelSize;
		} pushData;
		pushData.Size = m_FinalImage->GetSize();
		pushData.TexelSize = 1.f / glm::vec2(pushData.Size);

		constexpr uint32_t tileSize = 8;
		const auto& size = pushData.Size;
		glm::uvec2 numGroups = { glm::ceil(size.x / float(tileSize)), glm::ceil(size.y / float(tileSize)) };

		if (!m_HistoryImage)
		{
			ImageSpecifications colorSpecs;
			colorSpecs.Format = m_FinalImage->GetFormat();
			colorSpecs.Size = { size.x, size.y, 1 };
			colorSpecs.Usage = ImageUsage::Sampled | ImageUsage::TransferDst;
			m_HistoryImage = Image::Create(colorSpecs, "TAA_History");

			colorSpecs.Usage = ImageUsage::Storage | ImageUsage::TransferSrc;
			m_Result = Image::Create(colorSpecs, "TAA_Result");

			cmd->TransitionLayout(m_HistoryImage, ImageLayoutType::Unknown, ImageReadAccess::PixelShaderRead);
			cmd->TransitionLayout(m_Result, ImageLayoutType::Unknown, ImageLayoutType::StorageImage);
			cmd->CopyImage(m_FinalImage, ImageView{}, m_HistoryImage, ImageView{}, glm::ivec3{ 0 }, glm::ivec3{ 0 }, m_FinalImage->GetSize());
		}

		m_Pipeline->SetImageSampler(m_HistoryImage, Sampler::BilinearSampler, 0, 0);
		m_Pipeline->SetImageSampler(m_Renderer.GetGBuffer().Motion, Sampler::PointSampler, 0, 1);
		m_Pipeline->SetImage(m_FinalImage, 0, 2);
		m_Pipeline->SetImage(m_Result, 0, 3);

		const ImageLayout oldLayout = m_FinalImage->GetLayout();
		cmd->TransitionLayout(m_FinalImage, oldLayout, ImageLayoutType::StorageImage);

		cmd->Dispatch(m_Pipeline, numGroups.x, numGroups.y, 1, &pushData);

		cmd->TransitionLayout(m_FinalImage, ImageLayoutType::StorageImage, oldLayout);
		cmd->CopyImage(m_Result, ImageView{}, m_FinalImage, ImageView{}, glm::ivec3{0}, glm::ivec3{0}, m_FinalImage->GetSize());
		cmd->CopyImage(m_Result, ImageView{}, m_HistoryImage, ImageView{}, glm::ivec3{0}, glm::ivec3{0}, m_FinalImage->GetSize());
	}
	
	void TAATask::InitPipeline()
	{
		PipelineComputeState state;
		state.ComputeShader = Shader::Create("assets/shaders/taa.comp", ShaderType::Compute);
		m_Pipeline = PipelineCompute::Create(state);
	}
}
