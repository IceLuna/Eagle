#include "egpch.h"
#include "PBRPassTask.h"

#include "Eagle/Renderer/RenderManager.h"
#include "Eagle/Renderer/SceneRenderer.h"
#include "Eagle/Renderer/VidWrappers/Image.h"
#include "Eagle/Renderer/VidWrappers/Texture.h"
#include "Eagle/Renderer/VidWrappers/RenderCommandManager.h"

#include "Eagle/Debug/CPUTimings.h"
#include "Eagle/Debug/GPUTimings.h"

namespace Eagle
{
	PBRPassTask::PBRPassTask(SceneRenderer& renderer, const Ref<Image>& renderTo) 
		: RendererTask(renderer)
		, m_ResultImage(renderTo)
	{
		InitPipeline();

		BufferSpecifications cameraViewDataBufferSpecs;
		cameraViewDataBufferSpecs.Size = sizeof(glm::mat4);
		cameraViewDataBufferSpecs.Usage = BufferUsage::UniformBuffer | BufferUsage::TransferDst;
		cameraViewDataBufferSpecs.Layout = BufferReadAccess::Uniform;
		m_CameraViewDataBuffer = Buffer::Create(cameraViewDataBufferSpecs, "CameraViewData");
	}

	void PBRPassTask::RecordCommandBuffer(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "PBR Pass");
		EG_CPU_TIMING_SCOPED("PBR Pass");

		auto& cameraViewBuffer = m_CameraViewDataBuffer;
		cmd->Write(cameraViewBuffer, &m_Renderer.GetViewMatrix()[0][0], sizeof(glm::mat4), 0, BufferLayoutType::Unknown, BufferReadAccess::Uniform);
		cmd->TransitionLayout(cameraViewBuffer, BufferReadAccess::Uniform, BufferReadAccess::Uniform);

		struct PushData
		{
			glm::mat4 ViewProjInv;
			glm::vec3 CameraPos;
			float MaxReflectionLOD;
			glm::ivec2 Size;
		} pushData;
		static_assert(sizeof(PushData) <= 128);

		const auto& iblTexture = m_Renderer.GetSkybox();
		const bool bHasIrradiance = iblTexture.operator bool();
		const auto& ibl = bHasIrradiance ? iblTexture : RenderManager::GetDummyIBL();
		const auto& options = m_Renderer.GetOptions_RT();
		auto& gbuffer = m_Renderer.GetGBuffer();

		pushData.ViewProjInv = glm::inverse(m_Renderer.GetViewProjection());
		pushData.CameraPos = m_Renderer.GetViewPosition();
		pushData.MaxReflectionLOD = float(ibl->GetPrefilterImage()->GetMipsCount() - 1);

		ConstantKernelInfo info;
		info.PointLightsCount = (uint32_t)m_Renderer.GetPointLights().size();
		info.SpotLightsCount = (uint32_t)m_Renderer.GetSpotLights().size();
		info.bHasDirLight = m_Renderer.HasDirectionalLight();
		info.bHasIrradiance = bHasIrradiance;
		RecreatePipeline(info);

		if (bRequestedToCreateShadowMapDistribution)
		{
			CreateShadowMapDistribution(cmd, EG_SM_DISTRIBUTION_TEXTURE_SIZE, EG_SM_DISTRIBUTION_FILTER_SIZE);
			bRequestedToCreateShadowMapDistribution = false;
		}

		const Ref<Image>& smDistribution = options.bEnableSoftShadows ? m_ShadowMapDistribution : RenderManager::GetDummyImage3D();
		const Ref<Image>& ssaoImage = options.AO == AmbientOcclusion::SSAO ? m_Renderer.GetSSAOResult() : Texture2D::WhiteTexture->GetImage();

