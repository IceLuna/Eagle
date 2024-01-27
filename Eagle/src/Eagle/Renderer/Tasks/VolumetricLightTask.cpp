#include "egpch.h"
#include "VolumetricLightTask.h"

#include "Eagle/Renderer/SceneRenderer.h"
#include "Eagle/Renderer/RenderManager.h"

#include "Eagle/Renderer/VidWrappers/RenderCommandManager.h"

#include "Eagle/Debug/CPUTimings.h"
#include "Eagle/Debug/GPUTimings.h"

namespace Eagle
{
    VolumetricLightTask::VolumetricLightTask(SceneRenderer& renderer, const Ref<Image>& renderTo)
        : RendererTask(renderer), m_ResultImage(renderTo)
    {
		const glm::uvec3 size = m_ResultImage->GetSize();
		const glm::uvec3 halfSize = glm::max(size / 2u, glm::uvec3(1u));

		ImageSpecifications specs;
		specs.Format = ImageFormat::R11G11B10_Float;
		specs.Size = halfSize;
		specs.Usage = ImageUsage::ColorAttachment | ImageUsage::Sampled | ImageUsage::Storage;
		m_VolumetricsImage = Image::Create(specs, "PBR_Volumetric");
		m_VolumetricsImageBlurred = Image::Create(specs, "PBR_Volumetric_Blurred");

		const auto& options = m_Renderer.GetOptions();
		m_VolumetricSettings = options.VolumetricSettings;
		m_Constants.VolumetricSamples = m_VolumetricSettings.Samples;
		bStutterlessShaders = options.bStutterlessShaders;
		bTranslucentShadows = options.bTranslucentShadows;

		InitPipeline(false, false, false);
    }

	void VolumetricLightTask::RecordCommandBuffer(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "Volumetric Light Pass");
		EG_CPU_TIMING_SCOPED("Volumetric Light Pass");

		constexpr uint32_t tileSize = 8;
		const glm::uvec2 size = m_ResultImage->GetSize();
		const glm::uvec2 numGroups = { glm::ceil(size.x / float(tileSize)), glm::ceil(size.y / float(tileSize)) };

		const glm::uvec2 halfSize = m_VolumetricsImage->GetSize();
		const glm::uvec2 halfNumGroups = { glm::ceil(halfSize.x / float(tileSize)), glm::ceil(halfSize.y / float(tileSize)) };
		const Timestep ts = Application::Get().GetTimestep();
		m_Time += ts * m_VolumetricSettings.FogSpeed;

		struct PushDataVol
		{
			glm::mat4 ViewProjInv;
			glm::vec3 CameraPos;
			float VolumetricMaxScatteringDist;
			glm::ivec2 Size;
			float MaxShadowDistance;
			float Time;
			uint32_t PointLights;
			uint32_t SpotLights;
			uint32_t HasDirLight;
		} pushData;
		pushData.ViewProjInv = glm::inverse(m_Renderer.GetViewProjection());
		pushData.CameraPos = m_Renderer.GetViewPosition();
		pushData.VolumetricMaxScatteringDist = m_VolumetricSettings.MaxScatteringDistance;
		pushData.Size = halfSize;
		pushData.MaxShadowDistance = m_Renderer.GetShadowMaxDistance() * m_Renderer.GetShadowMaxDistance();
		pushData.Time = m_Time;
		pushData.PointLights = (uint32_t)m_Renderer.GetPointLights().size();
		pushData.SpotLights = (uint32_t)m_Renderer.GetSpotLights().size();
		pushData.HasDirLight = uint32_t(m_Renderer.HasDirectionalLight());

		ConstantData info;
		info.PointLightsCount = pushData.PointLights;
		info.SpotLightsCount = pushData.SpotLights;
		info.bHasDirLight = pushData.HasDirLight;
		info.VolumetricSamples = m_VolumetricSettings.Samples;
		if (info != m_Constants)
		{
			m_Constants = info;
			// Don't need to recreate if stutterless
			const bool bRecreate = !bStutterlessShaders;
			if (bRecreate)
				InitPipeline(false, false, false);
		}

