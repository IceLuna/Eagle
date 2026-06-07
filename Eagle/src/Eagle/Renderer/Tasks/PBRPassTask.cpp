#include "egpch.h"
#include "PBRPassTask.h"

#include "Eagle/Renderer/RenderManager.h"
#include "Eagle/Renderer/SceneRenderer.h"
#include "Eagle/Renderer/VidWrappers/Image.h"
#include "Eagle/Renderer/VidWrappers/Texture.h"
#include "Eagle/Renderer/VidWrappers/RenderCommandManager.h"

#include "Eagle/Asset/Asset.h"

#include "Eagle/Debug/CPUTimings.h"
#include "Eagle/Debug/GPUTimings.h"

namespace Eagle
{
	PBRPassTask::PBRPassTask(SceneRenderer& renderer) 
		: RendererTask(renderer)
	{
		const auto& options = m_Renderer.GetOptions();

		m_ShaderDefines["EG_SCREEN_SPACE_SHADOWS"] = "";
		SetVisualizeCascades(options.bVisualizeCascades);
		SetSoftShadowsEnabled(options.bEnableSoftShadows);
		SetSSAOEnabled(options.AO != AmbientOcclusion::None);
		SetCSMSmoothTransitionEnabled(options.bEnableCSMSmoothTransition);
		SetTranslucentShadowsEnabled(options.bTranslucentShadows);
		InitPipeline();
	}

	void PBRPassTask::RecordCommandBuffer(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "PBR Pass");
		EG_CPU_TIMING_SCOPED("PBR Pass");

		struct PushData
		{
			glm::vec3 CameraPos;
			float MaxReflectionLOD;
			glm::ivec2 Size;
			float CascadesSmoothTransitionAlpha;
			float IBLIntensity;
			uint32_t TilesBufferWidth;
			uint32_t HasDirLight;
		} pushData;
		static_assert(sizeof(PushData) <= 128);

		const auto& iblAsset = m_Renderer.GetSkybox();
		const bool bHasIrradiance = m_Renderer.IsSkyboxEnabled() && iblAsset.operator bool() && iblAsset->GetTexture()->IsLoaded();
		const auto& ibl = bHasIrradiance ? iblAsset->GetTexture() : RenderManager::GetDummyIBL();
		const auto& options = m_Renderer.GetOptions_RT();
		auto& gbuffer = m_Renderer.GetGBuffer();
		const Ref<LightCullingTask>& lightCulling = m_Renderer.GetLightCullingTask();

		pushData.CameraPos = m_Renderer.GetViewPosition();
		pushData.MaxReflectionLOD = float(ibl->GetPrefilterImage()->GetMipsCount() - 1);
		pushData.CascadesSmoothTransitionAlpha = options.InternalState.CascadesSmoothTransitionAlpha;
		pushData.IBLIntensity = m_Renderer.GetSkyboxIntensity();
		pushData.TilesBufferWidth = lightCulling->GetTilesBufferWidth();
		pushData.HasDirLight = uint32_t(m_Renderer.HasDirectionalLight());

		const uint32_t newIrradiance = bHasIrradiance ? 1u : 0u;
		if (this->bHasIrradiance != newIrradiance)
		{
			this->bHasIrradiance = newIrradiance;
			RecreatePipeline();
		}

		if (bRequestedToCreateShadowMapDistribution)
		{
			CreateShadowMapDistribution(cmd, EG_SM_DISTRIBUTION_TEXTURE_SIZE, EG_SM_DISTRIBUTION_FILTER_SIZE);
			bRequestedToCreateShadowMapDistribution = false;
		}

		const Ref<Image>& smDistribution = options.bEnableSoftShadows ? m_ShadowMapDistribution : RenderManager::GetDummyImage3D();
		const Ref<Image>& ssaoImage = options.AO == AmbientOcclusion::SSAO ? m_Renderer.GetSSAOResult()
									: options.AO == AmbientOcclusion::GTAO ? m_Renderer.GetGTAOResult()
									: Texture2D::WhiteTexture->GetImage();