		m_Pipeline->SetBuffer(m_Renderer.GetPointLightsBuffer(), EG_SCENE_SET, EG_BINDING_POINT_LIGHTS);
		m_Pipeline->SetBuffer(m_Renderer.GetSpotLightsBuffer(), EG_SCENE_SET, EG_BINDING_SPOT_LIGHTS);
		m_Pipeline->SetBuffer(m_Renderer.GetDirectionalLightBuffer(), EG_SCENE_SET, EG_BINDING_DIRECTIONAL_LIGHT);
		m_Pipeline->SetImageSampler(gbuffer.AlbedoRoughness, Sampler::PointSampler, EG_SCENE_SET, EG_BINDING_ALBEDO_ROUGHNESS_TEXTURE);
		m_Pipeline->SetImageSampler(gbuffer.Geometry_Shading_Normals, Sampler::PointSampler, EG_SCENE_SET, EG_BINDING_GEOMETRY_SHADING_NORMALS_TEXTURE);
		m_Pipeline->SetImageSampler(gbuffer.Emissive, Sampler::PointSampler, EG_SCENE_SET, EG_BINDING_EMISSIVE_TEXTURE);
		m_Pipeline->SetImageSampler(gbuffer.Depth, Sampler::PointSampler, EG_SCENE_SET, EG_BINDING_DEPTH_TEXTURE);
		m_Pipeline->SetImageSampler(gbuffer.MaterialData, Sampler::PointSampler, EG_SCENE_SET, EG_BINDING_MATERIAL_DATA_TEXTURE);
		m_Pipeline->SetImageSampler(ibl->GetIrradianceImage(), Sampler::PointSampler, EG_SCENE_SET, EG_BINDING_IRRADIANCE_MAP);
		m_Pipeline->SetImageSampler(ibl->GetPrefilterImage(), ibl->GetPrefilterImageSampler(), EG_SCENE_SET, EG_BINDING_PREFILTER_MAP);
		m_Pipeline->SetImageSampler(RenderManager::GetBRDFLUTImage(), Sampler::PointSampler, EG_SCENE_SET, EG_BINDING_BRDF_LUT);
		m_Pipeline->SetBuffer(m_CameraViewDataBuffer, EG_SCENE_SET, EG_BINDING_CAMERA_VIEW);
		m_Pipeline->SetImageSampler(smDistribution, Sampler::PointSampler, EG_SCENE_SET, EG_BINDING_SM_DISTRIBUTION);
		m_Pipeline->SetImageSampler(ssaoImage, Sampler::PointSampler, EG_SCENE_SET, EG_BINDING_SSAO);
		m_Pipeline->SetImageSamplerArray(m_Renderer.GetDirectionalLightShadowMaps(), m_Renderer.GetDirectionalLightShadowMapsSamplers(), EG_SCENE_SET, EG_BINDING_CSM_SHADOW_MAPS);
		m_Pipeline->SetImageSamplerArray(m_Renderer.GetPointLightShadowMaps(), m_Renderer.GetPointLightShadowMapsSamplers(), EG_SCENE_SET, EG_BINDING_SM_POINT_LIGHT);
		m_Pipeline->SetImageSamplerArray(m_Renderer.GetSpotLightShadowMaps(), m_Renderer.GetSpotLightShadowMapsSamplers(), EG_SCENE_SET, EG_BINDING_SM_SPOT_LIGHT);

		m_Pipeline->SetImage(m_ResultImage, 2, 0);

		constexpr uint32_t tileSize = 8;
		const glm::uvec2 size = m_ResultImage->GetSize();
		glm::uvec2 numGroups = { glm::ceil(size.x / float(tileSize)), glm::ceil(size.y / float(tileSize)) };
		pushData.Size = size;

		cmd->TransitionLayout(m_ResultImage, m_ResultImage->GetLayout(), ImageLayoutType::StorageImage);
		cmd->TransitionLayout(gbuffer.Depth, gbuffer.Depth->GetLayout(), ImageReadAccess::PixelShaderRead);
		cmd->Dispatch(m_Pipeline, numGroups.x, numGroups.y, 1, &pushData);
		cmd->TransitionLayout(gbuffer.Depth, gbuffer.Depth->GetLayout(), ImageLayoutType::DepthStencilWrite);
		cmd->TransitionLayout(m_ResultImage, m_ResultImage->GetLayout(), ImageReadAccess::PixelShaderRead);
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
	
	void PBRPassTask::RecreatePipeline(const ConstantKernelInfo& info)
	{
		if (info == m_KernelInfo)
			return;

		m_KernelInfo = info;

		auto& defines = m_ShaderDefines;
		defines["EG_POINT_LIGHTS_COUNT"] = std::to_string(info.PointLightsCount);
		defines["EG_SPOT_LIGHTS_COUNT"] = std::to_string(info.SpotLightsCount);

		auto it = defines.find("EG_HAS_DIR_LIGHT");
		if (info.bHasDirLight)
		{
			if (it == defines.end())
				defines["EG_HAS_DIR_LIGHT"] = "";
		}
		else
		{
			if (it != defines.end())
				defines.erase(it);
		}

		it = defines.find("EG_HAS_IRRADIANCE");
		if (info.bHasIrradiance)
		{
			if (it == defines.end())
				defines["EG_HAS_IRRADIANCE"] = "";
		}
		else
		{
			if (it != defines.end())
				defines.erase(it);
		}

		m_Shader->SetDefines(defines);
	}

	void PBRPassTask::InitPipeline()
	{
		const auto& info = m_KernelInfo;
		
		auto& defines = m_ShaderDefines;
		defines["EG_POINT_LIGHTS_COUNT"] = std::to_string(info.PointLightsCount);
		defines["EG_SPOT_LIGHTS_COUNT"] = std::to_string(info.SpotLightsCount);
		
		auto it = defines.find("EG_HAS_DIR_LIGHT");
		if (info.bHasDirLight)
		{
			if (it == defines.end())
				defines["EG_HAS_DIR_LIGHT"] = "";
		}
		else
		{
			if (it != defines.end())
				defines.erase(it);
		}
		
		it = defines.find("EG_HAS_IRRADIANCE");
		if (info.bHasIrradiance)
		{
			if (it == defines.end())
				defines["EG_HAS_IRRADIANCE"] = "";
		}
		else
		{
			if (it != defines.end())
				defines.erase(it);
		}

		m_Shader = Shader::Create("assets/shaders/pbr_shade.comp", ShaderType::Compute, defines);
		PipelineComputeState state;
		state.ComputeShader = m_Shader;

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
		m_ShadowMapDistribution = Image::Create(specs, "Distribution Texture");

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