		const auto& gbuffer = m_Renderer.GetGBuffer();
		m_Pipeline->SetImage(m_VolumetricsImage, 0, 0);
		m_Pipeline->SetImageSampler(gbuffer.Depth, Sampler::PointSampler, 0, 1);
		m_Pipeline->SetImageSampler(gbuffer.Geometry_Shading_Normals, Sampler::PointSampler, 0, 2);
		m_Pipeline->SetBuffer(m_Renderer.GetPointLightsBuffer(), EG_SCENE_SET, 0);
		m_Pipeline->SetBuffer(m_Renderer.GetSpotLightsBuffer(), EG_SCENE_SET, 1);
		m_Pipeline->SetBuffer(m_Renderer.GetDirectionalLightBuffer(), EG_SCENE_SET, 2);
		m_Pipeline->SetBuffer(m_Renderer.GetCameraBuffer(), EG_SCENE_SET, 3);
		m_Pipeline->SetImageSamplerArray(m_Renderer.GetDirectionalLightShadowMaps(), m_Renderer.GetDirectionalLightShadowMapsSamplers(), 2, 0);
		m_Pipeline->SetImageSamplerArray(m_Renderer.GetPointLightShadowMaps(), m_Renderer.GetPointLightShadowMapsSamplers(), 3, 0);
		m_Pipeline->SetImageSamplerArray(m_Renderer.GetSpotLightShadowMaps(), m_Renderer.GetSpotLightShadowMapsSamplers(), 4, 0);

		if (bTranslucentShadows)
		{
			m_Pipeline->SetImageSamplerArray(m_Renderer.GetDirectionalLightShadowMapsColored(), m_Renderer.GetDirectionalLightShadowMapsSamplers(), 5, 0);
			m_Pipeline->SetImageSamplerArray(m_Renderer.GetPointLightShadowMapsColored(), m_Renderer.GetPointLightShadowMapsSamplers(), 6, 0);
			m_Pipeline->SetImageSamplerArray(m_Renderer.GetSpotLightShadowMapsColored(), m_Renderer.GetSpotLightShadowMapsSamplers(), 7, 0);

			m_Pipeline->SetImageSamplerArray(m_Renderer.GetDirectionalLightShadowMapsColoredDepth(), m_Renderer.GetDirectionalLightShadowMapsSamplers(), 8, 0);
			m_Pipeline->SetImageSamplerArray(m_Renderer.GetPointLightShadowMapsColoredDepth(), m_Renderer.GetPointLightShadowMapsSamplers(), 9, 0);
			m_Pipeline->SetImageSamplerArray(m_Renderer.GetSpotLightShadowMapsColoredDepth(), m_Renderer.GetSpotLightShadowMapsSamplers(), 10, 0);
		}

		m_CompositePipeline->SetImageSampler(m_VolumetricsImageBlurred, Sampler::BilinearSamplerClamp, 0, 0);
		m_CompositePipeline->SetImage(m_ResultImage, 0, 1);

		cmd->TransitionLayout(m_ResultImage, m_ResultImage->GetLayout(), ImageLayoutType::StorageImage);
		cmd->TransitionLayout(gbuffer.Depth, gbuffer.Depth->GetLayout(), ImageReadAccess::PixelShaderRead);

		{
			EG_GPU_TIMING_SCOPED(cmd, "Volumetric Lighting");
			EG_CPU_TIMING_SCOPED("Volumetric Lighting");

			cmd->TransitionLayout(m_VolumetricsImage, ImageLayoutType::Unknown, ImageLayoutType::StorageImage);
			cmd->Dispatch(m_Pipeline, halfNumGroups.x, halfNumGroups.y, 1, &pushData);
			cmd->TransitionLayout(m_VolumetricsImage, ImageLayoutType::StorageImage, ImageReadAccess::PixelShaderRead);
		}
		cmd->TransitionLayout(gbuffer.Depth, gbuffer.Depth->GetLayout(), ImageLayoutType::DepthStencilWrite);

		struct PushDataComp
		{
			glm::ivec2 Size;
			glm::vec2 TexelSize;
		} pushDataComp;

		{
			EG_GPU_TIMING_SCOPED(cmd, "Volumetric Blur");
			EG_CPU_TIMING_SCOPED("Volumetric Blur");

			m_GuassianPipeline->SetImageSampler(m_VolumetricsImage, Sampler::BilinearSamplerClamp, 0, 0);
			m_GuassianPipeline->SetImage(m_VolumetricsImageBlurred, 0, 1);

			pushDataComp.Size = pushData.Size;
			pushDataComp.TexelSize = 1.f / glm::vec2(pushData.Size);
			
			cmd->TransitionLayout(m_VolumetricsImageBlurred, ImageLayoutType::Unknown, ImageLayoutType::StorageImage);
			cmd->Dispatch(m_GuassianPipeline, halfNumGroups.x, halfNumGroups.y, 1, &pushDataComp);
			cmd->TransitionLayout(m_VolumetricsImageBlurred, ImageLayoutType::StorageImage, ImageReadAccess::PixelShaderRead);
		}