		m_Pipeline->SetBuffer(m_Renderer.GetLightMatricesBuffer(), EG_SCENE_SET, EG_BINDING_LIGHT_MATRICES);
		m_Pipeline->SetBuffer(lightCulling->GetCulledPointLightsBuffer(), EG_SCENE_SET, EG_BINDING_POINT_LIGHTS);
		m_Pipeline->SetBuffer(lightCulling->GetCulledSpotLightsBuffer(), EG_SCENE_SET, EG_BINDING_SPOT_LIGHTS);
		m_Pipeline->SetBuffer(lightCulling->GetTiles_Opaque_PL(), EG_SCENE_SET, EG_BINDING_POINT_LIGHT_TILE_BUCKETS);
		m_Pipeline->SetBuffer(lightCulling->GetTiles_Opaque_SL(), EG_SCENE_SET, EG_BINDING_SPOT_LIGHT_TILE_BUCKETS);
		m_Pipeline->SetBuffer(lightCulling->GetLightsCountersBuffer(), EG_SCENE_SET, EG_BINDING_LIGHTS_COUNT);
		m_Pipeline->SetBuffer(m_Renderer.GetDirectionalLightBuffer(), EG_SCENE_SET, EG_BINDING_DIRECTIONAL_LIGHT);
		m_Pipeline->SetImageSampler(gbuffer.Albedo, Sampler::PointSampler, EG_SCENE_SET, EG_BINDING_ALBEDO_ROUGHNESS_TEXTURE);
		m_Pipeline->SetImageSampler(gbuffer.Normals, Sampler::PointSampler, EG_SCENE_SET, EG_BINDING_GEOMETRY_SHADING_NORMALS_TEXTURE);
		m_Pipeline->SetImageSampler(gbuffer.Emissive, Sampler::PointSampler, EG_SCENE_SET, EG_BINDING_EMISSIVE_TEXTURE);
		m_Pipeline->SetImageSampler(gbuffer.Depth, Sampler::PointSampler, EG_SCENE_SET, EG_BINDING_DEPTH_TEXTURE);
		m_Pipeline->SetImageSampler(gbuffer.MaterialData, Sampler::PointSampler, EG_SCENE_SET, EG_BINDING_MATERIAL_DATA_TEXTURE);
		m_Pipeline->SetImageSampler(ibl->GetIrradianceImage(), Sampler::PointSamplerClamp, EG_SCENE_SET, EG_BINDING_IRRADIANCE_MAP);
		m_Pipeline->SetImageSampler(ibl->GetPrefilterImage(), ibl->GetPrefilterImageSampler(), EG_SCENE_SET, EG_BINDING_PREFILTER_MAP);
		m_Pipeline->SetImageSampler(RenderManager::GetBRDFLUTImage(), Sampler::PointSamplerClamp, EG_SCENE_SET, EG_BINDING_BRDF_LUT);
		m_Pipeline->SetBuffer(m_Renderer.GetCameraMatricesBuffer(), EG_SCENE_SET, EG_BINDING_CAMERA_VIEW);
		m_Pipeline->SetImageSampler(smDistribution, Sampler::PointSampler, EG_SCENE_SET, EG_BINDING_SM_DISTRIBUTION);
		m_Pipeline->SetImageSampler(ssaoImage, Sampler::PointSampler, EG_SCENE_SET, EG_BINDING_SSAO);
		m_Pipeline->SetImageSampler(m_Renderer.GetScreenSpaceShadows(), Sampler::PointSampler, EG_SCENE_SET, EG_BINDING_SCREEN_SPACE_SHADOWS);

		m_Pipeline->SetImageSamplerArray(m_Renderer.GetDirectionalLightShadowMaps(), m_Renderer.GetShadowMapPCFSampler(), EG_SCENE_SET, EG_BINDING_CSM_SHADOW_MAPS);
		m_Pipeline->SetImageSamplerArray(m_Renderer.GetPointLightShadowMaps(), m_Renderer.GetShadowMapPCFSampler(), 2, 0);
		m_Pipeline->SetImageSamplerArray(m_Renderer.GetSpotLightShadowMaps(), m_Renderer.GetShadowMapPCFSampler(), 3, 0);

		if (bTranslucentShadows)
		{
			m_Pipeline->SetImageSamplerArray(m_Renderer.GetDirectionalLightShadowMapsColored(), m_Renderer.GetColoredShadowMapSampler(), EG_SCENE_SET, EG_BINDING_CSMC_SHADOW_MAPS);
			m_Pipeline->SetImageSamplerArray(m_Renderer.GetPointLightShadowMapsColored(), m_Renderer.GetColoredShadowMapSampler(), 4, 0);
			m_Pipeline->SetImageSamplerArray(m_Renderer.GetSpotLightShadowMapsColored(), m_Renderer.GetColoredShadowMapSampler(), 5, 0);
		}

