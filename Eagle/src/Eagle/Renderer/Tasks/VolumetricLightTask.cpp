#include "egpch.h"
#include "VolumetricLightTask.h"

#include "Eagle/Renderer/SceneRenderer.h"
#include "Eagle/Renderer/RenderManager.h"

#include "Eagle/Renderer/VidWrappers/RenderCommandManager.h"

#include "Eagle/Debug/CPUTimings.h"
#include "Eagle/Debug/GPUTimings.h"

namespace Eagle
{
	constexpr uint32_t s_DownscaleFactor = 2u; // Volumetrics are calculated in half res

    VolumetricLightTask::VolumetricLightTask(SceneRenderer& renderer)
        : RendererTask(renderer)
    {
		const glm::uvec3 size = m_Renderer.GetHDROutput()->GetSize();
		const glm::uvec3 halfSize = glm::max(size / s_DownscaleFactor, glm::uvec3(1u));

		ImageSpecifications specs;
		specs.Format = ImageFormat::R11G11B10_Float;
		specs.Size = halfSize;
		specs.Usage = ImageUsage::ColorAttachment | ImageUsage::Sampled | ImageUsage::Storage;
		m_VolumetricsImage = Image::Create(specs, "PBR_Volumetric");
		m_VolumetricsImageBlurred = Image::Create(specs, "PBR_Volumetric_Blurred");

		const auto& options = m_Renderer.GetOptions();
		m_VolumetricSettings = options.VolumetricSettings;
		m_Constants.VolumetricSamples = m_VolumetricSettings.Samples;
		bTranslucentShadows = options.bTranslucentShadows;

		InitPipeline(false, false);
    }

	void VolumetricLightTask::RecordCommandBuffer(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "Volumetric Light Pass");
		EG_CPU_TIMING_SCOPED("Volumetric Light Pass");

		auto& stats = m_Renderer.GetStats();
		const auto& input = m_Renderer.GetHDROutput();

		const glm::uvec2 size = input->GetSize();
		const glm::uvec2 volumetricsImageSize = m_VolumetricsImage->GetSize();
		const Ref<LightCullingTask>& lightCulling = m_Renderer.GetLightCullingTask();

		const Timestep ts = Application::Get().GetTimestep();
		m_Time += ts * m_VolumetricSettings.FogSpeed;

		struct PushDataVol
		{
			glm::vec3 CameraPos;
			float VolumetricMaxScatteringDist;
			glm::ivec2 Size;
			float Time;
			glm::vec3 FogAlbedo;
			float FogAnisotropy;
			float NearPlane;
			float FarPlane;
			uint32_t TilesBufferWidth;
			uint32_t DownscaleFactor;
			uint32_t HasDirLight;
		} pushData;
		static_assert(sizeof(PushDataVol) <= 128);

		pushData.CameraPos = m_Renderer.GetViewPosition();
		pushData.VolumetricMaxScatteringDist = m_VolumetricSettings.MaxScatteringDistance;
		pushData.Size = volumetricsImageSize;
		pushData.Time = m_Time;
		pushData.FogAlbedo = m_VolumetricSettings.Albedo;
		pushData.FogAnisotropy = m_VolumetricSettings.Anisotropy;
		pushData.NearPlane = m_Renderer.GetZNear();
		pushData.FarPlane = m_Renderer.GetZFar();
		pushData.TilesBufferWidth = lightCulling->GetTilesBufferWidth();
		pushData.DownscaleFactor = s_DownscaleFactor;
		pushData.HasDirLight = uint32_t(m_Renderer.HasDirectionalLight());

		ConstantData info;
		info.VolumetricSamples = m_VolumetricSettings.Samples;
		if (info != m_Constants)
		{
			m_Constants = info;
			InitPipeline(false, false);
		}

		const auto& gbuffer = m_Renderer.GetGBuffer();
		m_Pipeline->SetImage(m_VolumetricsImage, 0, 0);
		m_Pipeline->SetImageSampler(gbuffer.Depth, Sampler::PointSampler, 0, 1);
		m_Pipeline->SetImageSampler(gbuffer.Normals, Sampler::PointSampler, 0, 2);
		m_Pipeline->SetBuffer(lightCulling->GetCulledPointLightsBuffer(), EG_SCENE_SET, 0);
		m_Pipeline->SetBuffer(lightCulling->GetCulledSpotLightsBuffer(), EG_SCENE_SET, 1);
		m_Pipeline->SetBuffer(lightCulling->GetTiles_Translucent_PL(), EG_SCENE_SET, 2);
		m_Pipeline->SetBuffer(lightCulling->GetTiles_Translucent_SL(), EG_SCENE_SET, 3);
		m_Pipeline->SetBuffer(lightCulling->GetLightsCountersBuffer(), EG_SCENE_SET, 4);
		m_Pipeline->SetBuffer(m_Renderer.GetDirectionalLightBuffer(), EG_SCENE_SET, 5);
		m_Pipeline->SetBuffer(m_Renderer.GetCameraMatricesBuffer(), EG_SCENE_SET, 6);
		m_Pipeline->SetBuffer(m_Renderer.GetLightMatricesBuffer(), EG_SCENE_SET, 7);
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
		m_CompositePipeline->SetImage(input, 0, 1);

