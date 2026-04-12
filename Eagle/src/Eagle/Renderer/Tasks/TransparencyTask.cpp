#include "egpch.h"
#include "TransparencyTask.h"

#include "Eagle/Renderer/RenderManager.h"
#include "Eagle/Renderer/SceneRenderer.h"
#include "Eagle/Renderer/TextureSystem.h"
#include "Eagle/Renderer/MaterialSystem.h"
#include "Eagle/Renderer/VidWrappers/RenderCommandManager.h"
#include "Eagle/Renderer/VidWrappers/Texture.h"
#include "Eagle/Renderer/Tasks/RenderMeshesTask.h"
#include "Eagle/Renderer/Tasks/RenderSkeletalMeshesTask.h"
#include "Eagle/Renderer/Tasks/RenderSpritesTask.h"
#include "Eagle/Renderer/Tasks/RenderTextLitTask.h"

#include "Eagle/Asset/Asset.h"

#include "Eagle/Debug/CPUTimings.h"
#include "Eagle/Debug/GPUTimings.h"

namespace Eagle
{
	struct UniformData
	{
		glm::vec3 CameraPos;
		float MaxReflectionLOD;
		glm::ivec2 Size;
		float MaxShadowDistance2; // Square of distance
		float CascadesSmoothTransitionAlpha;
		float IBLIntensity;
		uint32_t PointLights;
		uint32_t SpotLights;
		uint32_t HasDirLight;
	};

	constexpr static uint32_t s_OITFillValue = 0x0u; // 0xFFFFFFFFu

	TransparencyTask::TransparencyTask(SceneRenderer& renderer)
		: RendererTask(renderer)
	{
		const auto& options = m_Renderer.GetOptions();

		m_Layers = options.TransparencyLayers;
		const std::string layersString = std::to_string(m_Layers);

		auto& defines = m_ShaderDefines;
		defines["EG_OIT_LAYERS"] = layersString;

		SetVisualizeCascades(options.bVisualizeCascades);
		SetSoftShadowsEnabled(options.bEnableSoftShadows);
		SetCSMSmoothTransitionEnabled(options.bEnableCSMSmoothTransition);
		SetStutterlessEnabled(options.bStutterlessShaders);
		SetFogEnabled(options.FogSettings.bEnable);
		bObjectPickingEnabled = options.bEnableObjectPicking;

		m_TransparencyColorShader     = Shader::Create("transparency/transparency_color.frag", ShaderType::Fragment, defines);
		m_TransparencyDepthShader     = Shader::Create("transparency/transparency.frag", ShaderType::Fragment, { {"EG_DEPTH_PASS",     ""}, {"EG_OIT_LAYERS", layersString} });
		m_TransparencyCompositeShader = Shader::Create("transparency/transparency.frag", ShaderType::Fragment, { {"EG_COMPOSITE_PASS", ""}, {"EG_OIT_LAYERS", layersString} });

		m_TransparencyTextDepthShader = Shader::Create("transparency/transparency_text_depth.frag", ShaderType::Fragment, defines);
		m_TransparencyTextColorShader = Shader::Create("transparency/transparency_text_color.frag", ShaderType::Fragment, defines);

		{
			BufferSpecifications specs{};
			specs.Size = sizeof(UniformData);
			specs.Usage = BufferUsage::UniformBuffer | BufferUsage::TransferDst;
			m_UniformBuffer = Buffer::Create(specs, "TransparencyTask_UniformBuffer");
		}

		InitOITBuffer();
		InitMeshPipelines();
		InitSkeletalMeshPipelines();
		InitSpritesPipelines();
		InitTextsPipelines();
		InitCompositePipelines();
		InitEntityIDPipelines();
	}
	
	void TransparencyTask::RecordCommandBuffer(const Ref<CommandBuffer>& cmd)
	{
		if (!m_OITBuffer)
			InitOITBuffer();

		EG_GPU_TIMING_SCOPED(cmd, "Transparency");
		EG_CPU_TIMING_SCOPED("Transparency");

		Prepare(cmd);
		{
			EG_GPU_TIMING_SCOPED(cmd, "Transparency. Clear Buffer");
			EG_CPU_TIMING_SCOPED("Transparency. Clear Buffer");

			// Fill all depth values
			size_t bytesToClear = m_OITBuffer->GetSize() / 2;
			bytesToClear += 4ull - (bytesToClear % 4ull);

			cmd->FillBuffer(m_OITBuffer, s_OITFillValue, 0, bytesToClear);
		}

		RenderMeshesDepth(cmd);
		RenderSkeletalMeshesDepth(cmd);
		RenderSpritesDepth(cmd);
		RenderTextsDepth(cmd);

		RenderMeshesColor(cmd);
		RenderSkeletalMeshesColor(cmd);
		RenderSpritesColor(cmd);
		RenderTextsColor(cmd);

		CompositePass(cmd);
		RenderEntityIDs(cmd);
	}
	
	void TransparencyTask::InitWithOptions(const SceneRendererSettings& settings)
	{
		const bool bLayersChanged = m_Layers != settings.TransparencyLayers;
		bool bReloadShader = bLayersChanged;
		bReloadShader |= SetVisualizeCascades(settings.bVisualizeCascades);
		bReloadShader |= SetSoftShadowsEnabled(settings.bEnableSoftShadows);
		bReloadShader |= SetCSMSmoothTransitionEnabled(settings.bEnableCSMSmoothTransition);
		bReloadShader |= SetFogEnabled(settings.FogSettings.bEnable);
		bObjectPickingEnabled = settings.bEnableObjectPicking;

		const bool bReloadPipeline = SetStutterlessEnabled(settings.bStutterlessShaders);

		if (!bReloadShader && !bReloadPipeline)
			return;

		m_Layers = settings.TransparencyLayers;
		const std::string layersString = std::to_string(m_Layers);
		m_ShaderDefines["EG_OIT_LAYERS"] = layersString;

		if (bReloadPipeline)
			RecreatePipeline(true);
		else if (bReloadShader)
		{
			m_TransparencyColorShader->SetDefines(m_ShaderDefines);
			m_TransparencyTextColorShader->SetDefines(m_ShaderDefines);
		}

		if (bLayersChanged)
		{
			{
				auto defines = m_TransparencyDepthShader->GetDefines();
				defines["EG_OIT_LAYERS"] = layersString;
				m_TransparencyDepthShader->SetDefines(defines);
			}
			{
				auto defines = m_TransparencyCompositeShader->GetDefines();
				defines["EG_OIT_LAYERS"] = layersString;
				m_TransparencyCompositeShader->SetDefines(defines);
			}
			{
				auto defines = m_TransparencyTextDepthShader->GetDefines();
				defines["EG_OIT_LAYERS"] = layersString;
				m_TransparencyTextDepthShader->SetDefines(defines);
			}
			const glm::uvec2 size = m_Renderer.GetViewportSize();
			constexpr size_t formatSize = GetImageFormatBPP(ImageFormat::R32_UInt) / 8u;
			const size_t bufferSize = size_t(size.x * size.y * m_Layers) * s_Stride;
			m_OITBuffer->Resize(bufferSize);
		}
	}

