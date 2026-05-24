#include "egpch.h"
#include "FXAATask.h"

#include "Eagle/Renderer/SceneRenderer.h"
#include "Eagle/Renderer/VidWrappers/RenderCommandManager.h"

#include "Eagle/Debug/CPUTimings.h"
#include "Eagle/Debug/GPUTimings.h"

namespace Eagle
{
	FXAATask::FXAATask(SceneRenderer& renderer)
		: RendererTask(renderer)
	{
		{
			PipelineComputeState state{};
			state.ComputeShader = Shader::Create("fxaa/fxaa.comp", ShaderType::Compute);

			m_Pipeline = PipelineCompute::Create(state);
		}

		{
			const glm::uvec2 size = m_Renderer.GetViewportSize();

			ImageSpecifications colorSpecs;
			colorSpecs.Format = m_Renderer.GetOutputImageSpecs().Format;
			colorSpecs.Layout = ImageLayoutType::StorageImage;
			colorSpecs.Size = { size.x, size.y, 1 };
			colorSpecs.Usage = ImageUsage::Storage | ImageUsage::TransferSrc;

			m_Image = Image::Create(colorSpecs, "FXAA");
		}
	}
	
	void FXAATask::RecordCommandBuffer(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "FXAA");
		EG_CPU_TIMING_SCOPED("FXAA");

		const auto& input = m_Renderer.GetOutput();
		if (m_Image->GetSize() != input->GetSize())
		{
			m_Image->Resize(input->GetSize());
		}

		m_Pipeline->SetImageSampler(input, Sampler::BilinearSamplerClamp, 0, 0);
		m_Pipeline->SetImage(m_Image, 0, 1);

		const ImageLayout oldInputLayout = input->GetLayout();
		cmd->TransitionLayout(input, oldInputLayout, ImageReadAccess::NonPixelShaderRead);

		const glm::uvec2 size = m_Renderer.GetViewportSize();
		const glm::uvec3 groupSize = m_Pipeline->GetWorkGroupSize();
		const glm::uvec2 numGroups = CalcNumGroups(size, groupSize);
		cmd->Dispatch(m_Pipeline, numGroups.x, numGroups.y, 1, &size);

		cmd->CopyImage(m_Image, input, input->GetLayout(), oldInputLayout);
	}
}
