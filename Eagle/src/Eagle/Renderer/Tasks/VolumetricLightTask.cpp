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
		specs.Format = ImageFormat::R16G16B16A16_Float; // rgb - in-scattered light, a - transmittance
		specs.Size = halfSize;
		specs.Usage = ImageUsage::Sampled | ImageUsage::Storage;
		m_VolumetricsImage = Image::Create(specs, "PBR_Volumetric");
		m_VolumetricsImageBlurred = Image::Create(specs, "PBR_Volumetric_Blurred");

		specs.Format = ImageFormat::R32_Float;
		m_VolumetricsDepthImage = Image::Create(specs, "PBR_Volumetric_Depth"); // Linear depth that each low-res pixel was traced to

		const auto& options = m_Renderer.GetOptions();
		m_VolumetricSettings = options.VolumetricSettings;
		bTranslucentShadows = options.bTranslucentShadows;

		{
			BufferSpecifications specs{};
			specs.Size = sizeof(DispatchIndirectArgs) * 2; // For blur & composite passes
			specs.Usage = BufferUsage::TransferDst | BufferUsage::StorageBuffer | BufferUsage::IndirectBuffer;
			specs.Layout = BufferLayoutType::StorageBuffer;
			m_IndirectArgs = Buffer::Create(specs, "VolumetricLights_IndirectArgs");
		}

		{
			PipelineComputeState state{};
			state.ComputeShader = Shader::Create("volumetrics/prepare_args.comp", ShaderType::Compute);
			m_PrepareArgsPipeline = PipelineCompute::Create(state);
		}

		InitPipeline();
    }

	void VolumetricLightTask::RecordCommandBuffer(const Ref<CommandBuffer>& cmd)
	{
		if (!m_Renderer.HasVolumetricLights())
			return;

		EG_GPU_TIMING_SCOPED(cmd, "Volumetric Light Pass");
		EG_CPU_TIMING_SCOPED("Volumetric Light Pass");

		auto& stats = m_Renderer.GetStats();
		const auto& input = m_Renderer.GetHDROutput();

		const glm::uvec2 size = input->GetSize();
		const glm::uvec2 volumetricsImageSize = m_VolumetricsImage->GetSize();
		const Ref<LightCullingTask>& lightCulling = m_Renderer.GetLightCullingTask();

		const Timestep ts = Application::Get().GetTimestep();
		m_Time += ts * m_VolumetricSettings.FogSpeed;

		struct VolumetricPushData
		{
			glm::vec3 CameraPos;
			float VolumetricMaxScatteringDist;
			glm::ivec2 Size;
			float Time;
			uint32_t DownscaleFactor;
			glm::vec3 FogAlbedo;
			float FogAnisotropy;
			uint32_t TilesBufferWidth;
			uint32_t HasDirLight;
			float Density;
		} volumetricPushData;
		static_assert(sizeof(VolumetricPushData) <= 128);

		volumetricPushData.CameraPos = m_Renderer.GetViewPosition();
		volumetricPushData.VolumetricMaxScatteringDist = m_VolumetricSettings.MaxScatteringDistance;
		volumetricPushData.Size = volumetricsImageSize;
		volumetricPushData.Time = m_Time;
		volumetricPushData.FogAlbedo = m_VolumetricSettings.Albedo;
		volumetricPushData.FogAnisotropy = m_VolumetricSettings.Anisotropy;
		volumetricPushData.TilesBufferWidth = lightCulling->GetTilesBufferWidth();
		volumetricPushData.DownscaleFactor = s_DownscaleFactor;
		volumetricPushData.HasDirLight = uint32_t(m_Renderer.HasDirectionalLight());
		volumetricPushData.Density = glm::max(m_VolumetricSettings.Density, 0.f);

		const auto& gbuffer = m_Renderer.GetGBuffer();
		m_Pipeline->SetImage(m_VolumetricsImage, 0, 0);
		m_Pipeline->SetImage(m_VolumetricsDepthImage, 0, 1);
		m_Pipeline->SetImageSampler(gbuffer.Depth, Sampler::PointSamplerClamp, 0, 2);
		m_Pipeline->SetBuffer(m_IndirectArgs, 0, 3);
		m_Pipeline->SetBuffer(lightCulling->GetCulledPointLightsBuffer(), EG_SCENE_SET, 0);
		m_Pipeline->SetBuffer(lightCulling->GetCulledSpotLightsBuffer(), EG_SCENE_SET, 1);
		m_Pipeline->SetBuffer(lightCulling->GetTiles_Translucent_PL(), EG_SCENE_SET, 2);
		m_Pipeline->SetBuffer(lightCulling->GetTiles_Translucent_SL(), EG_SCENE_SET, 3);
		m_Pipeline->SetBuffer(lightCulling->GetLightsCountersBuffer(), EG_SCENE_SET, 4);
		m_Pipeline->SetBuffer(m_Renderer.GetDirectionalLightBuffer(), EG_SCENE_SET, 5);
		m_Pipeline->SetBuffer(m_Renderer.GetCameraMatricesBuffer(), EG_SCENE_SET, 6);
		m_Pipeline->SetBuffer(m_Renderer.GetLightMatricesBuffer(), EG_SCENE_SET, 7);
		m_Pipeline->SetImageSamplerArray(m_Renderer.GetDirectionalLightShadowMaps(), m_Renderer.GetShadowMapPCFSampler(), 2, 0);
		m_Pipeline->SetImageSamplerArray(m_Renderer.GetPointLightShadowMaps(), m_Renderer.GetShadowMapPCFSampler(), 3, 0);
		m_Pipeline->SetImageSamplerArray(m_Renderer.GetSpotLightShadowMaps(), m_Renderer.GetShadowMapPCFSampler(), 4, 0);

		if (bTranslucentShadows)
		{
			m_Pipeline->SetImageSamplerArray(m_Renderer.GetDirectionalLightShadowMapsColored(), m_Renderer.GetColoredShadowMapSampler(), 5, 0);
			m_Pipeline->SetImageSamplerArray(m_Renderer.GetPointLightShadowMapsColored(), m_Renderer.GetColoredShadowMapSampler(), 6, 0);
			m_Pipeline->SetImageSamplerArray(m_Renderer.GetSpotLightShadowMapsColored(), m_Renderer.GetColoredShadowMapSampler(), 7, 0);

			m_Pipeline->SetImageSamplerArray(m_Renderer.GetDirectionalLightShadowMapsColoredDepth(), m_Renderer.GetShadowMapPointSampler(), 8, 0);
			m_Pipeline->SetImageSamplerArray(m_Renderer.GetPointLightShadowMapsColoredDepth(), m_Renderer.GetShadowMapPointSampler(), 9, 0);
			m_Pipeline->SetImageSamplerArray(m_Renderer.GetSpotLightShadowMapsColoredDepth(), m_Renderer.GetShadowMapPointSampler(), 10, 0);
		}

		const ImageLayout resultLayout = input->GetLayout();
		const ImageLayout depthLayout = gbuffer.Depth->GetLayout();

		cmd->TransitionLayout(input, resultLayout, ImageLayoutType::StorageImage);
		cmd->TransitionLayout(gbuffer.Depth, depthLayout, ImageReadAccess::PixelShaderRead);

		cmd->FillBuffer(m_IndirectArgs, 0);

		{
			EG_GPU_TIMING_SCOPED(cmd, "Volumetric Lighting");
			EG_CPU_TIMING_SCOPED("Volumetric Lighting");

			const glm::uvec2 numGroups = CalcNumGroups(volumetricsImageSize, m_Pipeline->GetWorkGroupSize());

			cmd->TransitionLayout(m_VolumetricsImage, ImageLayoutType::Unknown, ImageLayoutType::StorageImage);
			cmd->TransitionLayout(m_VolumetricsDepthImage, ImageLayoutType::Unknown, ImageLayoutType::StorageImage);
			cmd->Dispatch(m_Pipeline, numGroups, &volumetricPushData);
			cmd->TransitionLayout(m_VolumetricsImage, ImageLayoutType::StorageImage, ImageReadAccess::PixelShaderRead);
			cmd->TransitionLayout(m_VolumetricsDepthImage, ImageLayoutType::StorageImage, ImageReadAccess::PixelShaderRead);
			++stats.Dispatches;
		}
		cmd->Barrier(m_IndirectArgs);

		const glm::uvec2 blurGroupSize = m_BlurPipeline->GetWorkGroupSize();
		const glm::uvec2 compositeGroupSize = m_CompositePipeline->GetWorkGroupSize();
		{
			EG_GPU_TIMING_SCOPED(cmd, "Volumetric Lights. Prepare args");
			EG_CPU_TIMING_SCOPED("Volumetric Lights. Prepare args");

			struct PrepareArgsPushData
			{
				glm::uvec2 BlurSize;
				glm::uvec2 CompositeSize;
				glm::uvec2 BlurGroupSize;
				glm::uvec2 CompositeGroupSize;
			} prepareArgsPushData;
			prepareArgsPushData.BlurSize = volumetricsImageSize;
			prepareArgsPushData.CompositeSize = size;
			prepareArgsPushData.BlurGroupSize = blurGroupSize;
			prepareArgsPushData.CompositeGroupSize = compositeGroupSize;

			m_PrepareArgsPipeline->SetBuffer(m_IndirectArgs, 0, 0);
			cmd->Dispatch(m_PrepareArgsPipeline, 1, 1, 1, &prepareArgsPushData);
			cmd->TransitionLayout(m_IndirectArgs, BufferLayoutType::StorageBuffer, BufferReadAccess::IndirectArgument);
		}

		struct BlurPushData
		{
			glm::ivec2 Size;
			glm::vec2 TexelSize;
		};

		{
			EG_GPU_TIMING_SCOPED(cmd, "Volumetric Blur");
			EG_CPU_TIMING_SCOPED("Volumetric Blur");

			m_BlurPipeline->SetImageSampler(m_VolumetricsImage, Sampler::PointSamplerClamp, 0, 0);
			m_BlurPipeline->SetImageSampler(m_VolumetricsDepthImage, Sampler::PointSamplerClamp, 0, 1);
			m_BlurPipeline->SetImage(m_VolumetricsImageBlurred, 0, 2);

			BlurPushData blurPushData;
			blurPushData.Size = volumetricsImageSize;
			blurPushData.TexelSize = 1.f / glm::vec2(volumetricsImageSize);

			cmd->TransitionLayout(m_VolumetricsImageBlurred, ImageLayoutType::Unknown, ImageLayoutType::StorageImage);
			cmd->DispatchIndirect(m_BlurPipeline, m_IndirectArgs, 0, &blurPushData);
			cmd->TransitionLayout(m_VolumetricsImageBlurred, ImageLayoutType::StorageImage, ImageReadAccess::PixelShaderRead);
			++stats.Dispatches;
		}

		{
			EG_GPU_TIMING_SCOPED(cmd, "Volumetric Composite");
			EG_CPU_TIMING_SCOPED("Volumetric Composite");

			struct CompositePushData
			{
				glm::ivec2 Size;
				glm::vec2 TexelSize;
				uint32_t DownscaleFactor;
			} compositePushData;
			compositePushData.Size = size;
			compositePushData.TexelSize = 1.f / glm::vec2(size);
			compositePushData.DownscaleFactor = s_DownscaleFactor;

			m_CompositePipeline->SetImageSampler(m_VolumetricsImageBlurred, Sampler::PointSamplerClamp, 0, 0);
			m_CompositePipeline->SetImageSampler(m_VolumetricsDepthImage, Sampler::PointSamplerClamp, 0, 1);
			m_CompositePipeline->SetImageSampler(gbuffer.Depth, Sampler::PointSamplerClamp, 0, 2);
			m_CompositePipeline->SetImage(input, 0, 3);
			m_CompositePipeline->SetBuffer(m_Renderer.GetCameraMatricesBuffer(), 0, 4);

			cmd->DispatchIndirect(m_CompositePipeline, m_IndirectArgs, sizeof(DispatchIndirectArgs), &compositePushData);
			++stats.Dispatches;
		}

		cmd->TransitionLayout(gbuffer.Depth, ImageReadAccess::PixelShaderRead, depthLayout);
		cmd->TransitionLayout(input, ImageLayoutType::StorageImage, resultLayout);
		cmd->TransitionLayout(m_IndirectArgs, BufferReadAccess::IndirectArgument, BufferLayoutType::StorageBuffer);
	}

	void VolumetricLightTask::InitWithOptions(const SceneRendererSettings& settings)
	{
		const bool bSamplesChanged = m_VolumetricSettings.Samples != settings.VolumetricSettings.Samples;
		const bool bFogChanged = m_VolumetricSettings.bFogEnable != settings.VolumetricSettings.bFogEnable;
		const bool bTranslucentShadowsChanged = bTranslucentShadows != settings.bTranslucentShadows;

		m_VolumetricSettings = settings.VolumetricSettings;
		bTranslucentShadows = settings.bTranslucentShadows;

		if (bSamplesChanged || bFogChanged || bTranslucentShadowsChanged)
			InitPipeline();
	}

	void VolumetricLightTask::OnResize(glm::uvec2 size)
	{
		const glm::uvec2 halfSize = glm::max(size / s_DownscaleFactor, glm::uvec2(1u));
		m_VolumetricsImage->Resize(glm::uvec3(halfSize, 1u));
		m_VolumetricsImageBlurred->Resize(glm::uvec3(halfSize, 1u));
		m_VolumetricsDepthImage->Resize(glm::uvec3(halfSize, 1u));
	}

	ShaderDefines VolumetricLightTask::GetShaderDefines() const
	{
		ShaderDefines defines;
		if (bTranslucentShadows)
			defines["EG_TRANSLUCENT_SHADOWS"] = "";
		if (m_VolumetricSettings.bFogEnable)
			defines["EG_VOLUMETRIC_FOG"] = "";
		return defines;
	}

	void VolumetricLightTask::InitPipeline()
	{
		m_Constants.VolumetricSamples = glm::max(m_VolumetricSettings.Samples, 1u);
		const ShaderDefines defines = GetShaderDefines();

		if (m_Pipeline)
		{
			// The pipeline references `m_Constants`, so it's enough to just recreate it
			const Ref<Shader>& shader = m_Pipeline->GetState().ComputeShader;
			if (shader->GetDefines() != defines)
				shader->SetDefines(defines);
			else
				m_Pipeline->Recreate();
		}
		else
		{
			ShaderSpecializationInfo constants;
			constants.MapEntries.push_back({ 0, uint32_t(offsetof(ConstantData, VolumetricSamples)), sizeof(uint32_t) });
			constants.Data = &m_Constants;
			constants.Size = sizeof(m_Constants);

			PipelineComputeState state;
			state.ComputeShader = Shader::Create("volumetrics/shade.comp", ShaderType::Compute, defines);
			state.ComputeSpecializationInfo = constants;

			m_Pipeline = PipelineCompute::Create(state);
		}

		if (!m_CompositePipeline)
		{
			PipelineComputeState state;
			state.ComputeShader = Shader::Create("volumetrics/composite.comp", ShaderType::Compute);
			m_CompositePipeline = PipelineCompute::Create(state);
		}

		if (!m_BlurPipeline)
		{
			PipelineComputeState state;
			state.ComputeShader = Shader::Create("volumetrics/blur.comp", ShaderType::Compute);
			m_BlurPipeline = PipelineCompute::Create(state);
		}
	}
}
