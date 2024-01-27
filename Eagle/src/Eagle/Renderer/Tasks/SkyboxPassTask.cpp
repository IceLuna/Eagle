#include "egpch.h"
#include "SkyboxPassTask.h"

#include "Eagle/Renderer/SceneRenderer.h"
#include "Eagle/Renderer/VidWrappers/RenderCommandManager.h"
#include "Eagle/Renderer/VidWrappers/Texture.h"

#include "Eagle/Debug/CPUTimings.h"
#include "Eagle/Debug/GPUTimings.h"

namespace Eagle
{
	SkyboxPassTask::SkyboxPassTask(SceneRenderer& renderer, const Ref<Image>& renderTo)
		: RendererTask(renderer)
		, m_FinalImage(renderTo)
	{
		const auto& sky = m_Renderer.GetSkySettings();
		m_Clouds.bCirrus = sky.bEnableCirrusClouds;
		m_Clouds.bCumulus = sky.bEnableCumulusClouds;
		m_Clouds.CumulusLayers = sky.CumulusLayers;

		InitPipeline();
	}

	void SkyboxPassTask::RecordCommandBuffer(const Ref<CommandBuffer>& cmd)
	{
		const bool bEnabled = m_Renderer.IsSkyboxEnabled();
		if (!bEnabled)
			return;

		const auto& skybox = m_Renderer.GetSkybox();
		const bool bSkyAsBackground = m_Renderer.GetUseSkyAsBackground();

		if (!skybox && !bSkyAsBackground)
			return;

		EG_GPU_TIMING_SCOPED(cmd, "Skybox Pass");
		EG_CPU_TIMING_SCOPED("Skybox Pass");

		const glm::mat4 ViewProj = m_Renderer.GetProjectionMatrix() * glm::mat4(glm::mat3(m_Renderer.GetViewMatrix()));

		if (bSkyAsBackground)
		{
			struct PushData
			{
				glm::vec3 SunPos = glm::vec3(0.f, 0.f, -1.f);
				float SkyIntensity = 11.f;

				glm::vec3 Nitrogen = glm::vec3(0.650f, 0.570f, 0.475f);
				float CloudsIntensity = 1.f;
				float ScatteringDir = 0.995f;

				float Cirrus = 0.4f;
				float Cumulus = 0.8f;
			} pushData;

			const auto& sky = m_Renderer.GetSkySettings();

			pushData.SunPos = sky.SunPos;
			pushData.SkyIntensity = sky.SkyIntensity;
			pushData.CloudsIntensity = sky.CloudsIntensity;
			pushData.Nitrogen = sky.CloudsColor;
			pushData.ScatteringDir = sky.Scattering;
			pushData.Cirrus = sky.Cirrus;
			pushData.Cumulus = sky.Cumulus;

			Clouds clouds;
			clouds.bCirrus = sky.bEnableCirrusClouds;
			clouds.bCumulus = sky.bEnableCumulusClouds;
			clouds.CumulusLayers = sky.CumulusLayers;
			if (clouds != m_Clouds)
			{
				m_Clouds = clouds;
				ReloadSkyPipeline();
			}

			cmd->BeginGraphics(m_SkyPipeline);
			cmd->SetGraphicsRootConstants(&ViewProj[0][0], &pushData);
		}
		else
		{
			m_IBLPipeline->SetImageSampler(skybox->GetTexture()->GetImage(), Sampler::BilinearSampler, 0, 0);
			cmd->BeginGraphics(m_IBLPipeline);
			cmd->SetGraphicsRootConstants(&ViewProj[0][0], nullptr);
		}
		cmd->Draw(36, 0);
		cmd->EndGraphics();
	}

	void SkyboxPassTask::InitPipeline()
	{
		ColorAttachment colorAttachment;
		colorAttachment.ClearOperation = ClearOperation::Load;
		colorAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
		colorAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		colorAttachment.Image = m_FinalImage;

		DepthStencilAttachment depthAttachment;
		depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.Image = m_Renderer.GetGBuffer().Depth;
		depthAttachment.bWriteDepth = false;
		depthAttachment.ClearOperation = ClearOperation::Load;
		depthAttachment.DepthCompareOp = CompareOperation::LessEqual;

		PipelineGraphicsState state;
		state.VertexShader = Shader::Create("skybox.vert", ShaderType::Vertex);
		state.FragmentShader = Shader::Create("skybox_ibl.frag", ShaderType::Fragment);
		state.ColorAttachments.push_back(colorAttachment);
		state.DepthStencilAttachment = depthAttachment;
		state.CullMode = CullMode::Back;

		m_IBLPipeline = PipelineGraphics::Create(state);

		state.FragmentShader = Shader::Create("skybox_sky.frag", ShaderType::Fragment);
		
		ShaderSpecializationInfo constants;
		constants.Data = &m_Clouds;
		constants.Size = sizeof(Clouds);
		constants.MapEntries = { {0, 0, 4}, {1, 4, 4}, {2, 8, 4} };
		state.FragmentSpecializationInfo = constants;

		m_SkyPipeline = PipelineGraphics::Create(state);
	}
	
	void SkyboxPassTask::ReloadSkyPipeline()
	{
		auto state = m_SkyPipeline->GetState();
		ShaderSpecializationInfo constants;
		constants.Data = &m_Clouds;
		constants.Size = sizeof(Clouds);
		constants.MapEntries = { {0, 0, 4}, {1, 4, 4}, {2, 8, 4} };
		state.FragmentSpecializationInfo = constants;

		m_SkyPipeline->SetState(state);
	}
}