		const auto& resultImage = m_Renderer.GetHDROutput();
		m_Pipeline->SetImage(resultImage, 6, 0);

		const glm::uvec2 size = resultImage->GetSize();
		const glm::uvec3 groupSize = m_Pipeline->GetWorkGroupSize();
		const glm::uvec2 numGroups = CalcNumGroups(size, groupSize);
		pushData.Size = size;

		const ImageLayout resultLayout = resultImage->GetLayout();
		const ImageLayout depthLayout = gbuffer.Depth->GetLayout();
		const ImageLayout albedoLayout = gbuffer.Albedo->GetLayout();
		const ImageLayout normalsLayout = gbuffer.Normals->GetLayout();
		const ImageLayout emissiveLayout = gbuffer.Emissive->GetLayout();
		const ImageLayout materialLayout = gbuffer.MaterialData->GetLayout();

		cmd->TransitionLayout(resultImage, resultLayout, ImageLayoutType::StorageImage);
		cmd->TransitionLayout(gbuffer.Depth, depthLayout, ImageReadAccess::PixelShaderRead);
		cmd->TransitionLayout(gbuffer.Albedo, albedoLayout, ImageReadAccess::PixelShaderRead);
		cmd->TransitionLayout(gbuffer.Normals, normalsLayout, ImageReadAccess::PixelShaderRead);
		cmd->TransitionLayout(gbuffer.Emissive, emissiveLayout, ImageReadAccess::PixelShaderRead);
		cmd->TransitionLayout(gbuffer.MaterialData, materialLayout, ImageReadAccess::PixelShaderRead);

		cmd->Dispatch(m_Pipeline, numGroups.x, numGroups.y, 1, &pushData);

		cmd->TransitionLayout(resultImage, ImageLayoutType::StorageImage, resultLayout);
		cmd->TransitionLayout(gbuffer.Depth, ImageReadAccess::PixelShaderRead, depthLayout);
		cmd->TransitionLayout(gbuffer.Albedo, ImageReadAccess::PixelShaderRead, albedoLayout);
		cmd->TransitionLayout(gbuffer.Normals, ImageReadAccess::PixelShaderRead, normalsLayout);
		cmd->TransitionLayout(gbuffer.Emissive, ImageReadAccess::PixelShaderRead, emissiveLayout);
		cmd->TransitionLayout(gbuffer.MaterialData, ImageReadAccess::PixelShaderRead, materialLayout);