		pushDataComp.Size = size;
		pushDataComp.TexelSize = 1.f / glm::vec2(pushDataComp.Size);

		{
			EG_GPU_TIMING_SCOPED(cmd, "Volumetric Composite");
			EG_CPU_TIMING_SCOPED("Volumetric Composite");
			cmd->Dispatch(m_CompositePipeline, numGroups.x, numGroups.y, 1, &pushDataComp);
		}

		cmd->TransitionLayout(m_ResultImage, m_ResultImage->GetLayout(), ImageReadAccess::PixelShaderRead);
	}

	void VolumetricLightTask::OnResize(glm::uvec2 size)
	{
		const glm::uvec2 halfSize = glm::max(size / 2u, glm::uvec2(1u));
		m_VolumetricsImage->Resize(glm::uvec3(halfSize, 1u));
		m_VolumetricsImageBlurred->Resize(glm::uvec3(halfSize, 1u));
	}

	void VolumetricLightTask::InitPipeline(bool bStutterlessChanged, bool translucentShadowsChanged, bool bVolumetricFogChanged)
	{
		ShaderSpecializationInfo constants;
		if (!bStutterlessShaders)
		{
			constants.MapEntries.push_back({ 0, 0, sizeof(uint32_t) });
			constants.MapEntries.push_back({ 1, 4, sizeof(uint32_t) });
			constants.MapEntries.push_back({ 2, 8, sizeof(uint32_t) });
		}
		constants.MapEntries.push_back({ 3, 12, sizeof(uint32_t) });
		constants.Data = &m_Constants;
		constants.Size = sizeof(m_Constants);

		if (m_Pipeline)
		{
			auto state = m_Pipeline->GetState();
			state.ComputeSpecializationInfo = constants;
			m_Pipeline->SetState(state);

			bool bUpdateDefines = false;
			auto defines = state.ComputeShader->GetDefines();
			if (bStutterlessChanged)
			{
				auto it = defines.find("EG_STUTTERLESS");
				if (bStutterlessShaders)
				{
					if (it == defines.end())
					{
						defines["EG_STUTTERLESS"] = "";
						bUpdateDefines = true;
					}
				}
				else
				{
					if (it != defines.end())
					{
						defines.erase(it);
						bUpdateDefines = true;
					}
				}
			}
			if (translucentShadowsChanged)
			{
				auto it = defines.find("EG_TRANSLUCENT_SHADOWS");
				if (bTranslucentShadows)
				{
					if (it == defines.end())
					{
						defines["EG_TRANSLUCENT_SHADOWS"] = "";
						bUpdateDefines = true;
					}
				}
				else
				{
					if (it != defines.end())
					{
						defines.erase(it);
						bUpdateDefines = true;
					}
				}
			}
			if (bVolumetricFogChanged)
			{
				auto it = defines.find("EG_VOLUMETRIC_FOG");
				if (m_VolumetricSettings.bFogEnable)
				{
					if (it == defines.end())
					{
						defines["EG_VOLUMETRIC_FOG"] = "";
						bUpdateDefines = true;
					}
				}
				else
				{
					if (it != defines.end())
					{
						defines.erase(it);
						bUpdateDefines = true;
					}
				}
			}

			if (bUpdateDefines)
				state.ComputeShader->SetDefines(defines);
		}
		else
		{
			ShaderDefines defines;
			defines["EG_VOLUMETRIC_LIGHT"] = "";
			if (bStutterlessShaders)
				defines["EG_STUTTERLESS"] = "";
			if (bTranslucentShadows)
				defines["EG_TRANSLUCENT_SHADOWS"] = "";
			if (m_VolumetricSettings.bFogEnable)
				defines["EG_VOLUMETRIC_FOG"] = "";

			PipelineComputeState state;
			state.ComputeShader = Shader::Create("volumetric_light.comp", ShaderType::Compute, defines);
			state.ComputeSpecializationInfo = constants;

			m_Pipeline = PipelineCompute::Create(state);
		}

		if (!m_CompositePipeline)
		{
			PipelineComputeState state;
			state.ComputeShader = Shader::Create("volumetric_composite.comp", ShaderType::Compute);
			m_CompositePipeline = PipelineCompute::Create(state);
		}

		if (!m_GuassianPipeline)
		{
			PipelineComputeState state;
			state.ComputeShader = Shader::Create("guassian.comp", ShaderType::Compute);
			m_GuassianPipeline = PipelineCompute::Create(state);
		}
	}
}
