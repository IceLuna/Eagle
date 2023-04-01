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

		struct VertexPushData
		{
			uint32_t FlipX = 0;
			uint32_t FlipY = 0;
		} vertexPushData;

		struct PushData
		{
			float InvGamma;
			float Exposure;
			float PhotolinearScale;
			float WhitePoint;
			uint32_t TonemappingMethod;
		} pushData;
		static_assert(sizeof(VertexPushData) + sizeof(PushData) <= 128);

		const auto& options = m_Renderer.GetOptions_RT();

		pushData.InvGamma = 1.f / options.Gamma;
		pushData.Exposure = options.Exposure;
		pushData.PhotolinearScale = m_Renderer.GetPhotoLinearScale();
		pushData.WhitePoint = options.FilmicTonemappingParams.WhitePoint;
		pushData.TonemappingMethod = (uint32_t)options.Tonemapping;

		m_Pipeline->SetImageSampler(m_Input, Sampler::PointSampler, 0, 0);

		cmd->BeginGraphics(m_Pipeline);
		cmd->SetGraphicsRootConstants(&vertexPushData, &pushData);
		cmd->Draw(6, 0);
		cmd->EndGraphics();
	}
	
	void PostprocessingPassTask::InitPipeline()
	{
		ColorAttachment colorAttachment;
		colorAttachment.InitialLayout = ImageLayoutType::Unknown;
		colorAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		colorAttachment.Image = m_Output;
		colorAttachment.ClearOperation = ClearOperation::Clear;
		colorAttachment.ClearColor = glm::vec4(0.f, 0.f, 0.f, 1.f);

		PipelineGraphicsState state;
		state.VertexShader = ShaderLibrary::GetOrLoad("assets/shaders/present.vert", ShaderType::Vertex);
		state.FragmentShader = ShaderLibrary::GetOrLoad("assets/shaders/postprocessing.frag", ShaderType::Fragment);
		state.ColorAttachments.push_back(colorAttachment);
		state.CullMode = CullMode::Back;

		m_Pipeline = PipelineGraphics::Create(state);
	}
}
