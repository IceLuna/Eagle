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
		specs.Format = ImageFormat::R16G16B16A16_Float;
		specs.Size = halfSize;
		specs.Usage = ImageUsage::ColorAttachment | ImageUsage::Sampled | ImageUsage::Storage;
		m_VolumetricsImage = Image::Create(specs, "PBR_Volumetric");

		const auto& options = m_Renderer.GetOptions();
		m_VolumetricSettings = options.VolumetricSettings;
		m_Constants.VolumetricSamples = m_VolumetricSettings.Samples;

		InitPipeline();
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

		const auto& gbuffer = m_Renderer.GetGBuffer();
		ConstantData info;
		info.PBRInfo.PointLightsCount = (uint32_t)m_Renderer.GetPointLights().size();
		info.PBRInfo.SpotLightsCount = (uint32_t)m_Renderer.GetSpotLights().size();
		info.PBRInfo.bHasDirLight = m_Renderer.HasDirectionalLight();
		info.PBRInfo.bHasIrradiance = false;
		info.VolumetricSamples = m_VolumetricSettings.Samples;
		if (info.PBRInfo != m_Constants.PBRInfo)
		{
			m_Constants = info;
			InitPipeline();
		}

		m_Pipeline->SetImage(m_VolumetricsImage, 0, 0);
		m_Pipeline->SetImageSampler(gbuffer.Depth, Sampler::PointSampler, 0, 1);
		m_Pipeline->SetImageSampler(gbuffer.Geometry_Shading_Normals, Sampler::PointSampler, 0, 2);
		m_Pipeline->SetBuffer(m_Renderer.GetPointLightsBuffer(), EG_SCENE_SET, 0);
		m_Pipeline->SetBuffer(m_Renderer.GetSpotLightsBuffer(), EG_SCENE_SET, 1);
		m_Pipeline->SetBuffer(m_Renderer.GetDirectionalLightBuffer(), EG_SCENE_SET, 2);
		m_Pipeline->SetBuffer(m_Renderer.GetCameraBuffer(), EG_SCENE_SET, 3);
		m_Pipeline->SetImageSamplerArray(m_Renderer.GetDirectionalLightShadowMaps(), m_Renderer.GetDirectionalLightShadowMapsSamplers(), 2, 0);
		m_Pipeline->SetImageSamplerArray(m_Renderer.GetPointLightShadowMaps(), m_Renderer.GetPointLightShadowMapsSamplers(), 2, 1);
		m_Pipeline->SetImageSamplerArray(m_Renderer.GetSpotLightShadowMaps(), m_Renderer.GetSpotLightShadowMapsSamplers(), 2, 2);

		m_CompositePipeline->SetImageSampler(m_VolumetricsImage, Sampler::BilinearSampler, 0, 0);
		m_CompositePipeline->SetImage(m_ResultImage, 0, 1);

		struct PushDataVol
		{
			glm::mat4 ViewProjInv;
			glm::vec3 CameraPos;
			float VolumetricMaxScatteringDist;
			glm::ivec2 Size;
			float MaxShadowDistance;
		} pushData;
		pushData.ViewProjInv = glm::inverse(m_Renderer.GetViewProjection());
		pushData.CameraPos = m_Renderer.GetViewPosition();
		pushData.VolumetricMaxScatteringDist = m_VolumetricSettings.MaxScatteringDistance;
		pushData.Size = halfSize;
		pushData.MaxShadowDistance = m_Renderer.GetShadowMaxDistance();

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
		pushDataComp.Size = size;
		pushDataComp.TexelSize = 1.f / glm::vec2(pushDataComp.Size);

		{
			EG_GPU_TIMING_SCOPED(cmd, "Volumetric Composite");
			EG_CPU_TIMING_SCOPED("Volumetric Composite");
			cmd->Dispatch(m_CompositePipeline, numGroups.x, numGroups.y, 1, &pushDataComp);
		}

		cmd->TransitionLayout(m_ResultImage, m_ResultImage->GetLayout(), ImageReadAccess::PixelShaderRead);
	}

	void VolumetricLightTask::InitPipeline()
	{
		ShaderSpecializationInfo constants;
		constants.MapEntries.push_back({ 0, 0, sizeof(uint32_t) });
		constants.MapEntries.push_back({ 1, 4, sizeof(uint32_t) });
		constants.MapEntries.push_back({ 2, 8, sizeof(uint32_t) });
		constants.MapEntries.push_back({ 3, 16, sizeof(uint32_t) });
		constants.Data = &m_Constants;
		constants.Size = sizeof(m_Constants);

		if (m_Pipeline)
		{
			auto state = m_Pipeline->GetState();
			state.ComputeSpecializationInfo = constants;
			m_Pipeline->SetState(state);
		}
		else
		{
			ShaderDefines defines;
			defines["EG_VOLUMETRIC_LIGHT"] = "";

			PipelineComputeState state;
			state.ComputeShader = Shader::Create("assets/shaders/volumetric_light.comp", ShaderType::Compute, defines);
			state.ComputeSpecializationInfo = constants;

			m_Pipeline = PipelineCompute::Create(state);
		}

		if (!m_CompositePipeline)
		{
			PipelineComputeState state;
			state.ComputeShader = Shader::Create("assets/shaders/volumetric_composite.comp", ShaderType::Compute);
			m_CompositePipeline = PipelineCompute::Create(state);
		}
	}
}