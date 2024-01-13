#include "egpch.h"
#include "PostprocessingPassTask.h"

#include "Eagle/Renderer/SceneRenderer.h"
#include "Eagle/Renderer/VidWrappers/RenderCommandManager.h"
#include "Eagle/Renderer/VidWrappers/Image.h"

#include "Eagle/Debug/CPUTimings.h"
#include "Eagle/Debug/GPUTimings.h"

namespace Eagle
{
	PostprocessingPassTask::PostprocessingPassTask(SceneRenderer& renderer, const Ref<Image>& input, const Ref<Image>& output)
		: RendererTask(renderer)
		, m_Input(input)
		, m_Output(output)
	{
		InitPipeline();
	}

	void PostprocessingPassTask::RecordCommandBuffer(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "Postprocessing Pass");
		EG_CPU_TIMING_SCOPED("Postprocessing Pass");

		struct PushData
		{
			glm::ivec2 Size;
			float InvGamma;
			float Exposure;
			float PhotolinearScale;
			float WhitePoint;
			uint32_t TonemappingMethod;
		} pushData;
		static_assert(sizeof(PushData) <= 128);

		const auto& options = m_Renderer.GetOptions_RT();

		constexpr uint32_t tileSize = 8;
		const glm::uvec2 size = m_Input->GetSize();
		glm::uvec2 numGroups = { glm::ceil(size.x / float(tileSize)), glm::ceil(size.y / float(tileSize)) };
		
		pushData.Size = size;
		pushData.InvGamma = 1.f / options.Gamma;
		pushData.Exposure = options.Exposure;
		pushData.PhotolinearScale = m_Renderer.GetPhotoLinearScale();
		pushData.WhitePoint = options.FilmicTonemappingParams.WhitePoint;
		pushData.TonemappingMethod = (uint32_t)options.Tonemapping;

		m_Pipeline->SetImage(m_Input, 0, 0);
		m_Pipeline->SetImage(m_Output, 0, 1);

		const ImageLayout inputOldLayout = m_Input->GetLayout();

		cmd->TransitionLayout(m_Input, inputOldLayout, ImageLayoutType::StorageImage);
		cmd->TransitionLayout(m_Output, ImageLayoutType::Unknown, ImageLayoutType::StorageImage);

		cmd->Dispatch(m_Pipeline, numGroups.x, numGroups.y, 1, &pushData);

		cmd->TransitionLayout(m_Input, ImageLayoutType::StorageImage, inputOldLayout);
		cmd->TransitionLayout(m_Output, ImageLayoutType::StorageImage, ImageReadAccess::PixelShaderRead);
	}
	
	void PostprocessingPassTask::InitPipeline()
	{
		PipelineComputeState state;
		state.ComputeShader = Shader::Create(Application::GetCorePath() / "assets/shaders/postprocessing.comp", ShaderType::Compute);

		m_Pipeline = PipelineCompute::Create(state);
	}
}