		auto& stats = m_Renderer.GetStats();
		++stats.Dispatches;
	}

	bool PBRPassTask::SetSoftShadowsEnabled(bool bEnable)
	{
		if (bSoftShadows == bEnable)
			return false;

		bSoftShadows = bEnable;
		auto& defines = m_ShaderDefines;

		bool bUpdate = false;
		if (bEnable)
		{
			bRequestedToCreateShadowMapDistribution = true; // Basically deffer creation
			defines["EG_SOFT_SHADOWS"] = "";
			bUpdate = true;
		}
		else
		{
			bRequestedToCreateShadowMapDistribution = false;
			auto it = defines.find("EG_SOFT_SHADOWS");
			if (it != defines.end())
			{
				m_ShadowMapDistribution.reset();
				defines.erase(it);
				bUpdate = true;
			}
		}

		return bUpdate;
	}

	bool PBRPassTask::SetVisualizeCascades(bool bVisualize)
	{
		if (bVisualizeCascades == bVisualize)
			return false;

		bVisualizeCascades = bVisualize;
		auto& defines = m_ShaderDefines;

		bool bUpdate = false;
		if (bVisualize)
		{
			defines["EG_ENABLE_CSM_VISUALIZATION"] = "";
			bUpdate = true;
		}
		else
		{
			auto it = defines.find("EG_ENABLE_CSM_VISUALIZATION");
			if (it != defines.end())
			{
				defines.erase(it);
				bUpdate = true;
			}
		}

		return bUpdate;
	}

	bool PBRPassTask::SetSSAOEnabled(bool bEnabled)
	{
		auto& defines = m_ShaderDefines;
		auto it = defines.find("EG_SSAO");

		bool bUpdate = false;
		if (bEnabled)
		{
			if (it == defines.end())
			{
				defines["EG_SSAO"] = "";
				bUpdate = true;
			}
		}
		else
		{
			if (it != defines.end())
			{
				defines.erase(it);
				bUpdate = true;
			}
		}

		return bUpdate;
	}

	bool PBRPassTask::SetCSMSmoothTransitionEnabled(bool bEnabled)
	{
		auto& defines = m_ShaderDefines;
		auto it = defines.find("EG_CSM_SMOOTH_TRANSITION");

		bool bUpdate = false;
		if (bEnabled)
		{
			if (it == defines.end())
			{
				defines["EG_CSM_SMOOTH_TRANSITION"] = "";
				bUpdate = true;
			}
		}
		else
		{
			if (it != defines.end())
			{
				defines.erase(it);
				bUpdate = true;
			}
		}

		return bUpdate;
	}

	bool PBRPassTask::SetTranslucentShadowsEnabled(bool bEnable)
	{
		if (bTranslucentShadows == bEnable)
			return false;

		bTranslucentShadows = bEnable;

		auto& defines = m_ShaderDefines;
		auto it = defines.find("EG_TRANSLUCENT_SHADOWS");

		bool bUpdate = false;
		if (bEnable)
		{
			if (it == defines.end())
			{
				defines["EG_TRANSLUCENT_SHADOWS"] = "";
				bUpdate = true;
			}
		}
		else
		{
			if (it != defines.end())
			{
				defines.erase(it);
				bUpdate = true;
			}
		}

		return bUpdate;
	}
	
	void PBRPassTask::RecreatePipeline()
	{
		ShaderSpecializationInfo constants;
		constants.MapEntries.push_back({ 0, 0, sizeof(uint32_t) });
		constants.Data = &bHasIrradiance;
		constants.Size = sizeof(uint32_t);

		auto state = m_Pipeline->GetState();
		state.ComputeSpecializationInfo = constants;
		m_Pipeline->SetState(state);
	}

	void PBRPassTask::InitPipeline()
	{
		ShaderSpecializationInfo constants;
		constants.MapEntries.push_back({0, 0, sizeof(uint32_t)});
		constants.Data = &bHasIrradiance;
		constants.Size = sizeof(uint32_t);

		if (m_Shader)
			m_Shader->SetDefines(m_ShaderDefines);
		else
			m_Shader = Shader::Create("pbr_shade.comp", ShaderType::Compute, m_ShaderDefines);
		
		PipelineComputeState state;
		state.ComputeShader = m_Shader;
		state.ComputeSpecializationInfo = constants;

		if (m_Pipeline)
			m_Pipeline->SetState(state);
		else
			m_Pipeline = PipelineCompute::Create(state);
	}
	
	void PBRPassTask::CreateShadowMapDistribution(const Ref<CommandBuffer>& cmd, uint32_t windowSize, uint32_t filterSize)
	{
		ImageSpecifications specs;
		specs.Size = glm::uvec3((filterSize * filterSize) / 2, windowSize, windowSize);
		specs.Format = ImageFormat::R32G32B32A32_Float;
		specs.Usage = ImageUsage::Sampled | ImageUsage::TransferDst;
		specs.Layout = ImageLayoutType::Unknown;
		specs.Type = ImageType::Type3D;
		m_ShadowMapDistribution = Image::Create(specs, "Shadow Distribution Texture");

		const size_t dataSize = windowSize * windowSize * filterSize * filterSize * 2;
		std::vector<float> data(dataSize);

		uint32_t index = 0;
		for (uint32_t y = 0; y < windowSize; ++y)
			for (uint32_t x = 0; x < windowSize; ++x)
				for (int v = int(filterSize) - 1; v >= 0; --v)
					for (uint32_t u = 0; u < filterSize; ++u)
					{
						float x = (float(u) + 0.5f + Random::Float(-0.5f, 0.5f)) / float(filterSize);
						float y = (float(v) + 0.5f + Random::Float(-0.5f, 0.5f)) / float(filterSize);

						EG_ASSERT(index + 1 < data.size());

						constexpr float pi = float(3.14159265358979323846);
						constexpr float _2pi = 2.f * pi;
						const float trigonometryArg = _2pi * x;
						const float sqrtf_y = sqrtf(y);
						data[index] = sqrtf_y * cosf(trigonometryArg);
						data[index + 1] = sqrtf_y * sinf(trigonometryArg);
						index += 2;
					}

		cmd->Write(m_ShadowMapDistribution, data.data(), data.size() * sizeof(float), ImageLayoutType::Unknown, ImageReadAccess::PixelShaderRead);
		cmd->TransitionLayout(m_ShadowMapDistribution, ImageReadAccess::PixelShaderRead, ImageReadAccess::PixelShaderRead);
	}
}