	void TransparencyTask::RenderMeshesDepth(const Ref<CommandBuffer>& cmd)
	{
		const auto& drawData = m_Renderer.GetStaticMeshesDrawData();
		const auto& singleSidedMeshes = drawData.SingleSided.Translucent.DrawData;
		const auto& doubleSidedMeshes = drawData.DoubleSided.Translucent.DrawData;
		if (singleSidedMeshes.empty() && doubleSidedMeshes.empty())
			return;

		EG_GPU_TIMING_SCOPED(cmd, "Transparency. Static Meshes. Depth");
		EG_CPU_TIMING_SCOPED("Transparency. Static Meshes. Depth");

		const auto& buffers = m_Renderer.GetStaticMeshesBuffers();
		const auto& transformsBuffer = m_Renderer.GetMeshTransformsBuffer();

		m_MeshesDepthPipeline->SetBuffer(transformsBuffer, 0, 0);
		m_MeshesDepthPipeline->SetBuffer(m_Renderer.GetCameraMatricesBuffer(), 0, 1);
		m_MeshesDepthPipeline->SetBuffer(m_OITBuffer, 5, 0);
		m_MeshesDepthPipeline->SetBuffer(m_UniformBuffer, 5, 1);

		auto& stats = m_Renderer.GetStats();
		if (!singleSidedMeshes.empty())
		{
			cmd->SetGraphicsCullMode(CullMode::Back);
			RenderMeshesTask::Draw(cmd, m_MeshesDepthPipeline, singleSidedMeshes, buffers, stats);
			cmd->Barrier(m_OITBuffer);
		}
		if (!doubleSidedMeshes.empty())
		{
			cmd->SetGraphicsCullMode(CullMode::None);
			RenderMeshesTask::Draw(cmd, m_MeshesDepthPipeline, doubleSidedMeshes, buffers, stats);
			cmd->Barrier(m_OITBuffer);
		}
	}

	void TransparencyTask::RenderSkeletalMeshesDepth(const Ref<CommandBuffer>& cmd)
	{
		const auto& singleSidedMeshes = m_Renderer.GetSkeletalMeshesDrawData().SingleSided.Translucent.DrawData;
		const auto& doubleSidedMeshes = m_Renderer.GetSkeletalMeshesDrawData().DoubleSided.Translucent.DrawData;
		if (singleSidedMeshes.empty() && doubleSidedMeshes.empty())
			return;

		EG_GPU_TIMING_SCOPED(cmd, "Transparency. Skeletal Meshes. Depth");
		EG_CPU_TIMING_SCOPED("Transparency. Skeletal Meshes. Depth");

		const auto& buffers = m_Renderer.GetSkeletalMeshesBuffers();
		const auto& vb = m_Renderer.GetSkinnedVertices();

		m_SkeletalMeshesDepthPipeline->SetBuffer(vb, 0, 0);
		m_SkeletalMeshesDepthPipeline->SetBuffer(m_Renderer.GetCameraMatricesBuffer(), 0, 1);
		m_SkeletalMeshesDepthPipeline->SetBuffer(m_OITBuffer, 5, 0);
		m_SkeletalMeshesDepthPipeline->SetBuffer(m_UniformBuffer, 5, 1);

		auto& stats = m_Renderer.GetStats();
		if (!singleSidedMeshes.empty())
		{
			cmd->SetGraphicsCullMode(CullMode::Back);
			RenderSkeletalMeshesTask::Draw(cmd, m_SkeletalMeshesDepthPipeline, singleSidedMeshes, buffers, stats);
			cmd->Barrier(m_OITBuffer);
		}
		if (!doubleSidedMeshes.empty())
		{
			cmd->SetGraphicsCullMode(CullMode::None);
			RenderSkeletalMeshesTask::Draw(cmd, m_SkeletalMeshesDepthPipeline, doubleSidedMeshes, buffers, stats);
			cmd->Barrier(m_OITBuffer);
		}
	}

	void TransparencyTask::RenderSpritesDepth(const Ref<CommandBuffer>& cmd)
	{
		const auto& singleSided = m_Renderer.GetSingleSidedSpritesRenderData();
		const auto& doubleSided = m_Renderer.GetDoubleSidedSpritesRenderData();

		if (singleSided.Translucent.IsEmpty() && doubleSided.Translucent.IsEmpty())
			return;

		EG_GPU_TIMING_SCOPED(cmd, "Transparency. Sprites. Depth");
		EG_CPU_TIMING_SCOPED("Transparency. Sprites. Depth");

		const auto& transformsBuffer = m_Renderer.GetSpritesTransformsBuffer();

		m_SpritesDepthPipeline->SetBuffer(transformsBuffer, 0, 0);
		m_SpritesDepthPipeline->SetBuffer(m_Renderer.GetCameraMatricesBuffer(), 0, 1);
		m_SpritesDepthPipeline->SetBuffer(m_OITBuffer, 5, 0);
		m_SpritesDepthPipeline->SetBuffer(m_UniformBuffer, 5, 1);

		auto& stats = m_Renderer.GetStats();
		if (!singleSided.Translucent.IsEmpty())
		{
			cmd->SetGraphicsCullMode(CullMode::Back);
			RenderSpritesTask::Draw(cmd, m_SpritesDepthPipeline, singleSided.Translucent, nullptr, stats);
			cmd->Barrier(m_OITBuffer);
		}
		if (!doubleSided.Translucent.IsEmpty())
		{
			cmd->SetGraphicsCullMode(CullMode::None);
			RenderSpritesTask::Draw(cmd, m_SpritesDepthPipeline, doubleSided.Translucent, nullptr, stats);
			cmd->Barrier(m_OITBuffer);
		}
	}

	void TransparencyTask::RenderTextsDepth(const Ref<CommandBuffer>& cmd)
	{
		const auto& singleSided = m_Renderer.GetSingleSidedTextsRenderData();
		const auto& doubleSided = m_Renderer.GetDoubleSidedTextsRenderData();

		if (singleSided.Translucent.IsEmpty() && doubleSided.Translucent.IsEmpty())
			return;

		EG_GPU_TIMING_SCOPED(cmd, "Transparency. Texts. Depth");
		EG_CPU_TIMING_SCOPED("Transparency. Texts. Depth");

		m_TextDepthPipeline->SetBuffer(m_Renderer.GetTextsTransformsBuffer(), 0, 0);
		m_TextDepthPipeline->SetBuffer(m_OITBuffer, 0, 1);
		m_TextDepthPipeline->SetBuffer(m_UniformBuffer, 0, 2);
		m_TextDepthPipeline->SetTextureArray(m_Renderer.GetAtlases(), 1, 0);

		const auto& viewProj = m_Renderer.GetViewProjection();
		auto& stats = m_Renderer.GetStats();

		cmd->SetGraphicsCullMode(CullMode::Back);
		if (!singleSided.Translucent.IsEmpty())
		{
			RenderTextLitTask::Draw(cmd, m_TextDepthPipeline, singleSided.Translucent, glm::value_ptr(viewProj), stats);
			cmd->Barrier(m_OITBuffer);
		}

		cmd->SetGraphicsCullMode(CullMode::None);
		if (!doubleSided.Translucent.IsEmpty())
		{
			RenderTextLitTask::Draw(cmd, m_TextDepthPipeline, doubleSided.Translucent, glm::value_ptr(viewProj), stats);
			cmd->Barrier(m_OITBuffer);
		}
	}
	
