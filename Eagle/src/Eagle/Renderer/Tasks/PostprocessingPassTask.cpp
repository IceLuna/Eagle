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
			float InvGamma;
			float Exposure;
			float PhotolinearScale;
			float WhitePoint;
			uint32_t TonemappingMethod;

			// Only exist when fog is enabled
			float FogMin;
			float FogMax;
			float Density;
			glm::mat4 InvProjMat;
			glm::vec3 FogColor;
			FogEquation Equation;
		} pushData;
		static_assert(sizeof(PushData) <= 128);

		const auto& options = m_Renderer.GetOptions_RT();
		const auto& fogOptions = options.FogSettings;

		pushData.InvGamma = 1.f / options.Gamma;
		pushData.Exposure = options.Exposure;
		pushData.PhotolinearScale = m_Renderer.GetPhotoLinearScale();
		pushData.WhitePoint = options.FilmicTonemappingParams.WhitePoint;
		pushData.TonemappingMethod = (uint32_t)options.Tonemapping;

		ImageLayout oldDepthLayout;
		if (fogOptions.bEnable)
		{
			pushData.FogMin = fogOptions.MinDistance;
			pushData.FogMax = fogOptions.MaxDistance;
			pushData.Density = fogOptions.Density;
			pushData.FogColor = fogOptions.Color;
			pushData.Equation = fogOptions.Equation;
			pushData.InvProjMat = glm::inverse(m_Renderer.GetProjectionMatrix());

			auto& depth = m_Renderer.GetGBuffer().Depth;
			oldDepthLayout = depth->GetLayout();
			cmd->TransitionLayout(depth, oldDepthLayout, ImageReadAccess::PixelShaderRead);
			m_Pipeline->SetImageSampler(depth, Sampler::PointSampler, 0, 1);
		}

		m_Pipeline->SetImageSampler(m_Input, Sampler::PointSampler, 0, 0);

		cmd->BeginGraphics(m_Pipeline);
		cmd->SetGraphicsRootConstants(nullptr, &pushData);
		cmd->Draw(6, 0);
		cmd->EndGraphics();

		if (fogOptions.bEnable)
			cmd->TransitionLayout(m_Renderer.GetGBuffer().Depth, ImageReadAccess::PixelShaderRead, oldDepthLayout);
	}

	void PostprocessingPassTask::InitWithOptions(const SceneRendererSettings& settings)
	{
		auto state = m_Pipeline->GetState();

		ShaderDefines defines = state.FragmentShader->GetDefines();
		auto it = defines.find("EG_FOG");

		if (settings.FogSettings.bEnable)
		{
			if (it == defines.end())
			{
				defines["EG_FOG"] = "";
				state.FragmentShader->SetDefines(defines);
				state.FragmentShader->Reload();
			}
		}
		else
		{
			if (it != defines.end())
			{
				defines.erase(it);
				state.FragmentShader->SetDefines(defines);
				state.FragmentShader->Reload();
			}
		}
	}
	
	void PostprocessingPassTask::InitPipeline()
	{
		ColorAttachment colorAttachment;
		colorAttachment.InitialLayout = ImageLayoutType::Unknown;
		colorAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		colorAttachment.Image = m_Output;
		colorAttachment.ClearOperation = ClearOperation::Clear;
		colorAttachment.ClearColor = glm::vec4(0.f, 0.f, 0.f, 1.f);

		ShaderDefines defines;
		if (m_Renderer.GetOptions_RT().FogSettings.bEnable)
			defines["EG_FOG"] = "";

		PipelineGraphicsState state;
		state.VertexShader = Shader::Create("assets/shaders/present.vert", ShaderType::Vertex, ShaderDefines{ {"EG_NO_FLIP", ""} });
		state.FragmentShader = Shader::Create("assets/shaders/postprocessing.frag", ShaderType::Fragment, defines);
		state.ColorAttachments.push_back(colorAttachment);
		state.CullMode = CullMode::Back;

		m_Pipeline = PipelineGraphics::Create(state);
	}
}