		const ImageLayout resultLayout = input->GetLayout();
		const ImageLayout depthLayout = gbuffer.Depth->GetLayout();
		const ImageLayout normalsLayout = gbuffer.Normals->GetLayout();

		cmd->TransitionLayout(input, resultLayout, ImageLayoutType::StorageImage);
		cmd->TransitionLayout(gbuffer.Depth, depthLayout, ImageReadAccess::PixelShaderRead);
		cmd->TransitionLayout(gbuffer.Normals, normalsLayout, ImageReadAccess::PixelShaderRead);

		{
			EG_GPU_TIMING_SCOPED(cmd, "Volumetric Lighting");
			EG_CPU_TIMING_SCOPED("Volumetric Lighting");

			const glm::uvec2 numGroups = CalcNumGroups(volumetricsImageSize, m_Pipeline->GetWorkGroupSize());

			cmd->TransitionLayout(m_VolumetricsImage, ImageLayoutType::Unknown, ImageLayoutType::StorageImage);
			cmd->Dispatch(m_Pipeline, numGroups, &pushData);
			cmd->TransitionLayout(m_VolumetricsImage, ImageLayoutType::StorageImage, ImageReadAccess::PixelShaderRead);
			++stats.Dispatches;
		}
		cmd->TransitionLayout(gbuffer.Depth, ImageReadAccess::PixelShaderRead, depthLayout);
		cmd->TransitionLayout(gbuffer.Normals, ImageReadAccess::PixelShaderRead, normalsLayout);

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

			const glm::uvec2 numGroups = CalcNumGroups(volumetricsImageSize, m_GuassianPipeline->GetWorkGroupSize());
			cmd->TransitionLayout(m_VolumetricsImageBlurred, ImageLayoutType::Unknown, ImageLayoutType::StorageImage);
			cmd->Dispatch(m_GuassianPipeline, numGroups, &pushDataComp);
			cmd->TransitionLayout(m_VolumetricsImageBlurred, ImageLayoutType::StorageImage, ImageReadAccess::PixelShaderRead);
			++stats.Dispatches;
		}

		pushDataComp.Size = size;
		pushDataComp.TexelSize = 1.f / glm::vec2(pushDataComp.Size);

		{
			EG_GPU_TIMING_SCOPED(cmd, "Volumetric Composite");
			EG_CPU_TIMING_SCOPED("Volumetric Composite");

			const glm::uvec2 numGroups = CalcNumGroups(size, m_CompositePipeline->GetWorkGroupSize());
			cmd->Dispatch(m_CompositePipeline, numGroups, &pushDataComp);
			++stats.Dispatches;
		}

		cmd->TransitionLayout(input, ImageLayoutType::StorageImage, resultLayout);
	}

	void VolumetricLightTask::InitWithOptions(const SceneRendererSettings& settings)
	{
		bool bReloadPipeline = false;
		bool bVolumetricFogChanged = false;
		if (m_VolumetricSettings != settings.VolumetricSettings)
		{
			bReloadPipeline |= m_VolumetricSettings.Samples != settings.VolumetricSettings.Samples;
			bVolumetricFogChanged = m_VolumetricSettings.bFogEnable != settings.VolumetricSettings.bFogEnable;
			bReloadPipeline |= bVolumetricFogChanged;

			m_VolumetricSettings = settings.VolumetricSettings;
			m_Constants.VolumetricSamples = m_VolumetricSettings.Samples;

			if (m_VolumetricSettings.bEnable)
			{
				if (!m_VolumetricsImage)
				{
					ImageSpecifications specs;
					specs.Format = ImageFormat::R16G16B16A16_Float;
					specs.Size = glm::max(m_Renderer.GetHDROutput()->GetSize() / 2u, glm::uvec3(1u));
					specs.Usage = ImageUsage::ColorAttachment | ImageUsage::Sampled | ImageUsage::Storage;
					m_VolumetricsImage = Image::Create(specs, "PBR_Volumetric");
				}
			}
			else
				m_VolumetricsImage.reset();
		}

		const bool translucentShadowsChanged = bTranslucentShadows != settings.bTranslucentShadows;
		bReloadPipeline |= translucentShadowsChanged;
		bTranslucentShadows = settings.bTranslucentShadows;

		if (bReloadPipeline)
			InitPipeline(translucentShadowsChanged, bVolumetricFogChanged);
	}

	void VolumetricLightTask::OnResize(glm::uvec2 size)
	{
		const glm::uvec2 halfSize = glm::max(size / s_DownscaleFactor, glm::uvec2(1u));
		m_VolumetricsImage->Resize(glm::uvec3(halfSize, 1u));
		m_VolumetricsImageBlurred->Resize(glm::uvec3(halfSize, 1u));
	}

	void VolumetricLightTask::InitPipeline(bool translucentShadowsChanged, bool bVolumetricFogChanged)
	{
		ShaderSpecializationInfo constants;
		constants.MapEntries.push_back({ 0, 0, sizeof(uint32_t) });
		constants.Data = &m_Constants;
		constants.Size = sizeof(m_Constants);

		if (m_Pipeline)
		{
			auto state = m_Pipeline->GetState();
			state.ComputeSpecializationInfo = constants;
			m_Pipeline->SetState(state);

			bool bUpdateDefines = false;
			auto defines = state.ComputeShader->GetDefines();
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
