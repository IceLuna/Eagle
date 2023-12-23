#include "egpch.h"
#include "FogPassTask.h"

#include "Eagle/Renderer/SceneRenderer.h"
#include "Eagle/Renderer/VidWrappers/RenderCommandManager.h"

#include "Eagle/Debug/CPUTimings.h"
#include "Eagle/Debug/GPUTimings.h"

namespace Eagle
{
	FogPassTask::FogPassTask(SceneRenderer& renderer, const Ref<Image>& renderTo)
		: RendererTask(renderer)
		, m_Result(renderTo)
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

		constexpr uint32_t tileSize = 8;
		const glm::uvec2 size = m_Result->GetSize();
		glm::uvec2 numGroups = { glm::ceil(size.x / float(tileSize)), glm::ceil(size.y / float(tileSize)) };

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
			cmd->Write(m_FogDataBuffer, &m_FogData, sizeof(FogData), 0, BufferLayoutType::Unknown, BufferReadAccess::Uniform);
			cmd->Barrier(m_FogDataBuffer);
		}

		const auto& depth = m_Renderer.GetGBuffer().Depth;
		m_Pipeline->SetImage(m_Result, 0, 0);
		m_Pipeline->SetImageSampler(depth, Sampler::PointSampler, 0, 1);
		m_Pipeline->SetBuffer(m_FogDataBuffer, 0, 2);

		const ImageLayout inputOldLayout = m_Result->GetLayout();
		const ImageLayout oldDepthLayout = depth->GetLayout();

		cmd->TransitionLayout(depth, oldDepthLayout, ImageReadAccess::PixelShaderRead);
		cmd->TransitionLayout(m_Result, inputOldLayout, ImageLayoutType::StorageImage);
		cmd->Dispatch(m_Pipeline, numGroups.x, numGroups.y, 1, &pushData);
		cmd->TransitionLayout(m_Result, ImageLayoutType::StorageImage, inputOldLayout);
		cmd->TransitionLayout(m_Renderer.GetGBuffer().Depth, ImageReadAccess::PixelShaderRead, oldDepthLayout);
	}
	
	void FogPassTask::InitPipeline()
	{
		PipelineComputeState state;
		state.ComputeShader = Shader::Create("assets/shaders/fog.comp", ShaderType::Compute);
		m_Pipeline = PipelineCompute::Create(state);
	}
}
