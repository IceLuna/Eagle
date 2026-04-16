#include "egpch.h"
#include "FogPassTask.h"

#include "Eagle/Renderer/SceneRenderer.h"
#include "Eagle/Renderer/VidWrappers/RenderCommandManager.h"

#include "Eagle/Debug/CPUTimings.h"
#include "Eagle/Debug/GPUTimings.h"

namespace Eagle
{
	FogPassTask::FogPassTask(SceneRenderer& renderer)
		: RendererTask(renderer)
	{
		InitPipeline();

		BufferSpecifications specs;
		specs.Size = sizeof(FogData);
		specs.Usage = BufferUsage::TransferDst | BufferUsage::UniformBuffer;
		m_FogDataBuffer = Buffer::Create(specs, "FogData");
	}

	void FogPassTask::RecordCommandBuffer(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "Fog pass");
		EG_CPU_TIMING_SCOPED("Fog pass");

		const auto& options = m_Renderer.GetOptions_RT();
		const auto& fogOptions = options.FogSettings;
		const auto& input = m_Renderer.GetHDROutput();

		const glm::uvec2 size = input->GetSize();
		const glm::uvec3 groupSize = m_Pipeline->GetWorkGroupSize();
		const glm::uvec2 numGroups = CalcNumGroups(size, groupSize);

		struct PushData
		{
			glm::mat4 InvProjMat;
			glm::ivec2 Size;
			glm::vec2 TexelSize;
		} pushData;
		static_assert(sizeof(PushData) <= 128);
		pushData.InvProjMat = glm::inverse(m_Renderer.GetProjectionMatrix());
		pushData.Size = size;
		pushData.TexelSize = 1.f / glm::vec2(size);

		{
			m_FogData.FogColor = fogOptions.Color;
			m_FogData.FogMin = fogOptions.MinDistance;
			m_FogData.FogMax = fogOptions.MaxDistance;
			m_FogData.Density = fogOptions.Density;
			m_FogData.FogEquation = fogOptions.Equation;
			cmd->Write(m_FogDataBuffer, &m_FogData, sizeof(FogData), 0, m_FogDataBuffer->GetLayout(), BufferReadAccess::Uniform);
		}

		const auto& depth = m_Renderer.GetGBuffer().Depth;
		m_Pipeline->SetImage(input, 0, 0);
		m_Pipeline->SetImageSampler(depth, Sampler::PointSampler, 0, 1);
		m_Pipeline->SetBuffer(m_FogDataBuffer, 0, 2);

		const ImageLayout inputOldLayout = input->GetLayout();
		const ImageLayout oldDepthLayout = depth->GetLayout();

		cmd->TransitionLayout(depth, oldDepthLayout, ImageReadAccess::PixelShaderRead);
		cmd->TransitionLayout(input, inputOldLayout, ImageLayoutType::StorageImage);
		cmd->Dispatch(m_Pipeline, numGroups.x, numGroups.y, 1, &pushData);
		cmd->TransitionLayout(input, ImageLayoutType::StorageImage, inputOldLayout);
		cmd->TransitionLayout(depth, ImageReadAccess::PixelShaderRead, oldDepthLayout);

		auto& stats = m_Renderer.GetStats();
		++stats.Dispatches;
	}
	
	void FogPassTask::InitPipeline()
	{
		PipelineComputeState state;
		state.ComputeShader = Shader::Create("fog.comp", ShaderType::Compute);
		m_Pipeline = PipelineCompute::Create(state);
	}
}
