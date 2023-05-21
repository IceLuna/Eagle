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

			// Only exist when fog is enabled
			float FogMin;
			glm::vec3 FogColor;
			float FogMax;
			glm::mat4 InvProjMat;
			float Density;
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

		constexpr uint32_t tileSize = 8;
		const glm::uvec2 size = m_Input->GetSize();
		glm::uvec2 numGroups = { glm::ceil(size.x / float(tileSize)), glm::ceil(size.y / float(tileSize)) };
		pushData.Size = size;

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
			m_Pipeline->SetImageSampler(depth, Sampler::PointSampler, 0, 2);
		}

		m_Pipeline->SetImage(m_Input, 0, 0);
		m_Pipeline->SetImage(m_Output, 0, 1);

		const ImageLayout inputOldLayout = m_Input->GetLayout();

		cmd->TransitionLayout(m_Input, inputOldLayout, ImageLayoutType::StorageImage);
		cmd->TransitionLayout(m_Output, ImageLayoutType::Unknown, ImageLayoutType::StorageImage);

		cmd->Dispatch(m_Pipeline, numGroups.x, numGroups.y, 1, &pushData);

		cmd->TransitionLayout(m_Input, ImageLayoutType::StorageImage, inputOldLayout);
		cmd->TransitionLayout(m_Output, ImageLayoutType::StorageImage, ImageReadAccess::PixelShaderRead);

		if (fogOptions.bEnable)
			cmd->TransitionLayout(m_Renderer.GetGBuffer().Depth, ImageReadAccess::PixelShaderRead, oldDepthLayout);
	}

	void PostprocessingPassTask::InitWithOptions(const SceneRendererSettings& settings)
	{
		const auto& state = m_Pipeline->GetState();

		auto& shader = state.ComputeShader;
		ShaderDefines defines = shader->GetDefines();
		auto it = defines.find("EG_FOG");

		if (settings.FogSettings.bEnable)
		{
			if (it == defines.end())
			{
				defines["EG_FOG"] = "";
				shader->SetDefines(defines);
			}
		}
		else
		{
			if (it != defines.end())
			{
				defines.erase(it);
				shader->SetDefines(defines);
			}
		}
	}
	
	void PostprocessingPassTask::InitPipeline()
	{
		ShaderDefines defines;
		if (m_Renderer.GetOptions_RT().FogSettings.bEnable)
			defines["EG_FOG"] = "";

		PipelineComputeState state;
		state.ComputeShader = Shader::Create("assets/shaders/postprocessing.comp", ShaderType::Compute, defines);

		m_Pipeline = PipelineCompute::Create(state);
	}
}