	void TransparencyTask::RenderMeshesColor(const Ref<CommandBuffer>& cmd)
	{
		const auto& drawData = m_Renderer.GetStaticMeshesDrawData();
		const auto& singleSidedMeshes = drawData.SingleSided.Translucent.DrawData;
		const auto& doubleSidedMeshes = drawData.DoubleSided.Translucent.DrawData;
		if (singleSidedMeshes.empty() && doubleSidedMeshes.empty())
			return;

		EG_GPU_TIMING_SCOPED(cmd, "Transparency. Static Meshes. Color");
		EG_CPU_TIMING_SCOPED("Transparency. Static Meshes. Color");

		const auto& buffers = m_Renderer.GetStaticMeshesBuffers();
		const auto& transformsBuffer = m_Renderer.GetMeshTransformsBuffer();
		const auto& materials = MaterialSystem::GetMaterialsBuffer();

		m_MeshesColorPipeline->SetBuffer(materials, EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
		m_MeshesColorPipeline->SetBuffer(MaterialSystem::GetMaterialsRawBuffer(), EG_PERSISTENT_SET, EG_BINDING_RAW_MATERIALS);
		m_MeshesColorPipeline->SetBuffer(transformsBuffer, EG_PERSISTENT_SET, EG_BINDING_MAX);
		m_MeshesColorPipeline->SetBuffer(m_OITBuffer, 5, 0);
		m_MeshesColorPipeline->SetBuffer(m_Renderer.GetCameraMatricesBuffer(), 5, 1);
		m_MeshesColorPipeline->SetBuffer(m_UniformBuffer, 5, 2);
		if (bFog)
			m_MeshesColorPipeline->SetBuffer(m_Renderer.GetFogDataBuffer(), 5, 3);
		
		const auto& iblAsset = m_Renderer.GetSkybox();
		const bool bHasIrradiance = m_Renderer.IsSkyboxEnabled() && iblAsset.operator bool() && iblAsset->GetTexture()->IsLoaded();
		const auto& ibl = bHasIrradiance ? iblAsset->GetTexture() : RenderManager::GetDummyIBL();
		
		const Ref<Image>& smDistribution = bSoftShadows ? m_Renderer.GetSMDistribution() : RenderManager::GetDummyImage3D();
		
		m_MeshesColorPipeline->SetBuffer(m_Renderer.GetPointLightsBuffer(), EG_SCENE_SET, EG_BINDING_POINT_LIGHTS);
		m_MeshesColorPipeline->SetBuffer(m_Renderer.GetSpotLightsBuffer(), EG_SCENE_SET, EG_BINDING_SPOT_LIGHTS);
		m_MeshesColorPipeline->SetBuffer(m_Renderer.GetDirectionalLightBuffer(), EG_SCENE_SET, EG_BINDING_DIRECTIONAL_LIGHT);
		m_MeshesColorPipeline->SetImageSampler(ibl->GetIrradianceImage(), Sampler::PointSampler, EG_SCENE_SET, EG_BINDING_DIRECTIONAL_LIGHT + 1);
		m_MeshesColorPipeline->SetImageSampler(ibl->GetPrefilterImage(), ibl->GetPrefilterImageSampler(), EG_SCENE_SET, EG_BINDING_DIRECTIONAL_LIGHT + 2);
		m_MeshesColorPipeline->SetImageSampler(RenderManager::GetBRDFLUTImage(), Sampler::PointSampler, EG_SCENE_SET, EG_BINDING_DIRECTIONAL_LIGHT + 3);
		m_MeshesColorPipeline->SetImageSampler(smDistribution, Sampler::PointSampler, EG_SCENE_SET, EG_BINDING_DIRECTIONAL_LIGHT + 4);
		
		m_MeshesColorPipeline->SetImageSamplerArray(m_Renderer.GetDirectionalLightShadowMaps(), m_Renderer.GetDirectionalLightShadowMapsSamplers(), EG_SCENE_SET, EG_BINDING_DIRECTIONAL_LIGHT + 5);
		m_MeshesColorPipeline->SetImageSamplerArray(m_Renderer.GetPointLightShadowMaps(), m_Renderer.GetPointLightShadowMapsSamplers(), 3, 0);
		m_MeshesColorPipeline->SetImageSamplerArray(m_Renderer.GetSpotLightShadowMaps(), m_Renderer.GetSpotLightShadowMapsSamplers(), 4, 0);

		auto& stats = m_Renderer.GetStats();
		if (!singleSidedMeshes.empty())
		{
			cmd->SetGraphicsCullMode(CullMode::Back);
			RenderMeshesTask::Draw(cmd, m_MeshesColorPipeline, singleSidedMeshes, buffers, stats);
			cmd->Barrier(m_OITBuffer);
		}
		if (!doubleSidedMeshes.empty())
		{
			cmd->SetGraphicsCullMode(CullMode::None);
			RenderMeshesTask::Draw(cmd, m_MeshesColorPipeline, doubleSidedMeshes, buffers, stats);
			cmd->Barrier(m_OITBuffer);
		}
	}
	
	void TransparencyTask::RenderSkeletalMeshesColor(const Ref<CommandBuffer>& cmd)
	{
		const auto& singleSidedMeshes = m_Renderer.GetSkeletalMeshesDrawData().SingleSided.Translucent.DrawData;
		const auto& doubleSidedMeshes = m_Renderer.GetSkeletalMeshesDrawData().DoubleSided.Translucent.DrawData;
		if (singleSidedMeshes.empty() && doubleSidedMeshes.empty())
			return;

		EG_GPU_TIMING_SCOPED(cmd, "Transparency. Skeletal Meshes. Color");
		EG_CPU_TIMING_SCOPED("Transparency. Skeletal Meshes. Color");

		const auto& buffers = m_Renderer.GetSkeletalMeshesBuffers();
		const auto& materials = MaterialSystem::GetMaterialsBuffer();
		const auto& vb = m_Renderer.GetSkinnedVertices();

		m_SkeletalMeshesColorPipeline->SetBuffer(materials, EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
		m_SkeletalMeshesColorPipeline->SetBuffer(MaterialSystem::GetMaterialsRawBuffer(), EG_PERSISTENT_SET, EG_BINDING_RAW_MATERIALS);
		m_SkeletalMeshesColorPipeline->SetBuffer(vb, EG_PERSISTENT_SET, EG_BINDING_MAX);
		m_SkeletalMeshesColorPipeline->SetBuffer(buffers.InstanceBuffer, EG_PERSISTENT_SET, EG_BINDING_MAX + 1);
		m_SkeletalMeshesColorPipeline->SetBuffer(m_Renderer.GetSkeletalMeshTransformsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MAX + 2);
		
		const auto& iblAsset = m_Renderer.GetSkybox();
		const bool bHasIrradiance = m_Renderer.IsSkyboxEnabled() && iblAsset.operator bool() && iblAsset->GetTexture()->IsLoaded();
		const auto& ibl = bHasIrradiance ? iblAsset->GetTexture() : RenderManager::GetDummyIBL();
		const Ref<Image>& smDistribution = bSoftShadows ? m_Renderer.GetSMDistribution() : RenderManager::GetDummyImage3D();
		
		m_SkeletalMeshesColorPipeline->SetBuffer(m_Renderer.GetPointLightsBuffer(), EG_SCENE_SET, EG_BINDING_POINT_LIGHTS);
		m_SkeletalMeshesColorPipeline->SetBuffer(m_Renderer.GetSpotLightsBuffer(), EG_SCENE_SET, EG_BINDING_SPOT_LIGHTS);
		m_SkeletalMeshesColorPipeline->SetBuffer(m_Renderer.GetDirectionalLightBuffer(), EG_SCENE_SET, EG_BINDING_DIRECTIONAL_LIGHT);
		m_SkeletalMeshesColorPipeline->SetImageSampler(ibl->GetIrradianceImage(), Sampler::PointSampler, EG_SCENE_SET, EG_BINDING_DIRECTIONAL_LIGHT + 1);
		m_SkeletalMeshesColorPipeline->SetImageSampler(ibl->GetPrefilterImage(), ibl->GetPrefilterImageSampler(), EG_SCENE_SET, EG_BINDING_DIRECTIONAL_LIGHT + 2);
		m_SkeletalMeshesColorPipeline->SetImageSampler(RenderManager::GetBRDFLUTImage(), Sampler::PointSampler, EG_SCENE_SET, EG_BINDING_DIRECTIONAL_LIGHT + 3);
		m_SkeletalMeshesColorPipeline->SetImageSampler(smDistribution, Sampler::PointSampler, EG_SCENE_SET, EG_BINDING_DIRECTIONAL_LIGHT + 4);
		
		m_SkeletalMeshesColorPipeline->SetImageSamplerArray(m_Renderer.GetDirectionalLightShadowMaps(), m_Renderer.GetDirectionalLightShadowMapsSamplers(), EG_SCENE_SET, EG_BINDING_DIRECTIONAL_LIGHT + 5);
		m_SkeletalMeshesColorPipeline->SetImageSamplerArray(m_Renderer.GetPointLightShadowMaps(), m_Renderer.GetPointLightShadowMapsSamplers(), 3, 0);
		m_SkeletalMeshesColorPipeline->SetImageSamplerArray(m_Renderer.GetSpotLightShadowMaps(), m_Renderer.GetSpotLightShadowMapsSamplers(), 4, 0);

		m_SkeletalMeshesColorPipeline->SetBuffer(m_OITBuffer, 5, 0);
		m_SkeletalMeshesColorPipeline->SetBuffer(m_Renderer.GetCameraMatricesBuffer(), 5, 1);
		m_SkeletalMeshesColorPipeline->SetBuffer(m_UniformBuffer, 5, 2);
		if (bFog)
			m_SkeletalMeshesColorPipeline->SetBuffer(m_Renderer.GetFogDataBuffer(), 5, 3);

		auto& stats = m_Renderer.GetStats();
		if (!singleSidedMeshes.empty())
		{
			cmd->SetGraphicsCullMode(CullMode::Back);
			RenderSkeletalMeshesTask::Draw(cmd, m_SkeletalMeshesColorPipeline, singleSidedMeshes, buffers, stats);
			cmd->Barrier(m_OITBuffer);
		}
		if (!doubleSidedMeshes.empty())
		{
			cmd->SetGraphicsCullMode(CullMode::None);
			RenderSkeletalMeshesTask::Draw(cmd, m_SkeletalMeshesColorPipeline, doubleSidedMeshes, buffers, stats);
			cmd->Barrier(m_OITBuffer);
		}
	}

	void TransparencyTask::RenderSpritesColor(const Ref<CommandBuffer>& cmd)
	{
		const auto& singleSided = m_Renderer.GetSingleSidedSpritesRenderData();
		const auto& doubleSided = m_Renderer.GetDoubleSidedSpritesRenderData();

		if (singleSided.Translucent.IsEmpty() && doubleSided.Translucent.IsEmpty())
			return;

		EG_GPU_TIMING_SCOPED(cmd, "Transparency. Sprites. Color");
		EG_CPU_TIMING_SCOPED("Transparency. Sprites. Color");

		const auto& transformsBuffer = m_Renderer.GetSpritesTransformsBuffer();

		const auto& materials = MaterialSystem::GetMaterialsBuffer();
		m_SpritesColorPipeline->SetBuffer(materials, EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
		m_SpritesColorPipeline->SetBuffer(MaterialSystem::GetMaterialsRawBuffer(), EG_PERSISTENT_SET, EG_BINDING_RAW_MATERIALS);
		m_SpritesColorPipeline->SetBuffer(transformsBuffer, EG_PERSISTENT_SET, EG_BINDING_MAX);
		m_SpritesColorPipeline->SetBuffer(m_OITBuffer, 5, 0);
		m_SpritesColorPipeline->SetBuffer(m_Renderer.GetCameraMatricesBuffer(), 5, 1);
		m_SpritesColorPipeline->SetBuffer(m_UniformBuffer, 5, 2);
		if (bFog)
			m_SpritesColorPipeline->SetBuffer(m_Renderer.GetFogDataBuffer(), 5, 3);

		const auto& iblAsset = m_Renderer.GetSkybox();
		const bool bHasIrradiance = m_Renderer.IsSkyboxEnabled() && iblAsset.operator bool() && iblAsset->GetTexture()->IsLoaded();
		const auto& ibl = bHasIrradiance ? iblAsset->GetTexture() : RenderManager::GetDummyIBL();
		
		const Ref<Image>& smDistribution = m_Renderer.GetSMDistribution();
		const Ref<Image>& smDistributionToUse = smDistribution.operator bool() ? smDistribution : RenderManager::GetDummyImage3D();
		
		m_SpritesColorPipeline->SetBuffer(m_Renderer.GetPointLightsBuffer(), EG_SCENE_SET, EG_BINDING_POINT_LIGHTS);
		m_SpritesColorPipeline->SetBuffer(m_Renderer.GetSpotLightsBuffer(), EG_SCENE_SET, EG_BINDING_SPOT_LIGHTS);
		m_SpritesColorPipeline->SetBuffer(m_Renderer.GetDirectionalLightBuffer(), EG_SCENE_SET, EG_BINDING_DIRECTIONAL_LIGHT);
		m_SpritesColorPipeline->SetImageSampler(ibl->GetIrradianceImage(), Sampler::PointSampler, EG_SCENE_SET, EG_BINDING_DIRECTIONAL_LIGHT + 1);
		m_SpritesColorPipeline->SetImageSampler(ibl->GetPrefilterImage(), ibl->GetPrefilterImageSampler(), EG_SCENE_SET, EG_BINDING_DIRECTIONAL_LIGHT + 2);
		m_SpritesColorPipeline->SetImageSampler(RenderManager::GetBRDFLUTImage(), Sampler::PointSampler, EG_SCENE_SET, EG_BINDING_DIRECTIONAL_LIGHT + 3);
		m_SpritesColorPipeline->SetImageSampler(smDistributionToUse, Sampler::PointSampler, EG_SCENE_SET, EG_BINDING_DIRECTIONAL_LIGHT + 4);
		
		m_SpritesColorPipeline->SetImageSamplerArray(m_Renderer.GetDirectionalLightShadowMaps(), m_Renderer.GetDirectionalLightShadowMapsSamplers(), EG_SCENE_SET, EG_BINDING_DIRECTIONAL_LIGHT + 5);
		m_SpritesColorPipeline->SetImageSamplerArray(m_Renderer.GetPointLightShadowMaps(), m_Renderer.GetPointLightShadowMapsSamplers(), 3, 0);
		m_SpritesColorPipeline->SetImageSamplerArray(m_Renderer.GetSpotLightShadowMaps(), m_Renderer.GetSpotLightShadowMapsSamplers(), 4, 0);

		auto& stats = m_Renderer.GetStats();
		if (!singleSided.Translucent.IsEmpty())
		{
			cmd->SetGraphicsCullMode(CullMode::Back);
			RenderSpritesTask::Draw(cmd, m_SpritesColorPipeline, singleSided.Translucent, nullptr, stats);
			cmd->Barrier(m_OITBuffer);
		}
		if (!doubleSided.Translucent.IsEmpty())
		{
			cmd->SetGraphicsCullMode(CullMode::None);
			RenderSpritesTask::Draw(cmd, m_SpritesColorPipeline, doubleSided.Translucent, nullptr, stats);
			cmd->Barrier(m_OITBuffer);
		}
	}

	void TransparencyTask::RenderTextsColor(const Ref<CommandBuffer>& cmd)
	{
		const auto& singleSided = m_Renderer.GetSingleSidedTextsRenderData();
		const auto& doubleSided = m_Renderer.GetDoubleSidedTextsRenderData();

		if (singleSided.Translucent.IsEmpty() && doubleSided.Translucent.IsEmpty())
			return;

		EG_GPU_TIMING_SCOPED(cmd, "Transparency. Texts. Color");
		EG_CPU_TIMING_SCOPED("Transparency. Texts. Color");

		const auto& materials = MaterialSystem::GetMaterialsBuffer();
		m_TextColorPipeline->SetBuffer(materials, EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
		m_TextColorPipeline->SetBuffer(MaterialSystem::GetMaterialsRawBuffer(), EG_PERSISTENT_SET, EG_BINDING_RAW_MATERIALS);
		m_TextColorPipeline->SetBuffer(m_Renderer.GetTextsTransformsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MAX);
		m_TextColorPipeline->SetBuffer(m_OITBuffer, EG_PERSISTENT_SET, EG_BINDING_MAX + 1);
		m_TextColorPipeline->SetBuffer(m_Renderer.GetCameraMatricesBuffer(), EG_PERSISTENT_SET, EG_BINDING_MAX + 2);
		if (bFog)
			m_TextColorPipeline->SetBuffer(m_Renderer.GetFogDataBuffer(), EG_PERSISTENT_SET, EG_BINDING_MAX + 3);

		m_TextColorPipeline->SetTextureArray(m_Renderer.GetAtlases(), 5, 0);
		m_TextColorPipeline->SetBuffer(m_UniformBuffer, 6, 0);

		const auto& iblAsset = m_Renderer.GetSkybox();
		const bool bHasIrradiance = m_Renderer.IsSkyboxEnabled() && iblAsset.operator bool() && iblAsset->GetTexture()->IsLoaded();
		const auto& ibl = bHasIrradiance ? iblAsset->GetTexture() : RenderManager::GetDummyIBL();

		const Ref<Image>& smDistribution = m_Renderer.GetSMDistribution();
		const Ref<Image>& smDistributionToUse = smDistribution.operator bool() ? smDistribution : RenderManager::GetDummyImage3D();

		m_TextColorPipeline->SetBuffer(m_Renderer.GetPointLightsBuffer(), EG_SCENE_SET, EG_BINDING_POINT_LIGHTS);
		m_TextColorPipeline->SetBuffer(m_Renderer.GetSpotLightsBuffer(), EG_SCENE_SET, EG_BINDING_SPOT_LIGHTS);
		m_TextColorPipeline->SetBuffer(m_Renderer.GetDirectionalLightBuffer(), EG_SCENE_SET, EG_BINDING_DIRECTIONAL_LIGHT);
		m_TextColorPipeline->SetImageSampler(ibl->GetIrradianceImage(), Sampler::PointSampler, EG_SCENE_SET, EG_BINDING_DIRECTIONAL_LIGHT + 1);
		m_TextColorPipeline->SetImageSampler(ibl->GetPrefilterImage(), ibl->GetPrefilterImageSampler(), EG_SCENE_SET, EG_BINDING_DIRECTIONAL_LIGHT + 2);
		m_TextColorPipeline->SetImageSampler(RenderManager::GetBRDFLUTImage(), Sampler::PointSampler, EG_SCENE_SET, EG_BINDING_DIRECTIONAL_LIGHT + 3);
		m_TextColorPipeline->SetImageSampler(smDistributionToUse, Sampler::PointSampler, EG_SCENE_SET, EG_BINDING_DIRECTIONAL_LIGHT + 4);

		m_TextColorPipeline->SetImageSamplerArray(m_Renderer.GetDirectionalLightShadowMaps(), m_Renderer.GetDirectionalLightShadowMapsSamplers(), EG_SCENE_SET, EG_BINDING_DIRECTIONAL_LIGHT + 5);
		m_TextColorPipeline->SetImageSamplerArray(m_Renderer.GetPointLightShadowMaps(), m_Renderer.GetPointLightShadowMapsSamplers(), 3, 0);
		m_TextColorPipeline->SetImageSamplerArray(m_Renderer.GetSpotLightShadowMaps(), m_Renderer.GetSpotLightShadowMapsSamplers(), 4, 0);

		const auto& viewProj = m_Renderer.GetViewProjection();
		auto& stats = m_Renderer.GetStats();

		cmd->SetGraphicsCullMode(CullMode::Back);
		if (!singleSided.Translucent.IsEmpty())
		{
			RenderTextLitTask::Draw(cmd, m_TextColorPipeline, singleSided.Translucent, glm::value_ptr(viewProj), stats);
			cmd->Barrier(m_OITBuffer);
		}

		cmd->SetGraphicsCullMode(CullMode::None);
		if (!doubleSided.Translucent.IsEmpty())
		{
			RenderTextLitTask::Draw(cmd, m_TextColorPipeline, doubleSided.Translucent, glm::value_ptr(viewProj), stats);
			cmd->Barrier(m_OITBuffer);
		}
	}

	void TransparencyTask::CompositePass(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "Transparency. Composite");
		EG_CPU_TIMING_SCOPED("Transparency. Composite");

		m_CompositePipeline->SetBuffer(m_OITBuffer, 0, 0);

		const glm::uvec2 viewportSize = m_Renderer.GetViewportSize();

		cmd->BeginGraphics(m_CompositePipeline);
		cmd->SetGraphicsRootConstants(nullptr, &viewportSize);
		cmd->Draw(3, 0);
		cmd->EndGraphics();
	}

	void TransparencyTask::RenderEntityIDs(const Ref<CommandBuffer>& cmd)
	{
		const bool bRender = bObjectPickingEnabled || !m_Renderer.IsRuntime();
		if (!bRender)
			return;

		const glm::mat4& viewProj = m_Renderer.GetViewProjection();

		// Meshes
		{
			const auto& drawData = m_Renderer.GetStaticMeshesDrawData();
			const auto& singleSidedMeshes = drawData.SingleSided.Translucent.DrawData;
			const auto& doubleSidedMeshes = drawData.DoubleSided.Translucent.DrawData;
			const bool bNoMeshes = singleSidedMeshes.empty() && doubleSidedMeshes.empty();
			if (!bNoMeshes)
			{
				EG_GPU_TIMING_SCOPED(cmd, "Transparency. Static Meshes Entity IDs");
				EG_CPU_TIMING_SCOPED("Transparency. Static Meshes Entity IDs");

				const auto& transformsBuffer = m_Renderer.GetMeshTransformsBuffer();
				m_MeshesEntityIDPipeline->SetBuffer(transformsBuffer, 0, 0);

				const auto& buffers = m_Renderer.GetStaticMeshesBuffers();
				auto& stats = m_Renderer.GetStats();

				if (!singleSidedMeshes.empty())
				{
					cmd->SetGraphicsCullMode(CullMode::Back);
					RenderMeshesTask::Draw(cmd, m_MeshesEntityIDPipeline, singleSidedMeshes, buffers, stats, glm::value_ptr(viewProj));
				}
				if (!doubleSidedMeshes.empty())
				{
					cmd->SetGraphicsCullMode(CullMode::None);
					RenderMeshesTask::Draw(cmd, m_MeshesEntityIDPipeline, doubleSidedMeshes, buffers, stats, glm::value_ptr(viewProj));
				}
			}
		}

		// Skeletal Meshes
		{
			const auto& singleSidedMeshes = m_Renderer.GetSkeletalMeshesDrawData().SingleSided.Translucent.DrawData;
			const auto& doubleSidedMeshes = m_Renderer.GetSkeletalMeshesDrawData().DoubleSided.Translucent.DrawData;
			const bool bNoMeshes = singleSidedMeshes.empty() && doubleSidedMeshes.empty();
			if (!bNoMeshes)
			{
				EG_GPU_TIMING_SCOPED(cmd, "Transparency. Skeletal Meshes Entity IDs");
				EG_CPU_TIMING_SCOPED("Transparency. Skeletal Meshes Entity IDs");

				const auto& buffers = m_Renderer.GetSkeletalMeshesBuffers();
				const auto& vb = m_Renderer.GetSkinnedVertices();

				m_SkeletalMeshesEntityIDPipeline->SetBuffer(vb, 0, 0);
				m_SkeletalMeshesEntityIDPipeline->SetBuffer(buffers.InstanceBuffer, 0, 1);
				m_SkeletalMeshesEntityIDPipeline->SetBuffer(m_Renderer.GetCameraMatricesBuffer(), 0, 2);

				auto& stats = m_Renderer.GetStats();
				if (!singleSidedMeshes.empty())
				{
					cmd->SetGraphicsCullMode(CullMode::Back);
					RenderSkeletalMeshesTask::Draw(cmd, m_SkeletalMeshesEntityIDPipeline, singleSidedMeshes, buffers, stats);
				}
				if (!doubleSidedMeshes.empty())
				{
					cmd->SetGraphicsCullMode(CullMode::None);
					RenderSkeletalMeshesTask::Draw(cmd, m_SkeletalMeshesEntityIDPipeline, doubleSidedMeshes, buffers, stats);
				}
			}
		}

		// Sprites
		{
			const auto& singleSided = m_Renderer.GetSingleSidedSpritesRenderData();
			const auto& doubleSided = m_Renderer.GetDoubleSidedSpritesRenderData();

			const bool bNoSprites = singleSided.Translucent.IsEmpty() && doubleSided.Translucent.IsEmpty();
			if (!bNoSprites)
			{
				EG_GPU_TIMING_SCOPED(cmd, "Transparency. Sprites Entity IDs");
				EG_CPU_TIMING_SCOPED("Transparency. Sprites Entity IDs");

				const auto& transformsBuffer = m_Renderer.GetSpritesTransformsBuffer();
				m_SpritesEntityIDPipeline->SetBuffer(transformsBuffer, 0, 0);

				auto& stats = m_Renderer.GetStats();
				if (!singleSided.Translucent.IsEmpty())
				{
					cmd->SetGraphicsCullMode(CullMode::Back);
					RenderSpritesTask::Draw(cmd, m_SpritesEntityIDPipeline, singleSided.Translucent, nullptr, stats);
				}
				if (!doubleSided.Translucent.IsEmpty())
				{
					cmd->SetGraphicsCullMode(CullMode::None);
					RenderSpritesTask::Draw(cmd, m_SpritesEntityIDPipeline, doubleSided.Translucent, nullptr, stats);
				}
			}
		}
		
		// Texts
		{
			const auto& singleSided = m_Renderer.GetSingleSidedTextsRenderData();
			const auto& doubleSided = m_Renderer.GetDoubleSidedTextsRenderData();

			const bool bNoTexts = singleSided.Translucent.IsEmpty() && doubleSided.Translucent.IsEmpty();
			if (!bNoTexts)
			{
				EG_GPU_TIMING_SCOPED(cmd, "Transparency. Texts Entity IDs");
				EG_CPU_TIMING_SCOPED("Transparency. Texts Entity IDs");

				m_TextEntityIDPipeline->SetBuffer(m_Renderer.GetTextsTransformsBuffer(), 0, 0);
				m_TextEntityIDPipeline->SetTextureArray(m_Renderer.GetAtlases(), 1, 0);

				const auto& viewProj = m_Renderer.GetViewProjection();
				auto& stats = m_Renderer.GetStats();

				cmd->SetGraphicsCullMode(CullMode::Back);
				if (!singleSided.Translucent.IsEmpty())
				{
					RenderTextLitTask::Draw(cmd, m_TextEntityIDPipeline, singleSided.Translucent, glm::value_ptr(viewProj), stats);
				}
				cmd->SetGraphicsCullMode(CullMode::None);
				if (!doubleSided.Translucent.IsEmpty())
				{
					RenderTextLitTask::Draw(cmd, m_TextEntityIDPipeline, doubleSided.Translucent, glm::value_ptr(viewProj), stats);
				}
			}
		}
	}

	void TransparencyTask::Prepare(const Ref<CommandBuffer>& cmd)
	{
		const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
		const bool bTexturesDirty = texturesChangedFrame >= m_TexturesUpdatedFrames[RenderManager::GetCurrentFrameIndex()];
		if (bTexturesDirty)
		{
			m_TextColorPipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
			m_SpritesColorPipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
			m_MeshesColorPipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
			m_SkeletalMeshesColorPipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
			m_TexturesUpdatedFrames[RenderManager::GetCurrentFrameIndex()] = texturesChangedFrame + 1;
		}

		const auto& iblAsset = m_Renderer.GetSkybox();
		const bool bHasIrradiance = m_Renderer.IsSkyboxEnabled() && iblAsset.operator bool();
		const auto& ibl = bHasIrradiance ? iblAsset->GetTexture() : RenderManager::GetDummyIBL();

		UniformData uniforms{};
		uniforms.Size = m_Renderer.GetViewportSize();
		uniforms.CameraPos = m_Renderer.GetViewPosition();
		uniforms.MaxReflectionLOD = float(ibl->GetPrefilterImage()->GetMipsCount() - 1);
		uniforms.MaxShadowDistance2 = m_Renderer.GetShadowMaxDistance() * m_Renderer.GetShadowMaxDistance();
		uniforms.CascadesSmoothTransitionAlpha = m_Renderer.GetOptions_RT().InternalState.CascadesSmoothTransitionAlpha;
		uniforms.IBLIntensity = m_Renderer.GetSkyboxIntensity();
		uniforms.PointLights = (uint32_t)m_Renderer.GetPointLights().size();
		uniforms.SpotLights = (uint32_t)m_Renderer.GetSpotLights().size();
		uniforms.HasDirLight = uint32_t(m_Renderer.HasDirectionalLight());
		cmd->Write(m_UniformBuffer, &uniforms, sizeof(UniformData), 0, m_UniformBuffer->GetLayout(), BufferReadAccess::Uniform);

		PBRConstantsKernelInfo info;
		info.PointLightsCount = uniforms.PointLights;
		info.SpotLightsCount = uniforms.SpotLights;
		info.bHasDirLight = uniforms.HasDirLight;
		info.bHasIrradiance = bHasIrradiance;
		if (info != m_KernelInfo)
		{
			// If stutterless, reload only if `bHasIrradiance` differs
			const bool bRecreate = !bStutterlessShaders || (m_KernelInfo.bHasIrradiance != info.bHasIrradiance);
			m_KernelInfo = info;
			if (bRecreate)
				RecreatePipeline(false);
		}
	}

	void TransparencyTask::InitMeshPipelines()
	{
		const auto& gbuffer = m_Renderer.GetGBuffer();

		ColorAttachment attachment;
		attachment.Image = m_Renderer.GetHDROutput();
		attachment.InitialLayout = ImageLayoutType::RenderTarget;
		attachment.FinalLayout = ImageLayoutType::RenderTarget;
		attachment.ClearOperation = ClearOperation::Load;

		attachment.bBlendEnabled = true;
		attachment.BlendingState.BlendOp = BlendOperation::Add;
		attachment.BlendingState.BlendSrc = BlendFactor::One;
		attachment.BlendingState.BlendDst = BlendFactor::OneMinusSrcAlpha;

		attachment.BlendingState.BlendOpAlpha = BlendOperation::Add;
		attachment.BlendingState.BlendSrcAlpha = BlendFactor::One;
		attachment.BlendingState.BlendDstAlpha = BlendFactor::OneMinusSrcAlpha;

		DepthStencilAttachment depthAttachment;
		depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.Image = gbuffer.Depth;
		depthAttachment.bWriteDepth = false;
		depthAttachment.DepthCompareOp = CompareOperation::Greater;
		depthAttachment.ClearOperation = ClearOperation::Load;

		PipelineGraphicsState state;
		state.VertexShader = Shader::Create("transparency/mesh_transparency_color.vert", ShaderType::Vertex);
		state.FragmentShader = m_TransparencyColorShader;
		state.ColorAttachments.push_back(attachment);
		state.DepthStencilAttachment = depthAttachment;
		state.CullMode = CullMode::Dynamic;
		state.PerInstanceAttribs = RenderMeshesTask::PerInstanceAttribs;

		ShaderSpecializationInfo constants;
		if (!bStutterlessShaders)
		{
			constants.MapEntries.push_back({ 0, 0, sizeof(uint32_t) });
			constants.MapEntries.push_back({ 1, 4, sizeof(uint32_t) });
			constants.MapEntries.push_back({ 2, 8, sizeof(uint32_t) });
		}
		constants.MapEntries.push_back({ 3, 12, sizeof(uint32_t) });
		constants.Data = &m_KernelInfo;
		constants.Size = sizeof(PBRConstantsKernelInfo);
		
		state.FragmentSpecializationInfo = constants;
		m_MeshesColorPipeline = PipelineGraphics::Create(state);

		state.FragmentSpecializationInfo = ShaderSpecializationInfo{};
		state.VertexShader = Shader::Create("transparency/mesh_transparency_depth.vert", ShaderType::Vertex);
		state.FragmentShader = m_TransparencyDepthShader;
		m_MeshesDepthPipeline = PipelineGraphics::Create(state);
	}

	void TransparencyTask::InitSkeletalMeshPipelines()
	{
		const auto& gbuffer = m_Renderer.GetGBuffer();

		ColorAttachment attachment;
		attachment.Image = m_Renderer.GetHDROutput();
		attachment.InitialLayout = ImageLayoutType::RenderTarget;
		attachment.FinalLayout = ImageLayoutType::RenderTarget;
		attachment.ClearOperation = ClearOperation::Load;

		attachment.bBlendEnabled = true;
		attachment.BlendingState.BlendOp = BlendOperation::Add;
		attachment.BlendingState.BlendSrc = BlendFactor::One;
		attachment.BlendingState.BlendDst = BlendFactor::OneMinusSrcAlpha;

		attachment.BlendingState.BlendOpAlpha = BlendOperation::Add;
		attachment.BlendingState.BlendSrcAlpha = BlendFactor::One;
		attachment.BlendingState.BlendDstAlpha = BlendFactor::OneMinusSrcAlpha;

		DepthStencilAttachment depthAttachment;
		depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.Image = gbuffer.Depth;
		depthAttachment.bWriteDepth = false;
		depthAttachment.DepthCompareOp = CompareOperation::Greater;
		depthAttachment.ClearOperation = ClearOperation::Load;

		PipelineGraphicsState state;
		state.VertexShader = Shader::Create("transparency/skeletal_mesh_transparency_color.vert", ShaderType::Vertex);
		state.FragmentShader = m_TransparencyColorShader;
		state.ColorAttachments.push_back(attachment);
		state.DepthStencilAttachment = depthAttachment;
		state.CullMode = CullMode::Dynamic;
		state.PerInstanceAttribs = RenderSkeletalMeshesTask::PerInstanceAttribs;

		ShaderSpecializationInfo constants;
		if (!bStutterlessShaders)
		{
			constants.MapEntries.push_back({ 0, 0, sizeof(uint32_t) });
			constants.MapEntries.push_back({ 1, 4, sizeof(uint32_t) });
			constants.MapEntries.push_back({ 2, 8, sizeof(uint32_t) });
		}
		constants.MapEntries.push_back({ 3, 12, sizeof(uint32_t) });
		constants.Data = &m_KernelInfo;
		constants.Size = sizeof(PBRConstantsKernelInfo);
		
		state.FragmentSpecializationInfo = constants;
		m_SkeletalMeshesColorPipeline = PipelineGraphics::Create(state);

		state.FragmentSpecializationInfo = ShaderSpecializationInfo{};
		state.VertexShader = Shader::Create("transparency/skeletal_mesh_transparency_depth.vert", ShaderType::Vertex);
		state.FragmentShader = m_TransparencyDepthShader;
		m_SkeletalMeshesDepthPipeline = PipelineGraphics::Create(state);
	}

	void TransparencyTask::InitSpritesPipelines()
	{
		const auto& gbuffer = m_Renderer.GetGBuffer();

		ColorAttachment attachment;
		attachment.Image = m_Renderer.GetHDROutput();
		attachment.InitialLayout = ImageLayoutType::RenderTarget;
		attachment.FinalLayout = ImageLayoutType::RenderTarget;
		attachment.ClearOperation = ClearOperation::Load;

		attachment.bBlendEnabled = true;
		attachment.BlendingState.BlendOp = BlendOperation::Add;
		attachment.BlendingState.BlendSrc = BlendFactor::One;
		attachment.BlendingState.BlendDst = BlendFactor::OneMinusSrcAlpha;

		attachment.BlendingState.BlendOpAlpha = BlendOperation::Add;
		attachment.BlendingState.BlendSrcAlpha = BlendFactor::One;
		attachment.BlendingState.BlendDstAlpha = BlendFactor::OneMinusSrcAlpha;

		DepthStencilAttachment depthAttachment;
		depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.Image = gbuffer.Depth;
		depthAttachment.bWriteDepth = false;
		depthAttachment.DepthCompareOp = CompareOperation::Greater;
		depthAttachment.ClearOperation = ClearOperation::Load;

		PipelineGraphicsState state;
		state.VertexShader = Shader::Create("transparency/sprite_transparency_color.vert", ShaderType::Vertex);
		state.FragmentShader = m_TransparencyColorShader;
		state.ColorAttachments.push_back(attachment);
		state.DepthStencilAttachment = depthAttachment;
		state.CullMode = CullMode::Dynamic;

		ShaderSpecializationInfo constants;
		if (!bStutterlessShaders)
		{
			constants.MapEntries.push_back({ 0, 0, sizeof(uint32_t) });
			constants.MapEntries.push_back({ 1, 4, sizeof(uint32_t) });
			constants.MapEntries.push_back({ 2, 8, sizeof(uint32_t) });
		}
		constants.MapEntries.push_back({ 3, 12, sizeof(uint32_t) });
		constants.Data = &m_KernelInfo;
		constants.Size = sizeof(PBRConstantsKernelInfo);

		state.FragmentSpecializationInfo = constants;
		m_SpritesColorPipeline = PipelineGraphics::Create(state);

		state.FragmentSpecializationInfo = ShaderSpecializationInfo{};
		state.VertexShader = Shader::Create("transparency/sprite_transparency_depth.vert", ShaderType::Vertex);
		state.FragmentShader = m_TransparencyDepthShader;
		m_SpritesDepthPipeline = PipelineGraphics::Create(state);
	}

	void TransparencyTask::InitTextsPipelines()
	{
		const auto& gbuffer = m_Renderer.GetGBuffer();

		ColorAttachment attachment;
		attachment.Image = m_Renderer.GetHDROutput();
		attachment.InitialLayout = ImageLayoutType::RenderTarget;
		attachment.FinalLayout = ImageLayoutType::RenderTarget;
		attachment.ClearOperation = ClearOperation::Load;

		attachment.bBlendEnabled = true;
		attachment.BlendingState.BlendOp = BlendOperation::Add;
		attachment.BlendingState.BlendSrc = BlendFactor::One;
		attachment.BlendingState.BlendDst = BlendFactor::OneMinusSrcAlpha;

		attachment.BlendingState.BlendOpAlpha = BlendOperation::Add;
		attachment.BlendingState.BlendSrcAlpha = BlendFactor::One;
		attachment.BlendingState.BlendDstAlpha = BlendFactor::OneMinusSrcAlpha;

		DepthStencilAttachment depthAttachment;
		depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.Image = gbuffer.Depth;
		depthAttachment.bWriteDepth = false;
		depthAttachment.DepthCompareOp = CompareOperation::Greater;
		depthAttachment.ClearOperation = ClearOperation::Load;

		PipelineGraphicsState state;
		state.VertexShader = Shader::Create("transparency/text_transparency_color.vert", ShaderType::Vertex);
		state.FragmentShader = m_TransparencyTextColorShader;
		state.ColorAttachments.push_back(attachment);
		state.DepthStencilAttachment = depthAttachment;
		state.CullMode = CullMode::Dynamic;

		ShaderSpecializationInfo constants;
		if (!bStutterlessShaders)
		{
			constants.MapEntries.push_back({ 0, 0, sizeof(uint32_t) });
			constants.MapEntries.push_back({ 1, 4, sizeof(uint32_t) });
			constants.MapEntries.push_back({ 2, 8, sizeof(uint32_t) });
		}
		constants.MapEntries.push_back({ 3, 12, sizeof(uint32_t) });
		constants.Data = &m_KernelInfo;
		constants.Size = sizeof(PBRConstantsKernelInfo);

		state.FragmentSpecializationInfo = constants;
		m_TextColorPipeline = PipelineGraphics::Create(state);

		state.FragmentSpecializationInfo = ShaderSpecializationInfo{};
		state.VertexShader = Shader::Create("transparency/text_transparency_depth.vert", ShaderType::Vertex);
		state.FragmentShader = m_TransparencyTextDepthShader;
		m_TextDepthPipeline = PipelineGraphics::Create(state);
	}
	
	void TransparencyTask::InitCompositePipelines()
	{
		ColorAttachment colorAttachment;
		colorAttachment.Image = m_Renderer.GetHDROutput();
		colorAttachment.InitialLayout = ImageLayoutType::RenderTarget;
		colorAttachment.FinalLayout = ImageLayoutType::RenderTarget;
		colorAttachment.ClearOperation = ClearOperation::Load;

		colorAttachment.bBlendEnabled = true;
		colorAttachment.BlendingState.BlendOp = BlendOperation::Add;
		colorAttachment.BlendingState.BlendSrc = BlendFactor::One;
		colorAttachment.BlendingState.BlendDst = BlendFactor::OneMinusSrcAlpha;

		colorAttachment.BlendingState.BlendOpAlpha = BlendOperation::Add;
		colorAttachment.BlendingState.BlendSrcAlpha = BlendFactor::One;
		colorAttachment.BlendingState.BlendDstAlpha = BlendFactor::OneMinusSrcAlpha;

		PipelineGraphicsState state;
		state.VertexShader = Shader::Create("quad_tri.vert", ShaderType::Vertex);
		state.FragmentShader = m_TransparencyCompositeShader;
		state.ColorAttachments.push_back(colorAttachment);
		state.CullMode = CullMode::Back;

		m_CompositePipeline = PipelineGraphics::Create(state);
	}
	
	void TransparencyTask::InitEntityIDPipelines()
	{
		ColorAttachment objectIDAttachment;
		objectIDAttachment.Image = m_Renderer.GetGBuffer().ObjectID;
		objectIDAttachment.InitialLayout = ImageLayoutType::RenderTarget;
		objectIDAttachment.FinalLayout = ImageLayoutType::RenderTarget;
		objectIDAttachment.ClearOperation = ClearOperation::Load;

		DepthStencilAttachment depthAttachment;
		depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.Image = m_Renderer.GetGBuffer().Depth;
		depthAttachment.bWriteDepth = false;
		depthAttachment.DepthCompareOp = CompareOperation::Greater;
		depthAttachment.ClearOperation = ClearOperation::Load;

		PipelineGraphicsState state;
		state.VertexShader = Shader::Create("transparency/sprite_transparency_entityID.vert", ShaderType::Vertex);
		state.FragmentShader = Shader::Create("transparency/transparency_entityID.frag", ShaderType::Fragment);
		state.ColorAttachments.push_back(objectIDAttachment);
		state.DepthStencilAttachment = depthAttachment;
		state.CullMode = CullMode::Dynamic;

		m_SpritesEntityIDPipeline = PipelineGraphics::Create(state);

		state.PerInstanceAttribs = RenderMeshesTask::PerInstanceAttribs;
		state.VertexShader = Shader::Create("transparency/mesh_transparency_entityID.vert", ShaderType::Vertex);
		m_MeshesEntityIDPipeline = PipelineGraphics::Create(state);

		state.PerInstanceAttribs = RenderSkeletalMeshesTask::PerInstanceAttribs;
		state.VertexShader = Shader::Create("transparency/skeletal_mesh_transparency_entityID.vert", ShaderType::Vertex);
		m_SkeletalMeshesEntityIDPipeline = PipelineGraphics::Create(state);

		state.PerInstanceAttribs.clear();
		state.VertexShader = Shader::Create("transparency/text_transparency_entityID.vert", ShaderType::Vertex);
		state.FragmentShader = Shader::Create("transparency/transparency_text_entityID.frag", ShaderType::Fragment);
		m_TextEntityIDPipeline = PipelineGraphics::Create(state);
	}
	
	void TransparencyTask::InitOITBuffer()
	{
		const glm::uvec2 size = m_Renderer.GetViewportSize();
		BufferSpecifications specs;
		specs.Format = ImageFormat::R32_UInt;
		specs.Usage = BufferUsage::StorageTexelBuffer | BufferUsage::TransferDst;
		specs.Size = size_t(size.x * size.y * m_Layers) * s_Stride;
		specs.Layout = BufferLayoutType::StorageBuffer;
		m_OITBuffer = Buffer::Create(specs, "OIT_Buffer");
	}
	
	bool TransparencyTask::SetSoftShadowsEnabled(bool bEnable)
	{
		if (bSoftShadows == bEnable)
			return false;

		bSoftShadows = bEnable;
		auto& defines = m_ShaderDefines;

		bool bUpdate = false;
		if (bEnable)
		{
			defines["EG_SOFT_SHADOWS"] = "";
			bUpdate = true;
		}
		else
		{
			auto it = defines.find("EG_SOFT_SHADOWS");
			if (it != defines.end())
			{
				defines.erase(it);
				bUpdate = true;
			}
		}

		return bUpdate;
	}

	bool TransparencyTask::SetVisualizeCascades(bool bVisualize)
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

	bool TransparencyTask::SetCSMSmoothTransitionEnabled(bool bEnabled)
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

	bool TransparencyTask::SetStutterlessEnabled(bool bEnable)
	{
		if (bStutterlessShaders == bEnable)
			return false;

		bStutterlessShaders = bEnable;

		auto& defines = m_ShaderDefines;
		auto it = defines.find("EG_STUTTERLESS");

		bool bUpdate = false;
		if (bEnable)
		{
			if (it == defines.end())
			{
				defines["EG_STUTTERLESS"] = "";
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

	bool TransparencyTask::SetFogEnabled(bool bEnable)
	{
		if (bFog == bEnable)
			return false;

		bFog = bEnable;

		auto& defines = m_ShaderDefines;
		auto it = defines.find("EG_FOG");

		bool bUpdate = false;
		if (bEnable)
		{
			if (it == defines.end())
			{
				defines["EG_FOG"] = "";
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

	void TransparencyTask::RecreatePipeline(bool bUpdateDefines)
	{
		ShaderSpecializationInfo constants;
		if (!bStutterlessShaders)
		{
			constants.MapEntries.push_back({ 0, 0, sizeof(uint32_t) });
			constants.MapEntries.push_back({ 1, 4, sizeof(uint32_t) });
			constants.MapEntries.push_back({ 2, 8, sizeof(uint32_t) });
		}

		constants.MapEntries.push_back({ 3, 12, sizeof(uint32_t) });
		constants.Data = &m_KernelInfo;
		constants.Size = sizeof(PBRConstantsKernelInfo);

		{
			auto state = m_MeshesColorPipeline->GetState();
			state.FragmentSpecializationInfo = constants;
			m_MeshesColorPipeline->SetState(state);
		}
		{
			auto state = m_SkeletalMeshesColorPipeline->GetState();
			state.FragmentSpecializationInfo = constants;
			m_SkeletalMeshesColorPipeline->SetState(state);
		}
		{
			auto state = m_SpritesColorPipeline->GetState();
			state.FragmentSpecializationInfo = constants;
			m_SpritesColorPipeline->SetState(state);
		}
		{
			auto state = m_TextColorPipeline->GetState();
			state.FragmentSpecializationInfo = constants;
			m_TextColorPipeline->SetState(state);

			if (bUpdateDefines)
				state.FragmentShader->SetDefines(m_ShaderDefines);
		}

		if (bUpdateDefines)
		{
			m_TransparencyColorShader->SetDefines(m_ShaderDefines);
			m_TransparencyTextColorShader->SetDefines(m_ShaderDefines);
		}
	}
}
