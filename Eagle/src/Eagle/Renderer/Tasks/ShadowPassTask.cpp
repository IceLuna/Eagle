#include "egpch.h"
#include "ShadowPassTask.h"

#include "Eagle/Classes/StaticMesh.h"

#include "Eagle/Renderer/RenderManager.h"
#include "Eagle/Renderer/SceneRenderer.h"
#include "Eagle/Renderer/VidWrappers/Image.h"
#include "Eagle/Renderer/VidWrappers/Sampler.h"
#include "Eagle/Renderer/VidWrappers/Framebuffer.h"
#include "Eagle/Renderer/VidWrappers/PipelineGraphics.h"
#include "Eagle/Renderer/VidWrappers/RenderCommandManager.h"
#include "Eagle/Renderer/TextureSystem.h"
#include "Eagle/Renderer/MaterialSystem.h"

#include "RenderMeshesTask.h"
#include "RenderSkeletalMeshesTask.h"
#include "RenderSpritesTask.h"
#include "RenderTextLitTask.h"
#include "RenderTextUnlitTask.h"

#include "Eagle/Debug/CPUTimings.h"
#include "Eagle/Debug/GPUTimings.h"

#include "../../Eagle-Editor/assets/shaders/common_structures.h"

namespace Eagle
{
	glm::uvec2 ShadowPassTask::GetPointLightSMSize(float distanceToCamera, float maxShadowDistance)
	{
		const float k = distanceToCamera / maxShadowDistance;

		uint32_t scaler = 1u;
		for (float f = 0.1f; f < 1.f; f += 0.1f)
		{
			if (k > f)
				scaler *= 2u; // Resolution is getting two times lower each 10% of the distance
			else
				break; // early exit
		}

		constexpr glm::uvec2 minRes = glm::uvec2(64u);

		glm::uvec2 size = glm::uvec2(m_Settings.PointLightShadowMapSize / scaler);
		size = glm::max(minRes, size);

		return size;
	}

	glm::uvec2 ShadowPassTask::GetSpotLightSMSize(float distanceToCamera, float maxShadowDistance)
	{
		const float k = distanceToCamera / maxShadowDistance;

		uint32_t scaler = 1u;
		for (float f = 0.25f; f < 1.f; f += 0.25f)
		{
			if (k > f)
				scaler *= 2u; // Resolution is getting two times lower each 25% of the distance
			else
				break; // early exit
		}

		constexpr glm::uvec2 minRes = glm::uvec2(64u);
		
		glm::uvec2 size = glm::uvec2(m_Settings.SpotLightShadowMapSize / scaler);
		size = glm::max(minRes, size);

		return size;
	}

	static Ref<Image> CreateDepthImage(glm::uvec3 size, const std::string& debugName, bool bCube)
	{
		ImageSpecifications depthSpecs;
		depthSpecs.Format = Application::Get().GetRenderContext()->GetDepthFormat();
		depthSpecs.Usage = ImageUsage::DepthStencilAttachment | ImageUsage::Sampled;
		depthSpecs.bIsCube = bCube;
		depthSpecs.Size = size;
		return Image::Create(depthSpecs, debugName);
	}

	static Ref<Image> CreateColoredFilterImage(glm::uvec3 size, const std::string& debugName, bool bCube)
	{
		ImageSpecifications specs;
		specs.Format = ImageFormat::R8G8B8A8_UNorm;
		specs.Usage = ImageUsage::ColorAttachment | ImageUsage::Sampled;
		specs.bIsCube = bCube;
		specs.Size = size;
		return Image::Create(specs, debugName);
	}

	static Ref<Image> CreateDepthImage16(glm::uvec3 size, const std::string& debugName, bool bCube)
	{
		ImageSpecifications depthSpecs;
		depthSpecs.Format = ImageFormat::R16_Float;
		depthSpecs.Usage = ImageUsage::ColorAttachment | ImageUsage::Sampled;
		depthSpecs.bIsCube = bCube;
		depthSpecs.Size = size;
		return Image::Create(depthSpecs, debugName);
	}

	ShadowPassTask::ShadowPassTask(SceneRenderer& renderer)
		: RendererTask(renderer)
		, m_PLShadowMapSamplers(EG_MAX_LIGHT_SHADOW_MAPS)
		, m_SLShadowMapSamplers(m_PLShadowMapSamplers)
		, m_DLShadowMaps(EG_CASCADES_COUNT)
		, m_DLCShadowMaps(EG_CASCADES_COUNT)
		, m_DLCDShadowMaps(EG_CASCADES_COUNT)
		, m_DLShadowMapSamplers(EG_CASCADES_COUNT)
	{
		bVolumetricLightsEnabled = m_Renderer.GetOptions().VolumetricSettings.bEnable;
		bTranslucencyShadowsEnabled = m_Renderer.GetOptions().bTranslucentShadows;

		std::fill(m_DLShadowMaps.begin(), m_DLShadowMaps.end(), RenderManager::GetDummyDepthImage());
		std::fill(m_DLCShadowMaps.begin(), m_DLCShadowMaps.end(), RenderManager::GetDummyImage());
		std::fill(m_DLCDShadowMaps.begin(), m_DLCDShadowMaps.end(), RenderManager::GetDummyImageR16());

		InitOpacityMaskedMeshPipelines();
		InitTranslucentMeshPipelines();

		InitOpacitySkeletalMeshPipelines();
		InitMaskedSkeletalMeshPipelines();
		InitTranslucentSkeletalMeshPipelines();

		InitOpacitySpritesPipelines();
		InitMaskedSpritesPipelines();
		InitTranslucentSpritesPipelines();
		
		InitOpaqueLitTextsPipelines();
		InitMaskedLitTextsPipelines();
		InitTranslucentLitTextsPipelines();

		InitUnlitTextsPipelines();

		BufferSpecifications pointLightsVPBufferSpecs;
		pointLightsVPBufferSpecs.Size = sizeof(glm::mat4) * 6 * 10;
		pointLightsVPBufferSpecs.Layout = BufferLayoutType::StorageBuffer;
		pointLightsVPBufferSpecs.Usage = BufferUsage::StorageBuffer | BufferUsage::TransferDst;
		m_PLVPsBuffer = Buffer::Create(pointLightsVPBufferSpecs, "PointLightsVPs");

		InitWithOptions(m_Renderer.GetOptions());
	}

	void ShadowPassTask::RecordCommandBuffer(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "Shadow pass");
		EG_CPU_TIMING_SCOPED("Shadow pass");

		bDidDrawDL = false;
		bDidDrawPL = false;
		bDidDrawSL = false;
		bDidDrawDLC = false;
		bDidDrawPLC = false;
		bDidDrawSLC = false;

		HandlePointLightResources(cmd);
		HandleSpotLightResources(cmd);

		ShadowPassOpacityMeshes(cmd);
		ShadowPassMaskedMeshes(cmd);
		ShadowPassOpacitySkeletalMeshes(cmd);
		ShadowPassMaskedSkeletalMeshes(cmd);
		ShadowPassOpacitySprites(cmd);
		ShadowPassMaskedSprites(cmd);
		ShadowPassOpaqueLitTexts(cmd);
		ShadowPassMaskedLitTexts(cmd);
		ShadowPassUnlitTexts(cmd);

		// Clears framebuffers if they weren't
		// It's required so that shadowmaps don't have invalid values and shading translucent objects to work correctly
		ClearFramebuffers(cmd);
		
		if (bTranslucencyShadowsEnabled)
		{
			ShadowPassTranslucentMeshes(cmd);
			ShadowPassTranslucentSkeletalMeshes(cmd);
			ShadowPassTranslucentSprites(cmd);
			ShadowPassTranslucentLitTexts(cmd);
		}
	}

	void ShadowPassTask::HandlePointLightResources(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "Shadow pass. Handle PointLight Resources");
		EG_CPU_TIMING_SCOPED("Shadow pass. Handle PointLight Resources");

		const auto& pointLights = m_Renderer.GetPointLights();
		auto& framebuffers = m_PLFramebuffers;
		auto& shadowMaps = m_PLShadowMaps;
		auto& coloredShadowMaps = m_PLCShadowMaps;
		auto& depthShadowMaps = m_PLCDShadowMaps;
		const auto& pipeline = m_OpacityMPLPipeline;

		auto& translucentFramebuffers = m_PLCFramebuffers;
		auto& translucentFramebuffers_NoDepth = m_PLCFramebuffers_NoDepth;
		auto& translucentPipeline = m_TranslucentMPLPipeline;
		auto& translucentPipeline_NoDepth = m_TranslucentMPLPipeline_NoDepth;

		m_PLVPs.clear();
		m_PointLightIndices.clear();
		uint32_t pointLightsCount = 0;
		const glm::vec3 cameraPos = m_Renderer.GetViewPosition();
		const float shadowMaxDistance = m_Renderer.GetShadowMaxDistance();
		for (size_t plIndex = 0; plIndex < pointLights.size(); ++plIndex)
		{
			auto& pointLight = pointLights[plIndex];
			if (!pointLight.DoesCastShadows())
				continue;

			for (uint32_t i = 0; i < 6; ++i)
			{
				m_PLVPs.push_back(pointLight.ViewProj[i]);
			}

			m_PointLightIndices.push_back(plIndex);
			const float distanceToCamera = glm::length(cameraPos - pointLight.Position);
			const uint32_t& i = pointLightsCount;

			const glm::uvec3 smSize = glm::uvec3(GetPointLightSMSize(distanceToCamera, shadowMaxDistance), 1u);
			if (i >= framebuffers.size())
			{
				// Create SM & framebuffer
				shadowMaps.emplace_back(CreateDepthImage(smSize, "PointLight_SM" + std::to_string(i), true));
				framebuffers.push_back(Framebuffer::Create({ shadowMaps[i] }, smSize, pipeline->GetRenderPassHandle()));
			}
			else if (glm::uvec2(smSize) != framebuffers[i]->GetSize())
			{
				// Create SM & framebuffer with the new size
				shadowMaps[i] = CreateDepthImage(smSize, "PointLight_SM" + std::to_string(i), true);
				framebuffers[i] = Framebuffer::Create({ shadowMaps[i] }, smSize, pipeline->GetRenderPassHandle());
			}

			if (bTranslucencyShadowsEnabled)
			{
				bool bUpdateFb = false;
				if (i < coloredShadowMaps.size())
				{
					if (smSize != coloredShadowMaps[i]->GetSize())
					{
						coloredShadowMaps[i] = CreateColoredFilterImage(smSize, "PointLight_SMC" + std::to_string(i), true);
						bUpdateFb = true;
					}
				}
				else
				{
					coloredShadowMaps.emplace_back(CreateColoredFilterImage(smSize, "PointLight_SMC" + std::to_string(i), true));
					bUpdateFb = true;
				}

				if (bVolumetricLightsEnabled)
				{
					if (i < depthShadowMaps.size())
					{
						if (smSize != depthShadowMaps[i]->GetSize())
						{
							depthShadowMaps[i] = CreateDepthImage16(smSize, "PointLight_SMCD" + std::to_string(i), true);
							bUpdateFb = true;
						}
					}
					else
					{
						depthShadowMaps.emplace_back(CreateDepthImage16(smSize, "PointLight_SMCD" + std::to_string(i), true));
						bUpdateFb = true;
					}
				}

				if (bUpdateFb)
				{
					std::vector<Ref<Image>> attachments;
					attachments.reserve(3);
					attachments.push_back(coloredShadowMaps[i]);
					if (bVolumetricLightsEnabled)
						attachments.push_back(depthShadowMaps[i]);

					// No depth
					Ref<Framebuffer> fb = Framebuffer::Create(attachments, smSize, translucentPipeline_NoDepth->GetRenderPassHandle());
					if (i >= translucentFramebuffers_NoDepth.size())
						translucentFramebuffers_NoDepth.push_back(fb);
					else
						translucentFramebuffers_NoDepth[i] = fb;

					// With depth
					attachments.push_back(shadowMaps[i]);
					fb = Framebuffer::Create(attachments, smSize, translucentPipeline->GetRenderPassHandle());
					if (i >= translucentFramebuffers.size())
						translucentFramebuffers.push_back(fb);
					else
						translucentFramebuffers[i] = fb;
				}
			}

			++pointLightsCount;
		}

		const size_t requiredSize = m_PLVPs.size() * sizeof(glm::mat4);
		if (m_PLVPsBuffer->GetSize() < requiredSize)
		{
			m_PLVPsBuffer->Resize((requiredSize * 3) / 2);
		}
		if (requiredSize > 0)
			cmd->Write(m_PLVPsBuffer, m_PLVPs.data(), requiredSize, 0, m_PLVPsBuffer->GetLayout(), BufferLayoutType::StorageBuffer);

		// Release unused shadow-maps & framebuffers
		shadowMaps.resize(pointLightsCount);
		framebuffers.resize(pointLightsCount);

		// Release unused shadow-maps & framebuffers
		if (bTranslucencyShadowsEnabled)
		{
			coloredShadowMaps.resize(pointLightsCount);
			if (bVolumetricLightsEnabled)
				depthShadowMaps.resize(pointLightsCount);
			translucentFramebuffers.resize(pointLightsCount);
			translucentFramebuffers_NoDepth.resize(pointLightsCount);
		}
	}
	
	void ShadowPassTask::HandleSpotLightResources(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "Shadow pass. Handle SpotLight Resources");
		EG_CPU_TIMING_SCOPED("Shadow pass. Handle SpotLight Resources");

		const auto& spotLights = m_Renderer.GetSpotLights();
		auto& framebuffers = m_SLFramebuffers;
		auto& shadowMaps = m_SLShadowMaps;
		auto& coloredShadowMaps = m_SLCShadowMaps;
		auto& depthShadowMaps = m_SLCDShadowMaps;
		const auto& pipeline = m_OpacityMSLPipeline;

		auto& translucentFramebuffers = m_SLCFramebuffers;
		auto& translucentFramebuffers_NoDepth = m_SLCFramebuffers_NoDepth;
		auto& translucentPipeline = m_TranslucentMSLPipeline;
		auto& translucentPipeline_NoDepth = m_TranslucentMSLPipeline_NoDepth;

		uint32_t spotLightsCount = 0;
		const glm::vec3 cameraPos = m_Renderer.GetViewPosition();
		const float shadowMaxDistance = m_Renderer.GetShadowMaxDistance();
		m_SpotLightIndices.clear();
		for (size_t slIndex = 0; slIndex < spotLights.size(); ++slIndex)
		{
			auto& spotLight = spotLights[slIndex];
			if (!spotLight.bCastsShadows)
				continue;

			m_SpotLightIndices.push_back(slIndex);

			const float distanceToCamera = glm::length(cameraPos - spotLight.Position);
			const uint32_t& i = spotLightsCount;

			const glm::uvec3 smSize = glm::uvec3(GetSpotLightSMSize(distanceToCamera, shadowMaxDistance), 1u);
			if (i >= framebuffers.size())
			{
				// Create SM & framebuffer
				shadowMaps.emplace_back(CreateDepthImage(smSize, "SpotLight_SM" + std::to_string(i), false));
				framebuffers.push_back(Framebuffer::Create({ shadowMaps[i] }, smSize, pipeline->GetRenderPassHandle()));
			}
			else if (glm::uvec2(smSize) != framebuffers[i]->GetSize())
			{
				// Create SM & framebuffer with the new size
				shadowMaps[i] = CreateDepthImage(smSize, "SpotLight_SM" + std::to_string(i), false);
				framebuffers[i] = Framebuffer::Create({ shadowMaps[i] }, smSize, pipeline->GetRenderPassHandle());
			}

			if (bTranslucencyShadowsEnabled)
			{
				bool bUpdateFb = false;
				if (i < coloredShadowMaps.size())
				{
					if (smSize != coloredShadowMaps[i]->GetSize())
					{
						coloredShadowMaps[i] = CreateColoredFilterImage(smSize, "SpotLight_SMC" + std::to_string(i), false);
						bUpdateFb = true;
					}
				}
				else
				{
					coloredShadowMaps.emplace_back(CreateColoredFilterImage(smSize, "SpotLight_SMC" + std::to_string(i), false));
					bUpdateFb = true;
				}

				if (bVolumetricLightsEnabled)
				{
					if (i < depthShadowMaps.size())
					{
						if (smSize != depthShadowMaps[i]->GetSize())
						{
							depthShadowMaps[i] = CreateDepthImage16(smSize, "SpotLight_SMCD" + std::to_string(i), false);
							bUpdateFb = true;
						}
					}
					else
					{
						depthShadowMaps.emplace_back(CreateDepthImage16(smSize, "SpotLight_SMCD" + std::to_string(i), false));
						bUpdateFb = true;
					}
				}

				if (bUpdateFb)
				{
					std::vector<Ref<Image>> attachments;
					attachments.reserve(3);
					attachments.push_back(coloredShadowMaps[i]);
					if (bVolumetricLightsEnabled)
						attachments.push_back(depthShadowMaps[i]);

					// No depth
					Ref<Framebuffer> fb = Framebuffer::Create(attachments, smSize, translucentPipeline_NoDepth->GetRenderPassHandle());
					if (i >= translucentFramebuffers_NoDepth.size())
						translucentFramebuffers_NoDepth.push_back(fb);
					else
						translucentFramebuffers_NoDepth[i] = fb;

					// With depth
					attachments.push_back(shadowMaps[i]);
					fb = Framebuffer::Create(attachments, smSize, translucentPipeline->GetRenderPassHandle());
					if (i >= translucentFramebuffers.size())
						translucentFramebuffers.push_back(fb);
					else
						translucentFramebuffers[i] = fb;
				}
			}

			++spotLightsCount;
		}

		// Release unused shadow-maps & framebuffers
		shadowMaps.resize(spotLightsCount);
		framebuffers.resize(spotLightsCount);

		// Release unused shadow-maps & framebuffers
		if (bTranslucencyShadowsEnabled)
		{
			coloredShadowMaps.resize(spotLightsCount);
			if (bVolumetricLightsEnabled)
				depthShadowMaps.resize(spotLightsCount);
			translucentFramebuffers.resize(spotLightsCount);
			translucentFramebuffers_NoDepth.resize(spotLightsCount);
		}
	}

	void ShadowPassTask::ClearFramebuffers(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "Shadow pass. Clearing framebuffers");
		EG_CPU_TIMING_SCOPED("Shadow pass. Clearing framebuffers");

		if (!bDidDrawDL)
		{
			auto& framebuffers = m_DLFramebuffers;
			auto& pipeline = m_OpacityMDLPipelineClearing;
			for (uint32_t i = 0; i < framebuffers.size(); ++i)
			{
				cmd->BeginGraphics(pipeline, framebuffers[i]);
				cmd->EndGraphics();
			}
		}
		if (!bDidDrawPL)
		{
			auto& framebuffers = m_PLFramebuffers;
			auto& pipeline = m_OpacityMPLPipelineClearing;
			for (uint32_t i = 0; i < framebuffers.size(); ++i)
			{
				cmd->BeginGraphics(pipeline, framebuffers[i]);
				cmd->EndGraphics();
			}
		}
		if (!bDidDrawSL)
		{
			auto& framebuffers = m_SLFramebuffers;
			auto& pipeline = m_OpacityMSLPipelineClearing;
			for (uint32_t i = 0; i < framebuffers.size(); ++i)
			{
				cmd->BeginGraphics(pipeline, framebuffers[i]);
				cmd->EndGraphics();
			}
		}
		if (!bDidDrawDLC)
		{
			auto& framebuffers = m_DLCFramebuffers;
			auto& pipeline = m_TranslucentMDLPipeline;
			for (uint32_t i = 0; i < framebuffers.size(); ++i)
			{
				cmd->BeginGraphics(pipeline, framebuffers[i]);
				cmd->EndGraphics();
			}
		}
		if (!bDidDrawPLC)
		{
			auto& framebuffers = m_PLCFramebuffers;
			auto& pipeline = m_TranslucentMPLPipeline;
			for (uint32_t i = 0; i < framebuffers.size(); ++i)
			{
				cmd->BeginGraphics(pipeline, framebuffers[i]);
				cmd->EndGraphics();
			}
		}
		if (!bDidDrawSLC)
		{
			auto& framebuffers = m_SLCFramebuffers;
			auto& pipeline = m_TranslucentMSLPipeline;
			for (uint32_t i = 0; i < framebuffers.size(); ++i)
			{
				cmd->BeginGraphics(pipeline, framebuffers[i]);
				cmd->EndGraphics();
			}
		}
	}

	void ShadowPassTask::InitWithOptions(const SceneRendererSettings& settings)
	{
		if (settings.ShadowsSettings == m_Settings &&
			settings.VolumetricSettings.bEnable == bVolumetricLightsEnabled &&
			settings.bTranslucentShadows == bTranslucencyShadowsEnabled)
			return;

		const bool bDirLightChanged = !m_Settings.DirLightsEqual(settings.ShadowsSettings);
		const bool bPointLightChanged = m_Settings.PointLightShadowMapSize != settings.ShadowsSettings.PointLightShadowMapSize;
		const bool bSpotLightChanged = m_Settings.SpotLightShadowMapSize != settings.ShadowsSettings.SpotLightShadowMapSize;
		m_Settings = settings.ShadowsSettings;

		const bool bVolumetricChanged = bVolumetricLightsEnabled != settings.VolumetricSettings.bEnable;
		const bool bTranslucencyShadowsChanged = bTranslucencyShadowsEnabled != settings.bTranslucentShadows;

		bVolumetricLightsEnabled = settings.VolumetricSettings.bEnable;
		bTranslucencyShadowsEnabled = settings.bTranslucentShadows;

		// Disable if no translucent shadows
		bVolumetricLightsEnabled = bVolumetricLightsEnabled && bTranslucencyShadowsEnabled;

		if (bTranslucencyShadowsChanged || bVolumetricChanged)
		{
			InitTranslucentMeshPipelines();
			InitTranslucentSpritesPipelines();
			InitTranslucentLitTextsPipelines();
		}

		if (bDirLightChanged)
		{
			if (m_Renderer.HasDirectionalLight() && m_Renderer.GetDirectionalLight().bCastsShadows)
			{
				InitDirectionalLightShadowMaps();
				if (bTranslucencyShadowsEnabled)
				{
					InitColoredDirectionalLightShadowMaps();
					InitColoredDirectionalLightFramebuffers(m_DLCFramebuffers, m_TranslucentMDLPipeline, true);
				}
			}
		}
		else if (bTranslucencyShadowsEnabled && (bVolumetricChanged || bTranslucencyShadowsChanged))
		{
			if (m_DLShadowMaps[0] != RenderManager::GetDummyDepthImage()) // Init only if main DirLight shadows maps are initialized
				InitColoredDirectionalLightShadowMaps();
			
			if (m_DLCShadowMaps[0] != RenderManager::GetDummyImage())
			{
				InitColoredDirectionalLightFramebuffers(m_DLCFramebuffers, m_TranslucentMDLPipeline, true);
			}
		}

		if (!bTranslucencyShadowsEnabled)
			FreeColoredDirectionalLightShadowMaps();

		if (bPointLightChanged)
		{
			m_PLShadowMaps.clear();
			m_PLFramebuffers.clear();
			HandleColoredPointLightShadowMaps();
		}
		else if (bTranslucencyShadowsChanged || bVolumetricChanged)
			HandleColoredPointLightShadowMaps();

		if (bSpotLightChanged)
		{
			m_SLShadowMaps.clear();
			m_SLFramebuffers.clear();
			HandleColoredSpotLightShadowMaps();
		}
		else if (bTranslucencyShadowsChanged || bVolumetricChanged)
			HandleColoredSpotLightShadowMaps();
	}

	void ShadowPassTask::ShadowPassOpacityMeshes(const Ref<CommandBuffer>& cmd)
	{
		const auto& singleSided = m_Renderer.GetStaticMeshesDrawData().SingleSided.ShadowCastingOpaque;
		const auto& doubleSided = m_Renderer.GetStaticMeshesDrawData().DoubleSided.ShadowCastingOpaque;
		if (singleSided.empty() && doubleSided.empty())
			return;

		EG_GPU_TIMING_SCOPED(cmd, "Opacity Meshes shadow pass");
		EG_CPU_TIMING_SCOPED("Opacity Meshes shadow pass");

		const auto& buffers = m_Renderer.GetStaticMeshesBuffers();
		const auto& transformsBuffer = m_Renderer.GetMeshTransformsBuffer();
		const auto& dirLight = m_Renderer.GetDirectionalLight();
		auto& stats = m_Renderer.GetStats();

		// For directional light
		if (m_Renderer.HasDirectionalLight() && dirLight.bCastsShadows)
		{
			EG_GPU_TIMING_SCOPED(cmd, "Opacity Meshes: CSM Shadow pass");
			EG_CPU_TIMING_SCOPED("Opacity Meshes: CSM Shadow pass");

			CreateIfNeededDirectionalLightShadowMaps();
			m_OpacityMDLPipeline->SetBuffer(transformsBuffer, 0, 0);
			m_OpacityMDLPipelineClearing->SetBuffer(transformsBuffer, 0, 0);
			bDidDrawDL = true;
			for (uint32_t i = 0; i < m_DLFramebuffers.size(); ++i)
			{
				const auto& viewProj = dirLight.ViewProj[i];
				bool bDidDraw = false;
				if (!singleSided.empty())
				{
					cmd->SetGraphicsCullMode(CullMode::Front);
					const auto& pipeline = bDidDraw ? m_OpacityMDLPipeline : m_OpacityMDLPipelineClearing;
					RenderMeshesTask::Draw(cmd, pipeline, singleSided, buffers, stats, &viewProj, m_DLFramebuffers[i]);
					bDidDraw = true;
				}
				if (!doubleSided.empty())
				{
					cmd->SetGraphicsCullMode(CullMode::None);
					const auto& pipeline = bDidDraw ? m_OpacityMDLPipeline : m_OpacityMDLPipelineClearing;
					RenderMeshesTask::Draw(cmd, pipeline, doubleSided, buffers, stats, &viewProj, m_DLFramebuffers[i]);
					bDidDraw = true;
				}
			}
		}
		else
		{
			FreeDirectionalLightShadowMaps();
		}

		// For point lights
		{
			const auto& framebuffers = m_PLFramebuffers;
			{
				auto& vpsBuffer = m_PLVPsBuffer;
				m_OpacityMPLPipeline->SetBuffer(transformsBuffer, 0, 0);
				m_OpacityMPLPipeline->SetBuffer(vpsBuffer, 0, 1);
				m_OpacityMPLPipelineClearing->SetBuffer(transformsBuffer, 0, 0);
				m_OpacityMPLPipelineClearing->SetBuffer(vpsBuffer, 0, 1);
				{
					EG_GPU_TIMING_SCOPED(cmd, "Opacity Meshes: Point Lights Shadow pass");
					EG_CPU_TIMING_SCOPED("Opacity Meshes: Point Lights Shadow pass");

					uint32_t i = 0;
					for (const auto& index : m_PointLightIndices)
					{
						bool bDidDraw = false;
						if (!singleSided.empty())
						{
							cmd->SetGraphicsCullMode(CullMode::Front);
							const auto& pipeline = bDidDraw ? m_OpacityMPLPipeline : m_OpacityMPLPipelineClearing;
							RenderMeshesTask::Draw(cmd, pipeline, singleSided, buffers, stats, &i, framebuffers[i]);
							bDidDraw = true;
						}
						if (!doubleSided.empty())
						{
							cmd->SetGraphicsCullMode(CullMode::None);
							const auto& pipeline = bDidDraw ? m_OpacityMPLPipeline : m_OpacityMPLPipelineClearing;
							RenderMeshesTask::Draw(cmd, pipeline, doubleSided, buffers, stats, &i, framebuffers[i]);
							bDidDraw = true;
						}
						++i;
						bDidDrawPL = true;
					}
				}
			}
		}

		// For spot lights
		{
			const auto& spotLights = m_Renderer.GetSpotLights();
			uint32_t spotLightsCount = 0;
			auto& framebuffers = m_SLFramebuffers;
			{
				m_OpacityMSLPipeline->SetBuffer(transformsBuffer, 0, 0);
				m_OpacityMSLPipelineClearing->SetBuffer(transformsBuffer, 0, 0);
				{
					EG_GPU_TIMING_SCOPED(cmd, "Opacity Meshes: Spot Lights Shadow pass");
					EG_CPU_TIMING_SCOPED("Opacity Meshes: Spot Lights Shadow pass");

					for (const auto& index : m_SpotLightIndices)
					{
						const auto& spotLight = spotLights[index];
						const auto& viewProj = spotLight.ViewProj;
						const uint32_t& i = spotLightsCount;

						bool bDidDraw = false;
						if (!singleSided.empty())
						{
							cmd->SetGraphicsCullMode(CullMode::Front);
							const auto& pipeline = bDidDraw ? m_OpacityMSLPipeline : m_OpacityMSLPipelineClearing;
							RenderMeshesTask::Draw(cmd, pipeline, singleSided, buffers, stats, &viewProj, framebuffers[i]);
							bDidDraw = true;
						}
						if (!doubleSided.empty())
						{
							cmd->SetGraphicsCullMode(CullMode::None);
							const auto& pipeline = bDidDraw ? m_OpacityMSLPipeline : m_OpacityMSLPipelineClearing;
							RenderMeshesTask::Draw(cmd, pipeline, doubleSided, buffers, stats, &viewProj, framebuffers[i]);
							bDidDraw = true;
						}
						++spotLightsCount;
						bDidDrawSL = true;
					}
				}
			}
		}
	}

	void ShadowPassTask::ShadowPassTranslucentMeshes(const Ref<CommandBuffer>& cmd)
	{
		const auto& singleSided = m_Renderer.GetStaticMeshesDrawData().SingleSided.ShadowCastingTranslucent;
		const auto& doubleSided = m_Renderer.GetStaticMeshesDrawData().DoubleSided.ShadowCastingTranslucent;
		if (singleSided.empty() && doubleSided.empty())
			return;

		EG_GPU_TIMING_SCOPED(cmd, "Translucent Meshes shadow pass");
		EG_CPU_TIMING_SCOPED("Translucent Meshes shadow pass");

		const auto& buffers = m_Renderer.GetStaticMeshesBuffers();
		const auto& transformsBuffer = m_Renderer.GetMeshTransformsBuffer();
		const auto& dirLight = m_Renderer.GetDirectionalLight();
		auto& stats = m_Renderer.GetStats();
		const uint32_t currentFrameIndex = RenderManager::GetCurrentFrameIndex();

		// For directional light
		if (m_Renderer.HasDirectionalLight() && dirLight.bCastsShadows)
		{
			EG_GPU_TIMING_SCOPED(cmd, "Translucent Meshes: CSM Shadow pass");
			EG_CPU_TIMING_SCOPED("Translucent Meshes: CSM Shadow pass");

			auto& pipeline = bDidDrawDL ? m_TranslucentMDLPipeline : m_TranslucentMDLPipeline_NoDepth;
			auto& framebuffers = bDidDrawDL ? m_DLCFramebuffers : m_DLCFramebuffers_NoDepth;

			CreateIfNeededColoredDirectionalLightShadowMaps();
			if (bDidDrawDL)
				m_DLCFramebuffers_NoDepth.clear();
			else
				m_DLCFramebuffers.clear();
			
			if (framebuffers.empty())
				InitColoredDirectionalLightFramebuffers(framebuffers, pipeline, bDidDrawDL);

			pipeline->SetBuffer(transformsBuffer, 1, 0);

			const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
			const bool bTexturesDirty = texturesChangedFrame >= m_TranslucentMeshesDLTexturesUpdatedFrames[currentFrameIndex];
			if (bTexturesDirty)
			{
				m_TranslucentMDLPipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
				m_TranslucentMDLPipeline_NoDepth->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
				m_TranslucentMeshesDLTexturesUpdatedFrames[currentFrameIndex] = texturesChangedFrame + 1;
			}
			pipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
			pipeline->SetBuffer(MaterialSystem::GetMaterialsRawBuffer(), EG_PERSISTENT_SET, EG_BINDING_RAW_MATERIALS);

			for (uint32_t i = 0; i < framebuffers.size(); ++i)
			{
				const auto& viewProj = dirLight.ViewProj[i];
				cmd->SetGraphicsCullMode(CullMode::Front);
				RenderMeshesTask::Draw(cmd, pipeline, singleSided, buffers, stats, &viewProj, framebuffers[i]);
				cmd->SetGraphicsCullMode(CullMode::None);
				RenderMeshesTask::Draw(cmd, pipeline, doubleSided, buffers, stats, &viewProj, framebuffers[i]);
			}
			bDidDrawDLC = true;
		}
		else
		{
			FreeColoredDirectionalLightShadowMaps();
		}

		// For point lights
		{
			const auto& framebuffers = bDidDrawPL ? m_PLCFramebuffers : m_PLCFramebuffers_NoDepth;
			{
				auto& vpsBuffer = m_PLVPsBuffer;
				auto& pipeline = bDidDrawPL ? m_TranslucentMPLPipeline : m_TranslucentMPLPipeline_NoDepth;
				pipeline->SetBuffer(transformsBuffer, 1, 0);
				pipeline->SetBuffer(vpsBuffer, 1, 1);

				const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
				const bool bTexturesDirty = texturesChangedFrame >= m_TranslucentMeshesPLTexturesUpdatedFrames[currentFrameIndex];
				if (bTexturesDirty)
				{
					m_TranslucentMPLPipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
					m_TranslucentMPLPipeline_NoDepth->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
					m_TranslucentMeshesPLTexturesUpdatedFrames[currentFrameIndex] = texturesChangedFrame + 1;
				}
				pipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
				pipeline->SetBuffer(MaterialSystem::GetMaterialsRawBuffer(), EG_PERSISTENT_SET, EG_BINDING_RAW_MATERIALS);

				{
					EG_GPU_TIMING_SCOPED(cmd, "Translucent Meshes: Point Lights Shadow pass");
					EG_CPU_TIMING_SCOPED("Translucent Meshes: Point Lights Shadow pass");

					uint32_t i = 0;
					for (auto& index : m_PointLightIndices)
					{
						cmd->SetGraphicsCullMode(CullMode::Front);
						RenderMeshesTask::Draw(cmd, pipeline, singleSided, buffers, stats, &i, framebuffers[i]);
						cmd->SetGraphicsCullMode(CullMode::None);
						RenderMeshesTask::Draw(cmd, pipeline, doubleSided, buffers, stats, &i, framebuffers[i]);

						++i;
						bDidDrawPLC = true;
					}
				}
			}
		}

		// For spot lights
		{
			const auto& spotLights = m_Renderer.GetSpotLights();
			const auto& framebuffers = bDidDrawSL ? m_SLCFramebuffers : m_SLCFramebuffers_NoDepth;

			{
				auto& pipeline = bDidDrawSL ? m_TranslucentMSLPipeline : m_TranslucentMSLPipeline_NoDepth;
				pipeline->SetBuffer(transformsBuffer, 1, 0);

				const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
				const bool bTexturesDirty = texturesChangedFrame >= m_TranslucentMeshesSLTexturesUpdatedFrames[currentFrameIndex];
				if (bTexturesDirty)
				{
					m_TranslucentMSLPipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
					m_TranslucentMSLPipeline_NoDepth->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
					m_TranslucentMeshesSLTexturesUpdatedFrames[currentFrameIndex] = texturesChangedFrame + 1;
				}
				pipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
				pipeline->SetBuffer(MaterialSystem::GetMaterialsRawBuffer(), EG_PERSISTENT_SET, EG_BINDING_RAW_MATERIALS);

				{
					EG_GPU_TIMING_SCOPED(cmd, "Translucent Meshes: Spot Lights Shadow pass");
					EG_CPU_TIMING_SCOPED("Translucent Meshes: Spot Lights Shadow pass");

					uint32_t spotLightsCount = 0;
					for (const auto& index : m_SpotLightIndices)
					{
						const auto& spotLight = spotLights[index];
						const uint32_t& i = spotLightsCount;

						cmd->SetGraphicsCullMode(CullMode::Front);
						RenderMeshesTask::Draw(cmd, pipeline, singleSided, buffers, stats, &spotLight.ViewProj, framebuffers[i]);
						cmd->SetGraphicsCullMode(CullMode::None);
						RenderMeshesTask::Draw(cmd, pipeline, doubleSided, buffers, stats, &spotLight.ViewProj, framebuffers[i]);

						++spotLightsCount;
						bDidDrawSLC = true;
					}
				}
			}
		}
	}
	
	void ShadowPassTask::ShadowPassMaskedMeshes(const Ref<CommandBuffer>& cmd)
	{
		const auto& singleSided = m_Renderer.GetStaticMeshesDrawData().SingleSided.ShadowCastingMasked;
		const auto& doubleSided = m_Renderer.GetStaticMeshesDrawData().DoubleSided.ShadowCastingMasked;
		if (singleSided.empty() && doubleSided.empty())
			return;

		EG_GPU_TIMING_SCOPED(cmd, "Masked Meshes shadow pass");
		EG_CPU_TIMING_SCOPED("Masked Meshes shadow pass");

		const auto& buffers = m_Renderer.GetStaticMeshesBuffers();
		const auto& transformsBuffer = m_Renderer.GetMeshTransformsBuffer();
		const auto& dirLight = m_Renderer.GetDirectionalLight();
		auto& stats = m_Renderer.GetStats();

		const uint32_t currentFrameIndex = RenderManager::GetCurrentFrameIndex();

		// For directional light
		if (m_Renderer.HasDirectionalLight() && dirLight.bCastsShadows)
		{
			EG_GPU_TIMING_SCOPED(cmd, "Masked Meshes: CSM Shadow pass");
			EG_CPU_TIMING_SCOPED("Masked Meshes: CSM Shadow pass");

			auto& pipeline = bDidDrawDL ? m_MaskedMDLPipeline : m_MaskedMDLPipelineClearing;

			CreateIfNeededDirectionalLightShadowMaps();
			pipeline->SetBuffer(transformsBuffer, 1, 0);

			const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
			const bool bTexturesDirty = texturesChangedFrame >= m_MaskedMeshesDLTexturesUpdatedFrames[currentFrameIndex];
			if (bTexturesDirty)
			{
				m_MaskedMDLPipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
				m_MaskedMDLPipelineClearing->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
				m_MaskedMeshesDLTexturesUpdatedFrames[currentFrameIndex] = texturesChangedFrame + 1;
			}
			pipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
			pipeline->SetBuffer(MaterialSystem::GetMaterialsRawBuffer(), EG_PERSISTENT_SET, EG_BINDING_RAW_MATERIALS);

			for (uint32_t i = 0; i < m_DLFramebuffers.size(); ++i)
			{
				const auto& viewProj = dirLight.ViewProj[i];
				cmd->SetGraphicsCullMode(CullMode::Front);
				RenderMeshesTask::Draw(cmd, pipeline, singleSided, buffers, stats, &viewProj, m_DLFramebuffers[i]);
				cmd->SetGraphicsCullMode(CullMode::None);
				RenderMeshesTask::Draw(cmd, pipeline, doubleSided, buffers, stats, &viewProj, m_DLFramebuffers[i]);
			}
			bDidDrawDL = true;
		}
		else
		{
			FreeDirectionalLightShadowMaps();
		}

		// For point lights
		{
			auto& framebuffers = m_PLFramebuffers;
			{
				auto& vpsBuffer = m_PLVPsBuffer;
				auto& pipeline = bDidDrawPL ? m_MaskedMPLPipeline : m_MaskedMPLPipelineClearing;
				pipeline->SetBuffer(transformsBuffer, 1, 0);
				pipeline->SetBuffer(vpsBuffer, 1, 1);

				const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
				const bool bTexturesDirty = texturesChangedFrame >= m_MaskedMeshesPLTexturesUpdatedFrames[currentFrameIndex];
				if (bTexturesDirty)
				{
					m_MaskedMPLPipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
					m_MaskedMPLPipelineClearing->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
					m_MaskedMeshesPLTexturesUpdatedFrames[currentFrameIndex] = texturesChangedFrame + 1;
				}
				pipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
				pipeline->SetBuffer(MaterialSystem::GetMaterialsRawBuffer(), EG_PERSISTENT_SET, EG_BINDING_RAW_MATERIALS);

				{
					EG_GPU_TIMING_SCOPED(cmd, "Masked Meshes: Point Lights Shadow pass");
					EG_CPU_TIMING_SCOPED("Masked Meshes: Point Lights Shadow pass");

					uint32_t i = 0;
					for (const auto& index : m_PointLightIndices)
					{
						cmd->SetGraphicsCullMode(CullMode::Front);
						RenderMeshesTask::Draw(cmd, pipeline, singleSided, buffers, stats, &i, framebuffers[i]);
						cmd->SetGraphicsCullMode(CullMode::None);
						RenderMeshesTask::Draw(cmd, pipeline, doubleSided, buffers, stats, &i, framebuffers[i]);

						++i;
						bDidDrawPL = true;
					}
				}
			}
		}

		// For spot lights
		{
			const auto& spotLights = m_Renderer.GetSpotLights();
			uint32_t spotLightsCount = 0;
			auto& framebuffers = m_SLFramebuffers;
			{
				auto& pipeline = bDidDrawSL ? m_MaskedMSLPipeline : m_MaskedMSLPipelineClearing;
				pipeline->SetBuffer(transformsBuffer, 1, 0);

				const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
				const bool bTexturesDirty = texturesChangedFrame >= m_MaskedMeshesSLTexturesUpdatedFrames[currentFrameIndex];
				if (bTexturesDirty)
				{
					m_MaskedMSLPipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
					m_MaskedMSLPipelineClearing->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
					m_MaskedMeshesSLTexturesUpdatedFrames[currentFrameIndex] = texturesChangedFrame + 1;
				}
				pipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
				pipeline->SetBuffer(MaterialSystem::GetMaterialsRawBuffer(), EG_PERSISTENT_SET, EG_BINDING_RAW_MATERIALS);

				{
					EG_GPU_TIMING_SCOPED(cmd, "Masked Meshes: Spot Lights Shadow pass");
					EG_CPU_TIMING_SCOPED("Masked Meshes: Spot Lights Shadow pass");

					for (const auto& index : m_SpotLightIndices)
					{
						const auto& spotLight = spotLights[index];
						const uint32_t& i = spotLightsCount;

						cmd->SetGraphicsCullMode(CullMode::Front);
						RenderMeshesTask::Draw(cmd, pipeline, singleSided, buffers, stats, &spotLight.ViewProj, framebuffers[i]);
						cmd->SetGraphicsCullMode(CullMode::None);
						RenderMeshesTask::Draw(cmd, pipeline, doubleSided, buffers, stats, &spotLight.ViewProj, framebuffers[i]);

						++spotLightsCount;
						bDidDrawSL = true;
					}
				}
			}
		}
	}
	
	void ShadowPassTask::ShadowPassOpacitySkeletalMeshes(const Ref<CommandBuffer>& cmd)
	{
		const auto& singleSided = m_Renderer.GetSkeletalMeshesDrawData().SingleSided.ShadowCastingOpaque;
		const auto& doubleSided = m_Renderer.GetSkeletalMeshesDrawData().DoubleSided.ShadowCastingOpaque;
		if (singleSided.empty() && doubleSided.empty())
			return;

		EG_GPU_TIMING_SCOPED(cmd, "Opacity Skeletal Meshes shadow pass");
		EG_CPU_TIMING_SCOPED("Opacity Skeletal Meshes shadow pass");

		const auto& skinnedVertices = m_Renderer.GetSkinnedVertices();
		const auto& buffers = m_Renderer.GetSkeletalMeshesBuffers();
		const auto& dirLight = m_Renderer.GetDirectionalLight();
		auto& stats = m_Renderer.GetStats();

		// For directional light
		if (m_Renderer.HasDirectionalLight() && dirLight.bCastsShadows)
		{
			EG_GPU_TIMING_SCOPED(cmd, "Opacity Skeletal Meshes: CSM Shadow pass");
			EG_CPU_TIMING_SCOPED("Opacity Skeletal Meshes: CSM Shadow pass");
			
			CreateIfNeededDirectionalLightShadowMaps();
			
			auto& pipeline = bDidDrawDL ? m_OpacitySMDLPipeline : m_OpacitySMDLPipelineClearing;
			pipeline->SetBuffer(skinnedVertices, 0, 0);
			for (uint32_t i = 0; i < m_DLFramebuffers.size(); ++i)
			{
				const auto& viewProj = dirLight.ViewProj[i];
				cmd->SetGraphicsCullMode(CullMode::Front);
				RenderSkeletalMeshesTask::Draw(cmd, pipeline, singleSided, buffers, stats, DataBufferView(&viewProj, sizeof(viewProj)), m_DLFramebuffers[i]);
				cmd->SetGraphicsCullMode(CullMode::None);
				RenderSkeletalMeshesTask::Draw(cmd, pipeline, doubleSided, buffers, stats, DataBufferView(&viewProj, sizeof(viewProj)), m_DLFramebuffers[i]);
			}
			bDidDrawDL = true;
		}
		else
		{
			FreeDirectionalLightShadowMaps();
		}

		// For point lights
		{
			const auto& framebuffers = m_PLFramebuffers;
			{
				auto& vpsBuffer = m_PLVPsBuffer;
				auto& pipeline = bDidDrawPL ? m_OpacitySMPLPipeline : m_OpacitySMPLPipelineClearing;
				pipeline->SetBuffer(skinnedVertices, 0, 0);
				pipeline->SetBuffer(vpsBuffer, 0, 2);
				{
					EG_GPU_TIMING_SCOPED(cmd, "Opacity Skeletal Meshes: Point Lights Shadow pass");
					EG_CPU_TIMING_SCOPED("Opacity Skeletal Meshes: Point Lights Shadow pass");

					uint32_t i = 0;
					for (const auto& index : m_PointLightIndices)
					{
						cmd->SetGraphicsCullMode(CullMode::Front);
						RenderSkeletalMeshesTask::Draw(cmd, pipeline, singleSided, buffers, stats, DataBufferView(&i, sizeof(i)), framebuffers[i]);
						cmd->SetGraphicsCullMode(CullMode::None);
						RenderSkeletalMeshesTask::Draw(cmd, pipeline, doubleSided, buffers, stats, DataBufferView(&i, sizeof(i)), framebuffers[i]);

						++i;
						bDidDrawPL = true;
					}
				}
			}
		}

		// For spot lights
		{
			const auto& spotLights = m_Renderer.GetSpotLights();
			uint32_t spotLightsCount = 0;
			auto& framebuffers = m_SLFramebuffers;
			{
				auto& pipeline = bDidDrawSL ? m_OpacitySMSLPipeline : m_OpacitySMSLPipelineClearing;
				pipeline->SetBuffer(skinnedVertices, 0, 0);
				{
					EG_GPU_TIMING_SCOPED(cmd, "Opacity Skeletal Meshes: Spot Lights Shadow pass");
					EG_CPU_TIMING_SCOPED("Opacity Skeletal Meshes: Spot Lights Shadow pass");

					for (const auto& index : m_SpotLightIndices)
					{
						const auto& spotLight = spotLights[index];
						const auto& viewProj = spotLight.ViewProj;
						const uint32_t& i = spotLightsCount;

						cmd->SetGraphicsCullMode(CullMode::Front);
						RenderSkeletalMeshesTask::Draw(cmd, pipeline, singleSided, buffers, stats, DataBufferView(&viewProj, sizeof(viewProj)), framebuffers[i]);
						cmd->SetGraphicsCullMode(CullMode::None);
						RenderSkeletalMeshesTask::Draw(cmd, pipeline, doubleSided, buffers, stats, DataBufferView(&viewProj, sizeof(viewProj)), framebuffers[i]);

						++spotLightsCount;
						bDidDrawSL = true;
					}
				}
			}
		}
	}

	void ShadowPassTask::ShadowPassTranslucentSkeletalMeshes(const Ref<CommandBuffer>& cmd)
	{
		const auto& singleSided = m_Renderer.GetSkeletalMeshesDrawData().SingleSided.ShadowCastingTranslucent;
		const auto& doubleSided = m_Renderer.GetSkeletalMeshesDrawData().DoubleSided.ShadowCastingTranslucent;
		if (singleSided.empty() && doubleSided.empty())
			return;

		EG_GPU_TIMING_SCOPED(cmd, "Translucent Skeletal Meshes shadow pass");
		EG_CPU_TIMING_SCOPED("Translucent Skeletal Meshes shadow pass");

		const auto& skinnedVertices = m_Renderer.GetSkinnedVertices();
		const auto& buffers = m_Renderer.GetSkeletalMeshesBuffers();
		const auto& dirLight = m_Renderer.GetDirectionalLight();
		auto& stats = m_Renderer.GetStats();
		const uint32_t currentFrameIndex = RenderManager::GetCurrentFrameIndex();

		// For directional light
		if (m_Renderer.HasDirectionalLight() && dirLight.bCastsShadows)
		{
			EG_GPU_TIMING_SCOPED(cmd, "Translucent Skeletal Meshes: CSM Shadow pass");
			EG_CPU_TIMING_SCOPED("Translucent Skeletal Meshes: CSM Shadow pass");

			auto& pipeline = bDidDrawDL ?
				bDidDrawDLC ? m_TranslucentSMDLPipeline : m_TranslucentSMDLPipelineClearing :
				bDidDrawDLC ? m_TranslucentSMDLPipeline_NoDepth : m_TranslucentSMDLPipelineClearing_NoDepth;
			auto& framebuffers = bDidDrawDL ? m_DLCFramebuffers : m_DLCFramebuffers_NoDepth;

			CreateIfNeededColoredDirectionalLightShadowMaps();
			if (bDidDrawDL)
				m_DLCFramebuffers_NoDepth.clear();
			else
				m_DLCFramebuffers.clear();
			
			if (framebuffers.empty())
				InitColoredDirectionalLightFramebuffers(framebuffers, pipeline, bDidDrawDL);

			pipeline->SetBuffer(skinnedVertices, 1, 0);
			pipeline->SetBuffer(buffers.InstanceBuffer, 1, 1);

			const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
			const bool bTexturesDirty = texturesChangedFrame >= m_TranslucentSkeletalMeshesDLTexturesUpdatedFrames[currentFrameIndex];
			if (bTexturesDirty)
			{
				m_TranslucentSMDLPipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
				m_TranslucentSMDLPipelineClearing->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
				m_TranslucentSMDLPipeline_NoDepth->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
				m_TranslucentSMDLPipelineClearing_NoDepth->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
				m_TranslucentSkeletalMeshesDLTexturesUpdatedFrames[currentFrameIndex] = texturesChangedFrame + 1;
			}
			pipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
			pipeline->SetBuffer(MaterialSystem::GetMaterialsRawBuffer(), EG_PERSISTENT_SET, EG_BINDING_RAW_MATERIALS);

			for (uint32_t i = 0; i < framebuffers.size(); ++i)
			{
				const auto& viewProj = dirLight.ViewProj[i];
				cmd->SetGraphicsCullMode(CullMode::Front);
				RenderSkeletalMeshesTask::Draw(cmd, pipeline, singleSided, buffers, stats, DataBufferView(&viewProj, sizeof(viewProj)), framebuffers[i]);
				cmd->SetGraphicsCullMode(CullMode::None);
				RenderSkeletalMeshesTask::Draw(cmd, pipeline, doubleSided, buffers, stats, DataBufferView(&viewProj, sizeof(viewProj)), framebuffers[i]);
			}
			bDidDrawDLC = true;
		}
		else
		{
			FreeColoredDirectionalLightShadowMaps();
		}

		// For point lights
		{
			const auto& framebuffers = bDidDrawPL ? m_PLCFramebuffers : m_PLCFramebuffers_NoDepth;
			{
				auto& vpsBuffer = m_PLVPsBuffer;
				auto& pipeline = bDidDrawPL ?
					bDidDrawPLC ? m_TranslucentSMPLPipeline : m_TranslucentSMPLPipelineClearing :
					bDidDrawPLC ? m_TranslucentSMPLPipeline_NoDepth : m_TranslucentSMPLPipelineClearing_NoDepth;
				pipeline->SetBuffer(skinnedVertices, 1, 0);
				pipeline->SetBuffer(buffers.InstanceBuffer, 1, 1);
				pipeline->SetBuffer(vpsBuffer, 1, 2);

				const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
				const bool bTexturesDirty = texturesChangedFrame >= m_TranslucentSkeletalMeshesPLTexturesUpdatedFrames[currentFrameIndex];
				if (bTexturesDirty)
				{
					m_TranslucentSMPLPipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
					m_TranslucentSMPLPipeline_NoDepth->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
					m_TranslucentSMPLPipelineClearing->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
					m_TranslucentSMPLPipelineClearing_NoDepth->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
					m_TranslucentSkeletalMeshesDLTexturesUpdatedFrames[currentFrameIndex] = texturesChangedFrame + 1;
				}
				pipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
				pipeline->SetBuffer(MaterialSystem::GetMaterialsRawBuffer(), EG_PERSISTENT_SET, EG_BINDING_RAW_MATERIALS);

				{
					EG_GPU_TIMING_SCOPED(cmd, "Translucent Skeletal Meshes: Point Lights Shadow pass");
					EG_CPU_TIMING_SCOPED("Translucent Skeletal Meshes: Point Lights Shadow pass");

					uint32_t i = 0;
					for (const auto& index : m_PointLightIndices)
					{
						cmd->SetGraphicsCullMode(CullMode::Front);
						RenderSkeletalMeshesTask::Draw(cmd, pipeline, singleSided, buffers, stats, DataBufferView(&i, sizeof(i)), framebuffers[i]);
						cmd->SetGraphicsCullMode(CullMode::None);
						RenderSkeletalMeshesTask::Draw(cmd, pipeline, doubleSided, buffers, stats, DataBufferView(&i, sizeof(i)), framebuffers[i]);

						++i;
						bDidDrawPLC = true;
					}
				}
			}
		}

		// For spot lights
		{
			const auto& spotLights = m_Renderer.GetSpotLights();
			const auto& framebuffers = bDidDrawSL ? m_SLCFramebuffers : m_SLCFramebuffers_NoDepth;

			{
				auto& pipeline = bDidDrawSL ?
					bDidDrawSLC ? m_TranslucentSMSLPipeline : m_TranslucentSMSLPipelineClearing :
					bDidDrawSLC ? m_TranslucentSMSLPipeline_NoDepth : m_TranslucentSMSLPipelineClearing_NoDepth;
				pipeline->SetBuffer(skinnedVertices, 1, 0);
				pipeline->SetBuffer(buffers.InstanceBuffer, 1, 1);

				const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
				const bool bTexturesDirty = texturesChangedFrame >= m_TranslucentSkeletalMeshesSLTexturesUpdatedFrames[currentFrameIndex];
				if (bTexturesDirty)
				{
					m_TranslucentSMSLPipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
					m_TranslucentSMSLPipeline_NoDepth->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
					m_TranslucentSMSLPipelineClearing->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
					m_TranslucentSMSLPipelineClearing_NoDepth->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
					m_TranslucentSkeletalMeshesSLTexturesUpdatedFrames[currentFrameIndex] = texturesChangedFrame + 1;
				}
				pipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
				pipeline->SetBuffer(MaterialSystem::GetMaterialsRawBuffer(), EG_PERSISTENT_SET, EG_BINDING_RAW_MATERIALS);

				{
					EG_GPU_TIMING_SCOPED(cmd, "Translucent Skeletal Meshes: Spot Lights Shadow pass");
					EG_CPU_TIMING_SCOPED("Translucent Skeletal Meshes: Spot Lights Shadow pass");

					uint32_t spotLightsCount = 0;
					for (const auto& index : m_SpotLightIndices)
					{
						const auto& spotLight = spotLights[index];
						const auto& viewProj = spotLight.ViewProj;
						const uint32_t& i = spotLightsCount;

						cmd->SetGraphicsCullMode(CullMode::Front);
						RenderSkeletalMeshesTask::Draw(cmd, pipeline, singleSided, buffers, stats, DataBufferView(&viewProj, sizeof(viewProj)), framebuffers[i]);
						cmd->SetGraphicsCullMode(CullMode::None);
						RenderSkeletalMeshesTask::Draw(cmd, pipeline, doubleSided, buffers, stats, DataBufferView(&viewProj, sizeof(viewProj)), framebuffers[i]);

						++spotLightsCount;
						bDidDrawSLC = true;
					}
				}
			}
		}
	}
	
	void ShadowPassTask::ShadowPassMaskedSkeletalMeshes(const Ref<CommandBuffer>& cmd)
	{
		const auto& singleSided = m_Renderer.GetSkeletalMeshesDrawData().SingleSided.ShadowCastingMasked;
		const auto& doubleSided = m_Renderer.GetSkeletalMeshesDrawData().DoubleSided.ShadowCastingMasked;
		if (singleSided.empty() && doubleSided.empty())
			return;

		EG_GPU_TIMING_SCOPED(cmd, "Masked Skeletal Meshes shadow pass");
		EG_CPU_TIMING_SCOPED("Masked Skeletal Meshes shadow pass");

		const auto& skinnedVertices = m_Renderer.GetSkinnedVertices();
		const auto& buffers = m_Renderer.GetSkeletalMeshesBuffers();
		const auto& dirLight = m_Renderer.GetDirectionalLight();
		auto& stats = m_Renderer.GetStats();

		const uint32_t currentFrameIndex = RenderManager::GetCurrentFrameIndex();

		// For directional light
		if (m_Renderer.HasDirectionalLight() && dirLight.bCastsShadows)
		{
			EG_GPU_TIMING_SCOPED(cmd, "Masked Skeletal Meshes: CSM Shadow pass");
			EG_CPU_TIMING_SCOPED("Masked Skeletal Meshes: CSM Shadow pass");

			auto& pipeline = bDidDrawDL ? m_MaskedSMDLPipeline : m_MaskedSMDLPipelineClearing;

			CreateIfNeededDirectionalLightShadowMaps();
			pipeline->SetBuffer(skinnedVertices, 1, 0);
			pipeline->SetBuffer(buffers.InstanceBuffer, 1, 1);

			const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
			const bool bTexturesDirty = texturesChangedFrame >= m_MaskedSkeletalMeshesDLTexturesUpdatedFrames[currentFrameIndex];
			if (bTexturesDirty)
			{
				m_MaskedSMDLPipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
				m_MaskedSMDLPipelineClearing->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
				m_MaskedSkeletalMeshesDLTexturesUpdatedFrames[currentFrameIndex] = texturesChangedFrame + 1;
			}
			pipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
			pipeline->SetBuffer(MaterialSystem::GetMaterialsRawBuffer(), EG_PERSISTENT_SET, EG_BINDING_RAW_MATERIALS);

			for (uint32_t i = 0; i < m_DLFramebuffers.size(); ++i)
			{
				const auto& viewProj = dirLight.ViewProj[i];
				cmd->SetGraphicsCullMode(CullMode::Front);
				RenderSkeletalMeshesTask::Draw(cmd, pipeline, singleSided, buffers, stats, DataBufferView(&viewProj, sizeof(viewProj)), m_DLFramebuffers[i]);
				cmd->SetGraphicsCullMode(CullMode::None);
				RenderSkeletalMeshesTask::Draw(cmd, pipeline, doubleSided, buffers, stats, DataBufferView(&viewProj, sizeof(viewProj)), m_DLFramebuffers[i]);
			}
			bDidDrawDL = true;
		}
		else
		{
			FreeDirectionalLightShadowMaps();
		}

		// For point lights
		{
			auto& framebuffers = m_PLFramebuffers;

			{
				auto& vpsBuffer = m_PLVPsBuffer;
				auto& pipeline = bDidDrawPL ? m_MaskedSMPLPipeline : m_MaskedSMPLPipelineClearing;
				pipeline->SetBuffer(skinnedVertices, 1, 0);
				pipeline->SetBuffer(buffers.InstanceBuffer, 1, 1);
				pipeline->SetBuffer(vpsBuffer, 1, 2);

				const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
				const bool bTexturesDirty = texturesChangedFrame >= m_MaskedSkeletalMeshesPLTexturesUpdatedFrames[currentFrameIndex];
				if (bTexturesDirty)
				{
					m_MaskedSMPLPipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
					m_MaskedSMPLPipelineClearing->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
					m_MaskedSkeletalMeshesPLTexturesUpdatedFrames[currentFrameIndex] = texturesChangedFrame + 1;
				}
				pipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
				pipeline->SetBuffer(MaterialSystem::GetMaterialsRawBuffer(), EG_PERSISTENT_SET, EG_BINDING_RAW_MATERIALS);

				{
					EG_GPU_TIMING_SCOPED(cmd, "Masked Skeletal Meshes: Point Lights Shadow pass");
					EG_CPU_TIMING_SCOPED("Masked Skeletal Meshes: Point Lights Shadow pass");

					uint32_t i = 0;
					for (const auto& index : m_PointLightIndices)
					{
						cmd->SetGraphicsCullMode(CullMode::Front);
						RenderSkeletalMeshesTask::Draw(cmd, pipeline, singleSided, buffers, stats, DataBufferView(&i, sizeof(i)), framebuffers[i]);
						cmd->SetGraphicsCullMode(CullMode::None);
						RenderSkeletalMeshesTask::Draw(cmd, pipeline, doubleSided, buffers, stats, DataBufferView(&i, sizeof(i)), framebuffers[i]);

						++i;
						bDidDrawPL = true;
					}
				}
			}
		}

		// For spot lights
		{
			const auto& spotLights = m_Renderer.GetSpotLights();
			uint32_t spotLightsCount = 0;
			auto& framebuffers = m_SLFramebuffers;
			{
				auto& pipeline = bDidDrawSL ? m_MaskedSMSLPipeline : m_MaskedSMSLPipelineClearing;
				pipeline->SetBuffer(skinnedVertices, 1, 0);
				pipeline->SetBuffer(buffers.InstanceBuffer, 1, 1);

				const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
				const bool bTexturesDirty = texturesChangedFrame >= m_MaskedSkeletalMeshesSLTexturesUpdatedFrames[currentFrameIndex];
				if (bTexturesDirty)
				{
					m_MaskedSMSLPipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
					m_MaskedSMSLPipelineClearing->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
					m_MaskedSkeletalMeshesSLTexturesUpdatedFrames[currentFrameIndex] = texturesChangedFrame + 1;
				}
				pipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
				pipeline->SetBuffer(MaterialSystem::GetMaterialsRawBuffer(), EG_PERSISTENT_SET, EG_BINDING_RAW_MATERIALS);

				{
					EG_GPU_TIMING_SCOPED(cmd, "Masked Skeletal Meshes: Spot Lights Shadow pass");
					EG_CPU_TIMING_SCOPED("Masked Skeletal Meshes: Spot Lights Shadow pass");

					for (const auto& index : m_SpotLightIndices)
					{
						const auto& spotLight = spotLights[index];
						const auto& viewProj = spotLight.ViewProj;
						const uint32_t& i = spotLightsCount;

						cmd->SetGraphicsCullMode(CullMode::Front);
						RenderSkeletalMeshesTask::Draw(cmd, pipeline, singleSided, buffers, stats, DataBufferView(&viewProj, sizeof(viewProj)), framebuffers[i]);
						cmd->SetGraphicsCullMode(CullMode::None);
						RenderSkeletalMeshesTask::Draw(cmd, pipeline, doubleSided, buffers, stats, DataBufferView(&viewProj, sizeof(viewProj)), framebuffers[i]);

						++spotLightsCount;
						bDidDrawSL = true;
					}
				}
			}
		}
	}
	
	void ShadowPassTask::ShadowPassOpacitySprites(const Ref<CommandBuffer>& cmd)
	{
		const auto& singleSided = m_Renderer.GetSingleSidedSpritesRenderData();
		const auto& doubleSided = m_Renderer.GetDoubleSidedSpritesRenderData();

		if (singleSided.Opaque.IsEmpty() && doubleSided.Opaque.IsEmpty())
			return;

		EG_GPU_TIMING_SCOPED(cmd, "Opacity Sprites shadow pass");
		EG_CPU_TIMING_SCOPED("Opacity Sprites shadow pass");

		const auto& transformsBuffer = m_Renderer.GetSpritesTransformsBuffer();
		auto& stats = m_Renderer.GetStats();

		// For directional light
		const auto& dirLight = m_Renderer.GetDirectionalLight();
		if (m_Renderer.HasDirectionalLight() && dirLight.bCastsShadows)
		{
			EG_GPU_TIMING_SCOPED(cmd, "Opacity Sprites: CSM Shadow pass");
			EG_CPU_TIMING_SCOPED("Opacity Sprites: CSM Shadow pass");

			CreateIfNeededDirectionalLightShadowMaps();

			auto& pipeline = bDidDrawDL ? m_OpacitySDLPipeline : m_OpacitySDLPipelineClearing;
			pipeline->SetBuffer(transformsBuffer, 0, 0);
			for (uint32_t i = 0; i < m_DLFramebuffers.size(); ++i)
			{
				cmd->SetGraphicsCullMode(CullMode::Front);
				RenderSpritesTask::Draw(cmd, pipeline, singleSided.Opaque.ShadowCastingQuads, &dirLight.ViewProj[i], stats, m_DLFramebuffers[i]);
				cmd->SetGraphicsCullMode(CullMode::None);
				RenderSpritesTask::Draw(cmd, pipeline, doubleSided.Opaque.ShadowCastingQuads, &dirLight.ViewProj[i], stats, m_DLFramebuffers[i]);
			}
			bDidDrawDL = true;
		}
		else
		{
			FreeDirectionalLightShadowMaps();
		}

		// Point lights
		if (m_PointLightIndices.size())
		{
			EG_GPU_TIMING_SCOPED(cmd, "Opacity Sprites: Point Lights Shadow pass");
			EG_CPU_TIMING_SCOPED("Opacity Sprites: Point Lights Shadow pass");

			auto& framebuffers = m_PLFramebuffers;
			auto& vpsBuffer = m_PLVPsBuffer;
			auto& pipeline = bDidDrawPL ? m_OpacitySPLPipeline : m_OpacitySPLPipelineClearing;
			pipeline->SetBuffer(transformsBuffer, 0, 0);
			pipeline->SetBuffer(vpsBuffer, 0, 1);

			uint32_t i = 0;
			for (const auto& index : m_PointLightIndices)
			{
				cmd->SetGraphicsCullMode(CullMode::Front);
				RenderSpritesTask::Draw(cmd, pipeline, singleSided.Opaque.ShadowCastingQuads, &i, stats, framebuffers[i]);
				cmd->SetGraphicsCullMode(CullMode::None);
				RenderSpritesTask::Draw(cmd, pipeline, doubleSided.Opaque.ShadowCastingQuads, &i, stats, framebuffers[i]);
				++i;
			}
		}

		// Spot lights
		if (m_SpotLightIndices.size())
		{
			EG_GPU_TIMING_SCOPED(cmd, "Opacity Sprites: Spot Lights Shadow pass");
			EG_CPU_TIMING_SCOPED("Opacity Sprites: Spot Lights Shadow pass");

			const auto& spotLights = m_Renderer.GetSpotLights();
			uint32_t spotLightsCount = 0;
			auto& framebuffers = m_SLFramebuffers;

			auto& pipeline = bDidDrawSL ? m_OpacitySSLPipeline : m_OpacitySSLPipelineClearing;
			pipeline->SetBuffer(transformsBuffer, 0, 0);

			for (const auto& index : m_SpotLightIndices)
			{
				const auto& spotLight = spotLights[index];
				const uint32_t& i = spotLightsCount;

				cmd->SetGraphicsCullMode(CullMode::Front);
				RenderSpritesTask::Draw(cmd, pipeline, singleSided.Opaque.ShadowCastingQuads, &spotLight.ViewProj, stats, framebuffers[i]);
				cmd->SetGraphicsCullMode(CullMode::None);
				RenderSpritesTask::Draw(cmd, pipeline, doubleSided.Opaque.ShadowCastingQuads, &spotLight.ViewProj, stats, framebuffers[i]);

				bDidDrawSL = true;
				++spotLightsCount;
			}
		}
	}
	
	void ShadowPassTask::ShadowPassTranslucentSprites(const Ref<CommandBuffer>& cmd)
	{
		const auto& singleSided = m_Renderer.GetSingleSidedSpritesRenderData();
		const auto& doubleSided = m_Renderer.GetDoubleSidedSpritesRenderData();

		if (singleSided.Translucent.IsEmpty() && doubleSided.Translucent.IsEmpty())
			return;

		EG_GPU_TIMING_SCOPED(cmd, "Translucent Sprites shadow pass");
		EG_CPU_TIMING_SCOPED("Translucent Sprites shadow pass");

		const auto& transformsBuffer = m_Renderer.GetSpritesTransformsBuffer();
		auto& stats = m_Renderer.GetStats();

		const uint32_t currentFrameIndex = RenderManager::GetCurrentFrameIndex();

		// For directional light
		const auto& dirLight = m_Renderer.GetDirectionalLight();
		if (m_Renderer.HasDirectionalLight() && dirLight.bCastsShadows)
		{
			EG_GPU_TIMING_SCOPED(cmd, "Translucent Sprites: CSM Shadow pass");
			EG_CPU_TIMING_SCOPED("Translucent Sprites: CSM Shadow pass");

			auto& pipeline = bDidDrawDL ?
				bDidDrawDLC ? m_TranslucentSDLPipeline : m_TranslucentSDLPipelineClearing :
				bDidDrawDLC ? m_TranslucentSDLPipeline_NoDepth : m_TranslucentSDLPipelineClearing_NoDepth;
			auto& framebuffers = bDidDrawDL ? m_DLCFramebuffers : m_DLCFramebuffers_NoDepth;

			CreateIfNeededColoredDirectionalLightShadowMaps();
			if (bDidDrawDL)
				m_DLCFramebuffers_NoDepth.clear();
			else
				m_DLCFramebuffers.clear();

			if (framebuffers.empty())
				InitColoredDirectionalLightFramebuffers(framebuffers, pipeline, bDidDrawDL);

			const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
			const bool bTexturesDirty = texturesChangedFrame >= m_TranslucentSpritesDLTexturesUpdatedFrames[currentFrameIndex];
			if (bTexturesDirty)
			{
				m_TranslucentSDLPipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
				m_TranslucentSDLPipelineClearing->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
				m_TranslucentSDLPipeline_NoDepth->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
				m_TranslucentSDLPipelineClearing_NoDepth->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
				m_TranslucentSpritesDLTexturesUpdatedFrames[currentFrameIndex] = texturesChangedFrame + 1;
			}
			pipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
			pipeline->SetBuffer(MaterialSystem::GetMaterialsRawBuffer(), EG_PERSISTENT_SET, EG_BINDING_RAW_MATERIALS);
			pipeline->SetBuffer(transformsBuffer, 1, 0);

			for (uint32_t i = 0; i < framebuffers.size(); ++i)
			{
				cmd->SetGraphicsCullMode(CullMode::Front);
				RenderSpritesTask::Draw(cmd, pipeline, singleSided.Translucent.ShadowCastingQuads, &dirLight.ViewProj[i], stats, framebuffers[i]);
				cmd->SetGraphicsCullMode(CullMode::None);
				RenderSpritesTask::Draw(cmd, pipeline, doubleSided.Translucent.ShadowCastingQuads, &dirLight.ViewProj[i], stats, framebuffers[i]);
			}
			bDidDrawDLC = true;
		}
		else
		{
			FreeColoredDirectionalLightShadowMaps();
		}

		// Point lights
		{
			const auto& framebuffers = bDidDrawPL ? m_PLCFramebuffers : m_PLCFramebuffers_NoDepth;

			if (m_PointLightIndices.size())
			{
				EG_GPU_TIMING_SCOPED(cmd, "Translucent Sprites: Point Lights Shadow pass");
				EG_CPU_TIMING_SCOPED("Translucent Sprites: Point Lights Shadow pass");

				auto& vpsBuffer = m_PLVPsBuffer;
				auto& pipeline = bDidDrawPL ?
					bDidDrawPLC ? m_TranslucentSPLPipeline : m_TranslucentSPLPipelineClearing :
					bDidDrawPLC ? m_TranslucentSPLPipeline_NoDepth : m_TranslucentSPLPipelineClearing_NoDepth;

				const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
				const bool bTexturesDirty = texturesChangedFrame >= m_TranslucentSpritesPLTexturesUpdatedFrames[currentFrameIndex];
				if (bTexturesDirty)
				{
					m_TranslucentSPLPipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
					m_TranslucentSPLPipelineClearing->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
					m_TranslucentSPLPipeline_NoDepth->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
					m_TranslucentSPLPipelineClearing_NoDepth->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
					m_TranslucentSpritesPLTexturesUpdatedFrames[currentFrameIndex] = texturesChangedFrame + 1;
				}
				pipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
				pipeline->SetBuffer(MaterialSystem::GetMaterialsRawBuffer(), EG_PERSISTENT_SET, EG_BINDING_RAW_MATERIALS);

				pipeline->SetBuffer(transformsBuffer, 1, 0);
				pipeline->SetBuffer(vpsBuffer, 1, 1);

				uint32_t pointLightsCount = 0;
				for (const auto& index : m_PointLightIndices)
				{
					const uint32_t& i = pointLightsCount;
					cmd->SetGraphicsCullMode(CullMode::Front);
					RenderSpritesTask::Draw(cmd, pipeline, singleSided.Translucent.ShadowCastingQuads, &i, stats, framebuffers[i]);
					cmd->SetGraphicsCullMode(CullMode::None);
					RenderSpritesTask::Draw(cmd, pipeline, doubleSided.Translucent.ShadowCastingQuads, &i, stats, framebuffers[i]);

					++pointLightsCount;
					bDidDrawPLC = true;
				}
			}
		}

		// Spot lights
		{
			const auto& framebuffers = bDidDrawSL ? m_SLCFramebuffers : m_SLCFramebuffers_NoDepth;

			if (m_SpotLightIndices.size())
			{
				EG_GPU_TIMING_SCOPED(cmd, "Translucent Sprites: Spot Lights Shadow pass");
				EG_CPU_TIMING_SCOPED("Translucent Sprites: Spot Lights Shadow pass");

				auto& spotLights = m_Renderer.GetSpotLights();

				auto& pipeline = bDidDrawSL ?
					bDidDrawSLC ? m_TranslucentSSLPipeline : m_TranslucentSSLPipelineClearing :
					bDidDrawSLC ? m_TranslucentSSLPipeline_NoDepth : m_TranslucentSSLPipelineClearing_NoDepth;

				const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
				const bool bTexturesDirty = texturesChangedFrame >= m_TranslucentSpritesSLTexturesUpdatedFrames[currentFrameIndex];
				if (bTexturesDirty)
				{
					m_TranslucentSSLPipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
					m_TranslucentSSLPipelineClearing->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
					m_TranslucentSSLPipeline_NoDepth->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
					m_TranslucentSSLPipelineClearing_NoDepth->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
					m_TranslucentSpritesSLTexturesUpdatedFrames[currentFrameIndex] = texturesChangedFrame + 1;
				}
				pipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
				pipeline->SetBuffer(MaterialSystem::GetMaterialsRawBuffer(), EG_PERSISTENT_SET, EG_BINDING_RAW_MATERIALS);

				pipeline->SetBuffer(transformsBuffer, 1, 0);

				uint32_t spotLightsCount = 0;
				for (const auto& index : m_SpotLightIndices)
				{
					const auto& spotLight = spotLights[index];
					const uint32_t& i = spotLightsCount;

					cmd->SetGraphicsCullMode(CullMode::Front);
					RenderSpritesTask::Draw(cmd, pipeline, singleSided.Translucent.ShadowCastingQuads, &spotLight.ViewProj, stats, framebuffers[i]);
					cmd->SetGraphicsCullMode(CullMode::None);
					RenderSpritesTask::Draw(cmd, pipeline, doubleSided.Translucent.ShadowCastingQuads, &spotLight.ViewProj, stats, framebuffers[i]);

					++spotLightsCount;
					bDidDrawSLC = true;
				}
			}
		}
	}
	
	void ShadowPassTask::ShadowPassMaskedSprites(const Ref<CommandBuffer>& cmd)
	{
		const auto& singleSided = m_Renderer.GetSingleSidedSpritesRenderData();
		const auto& doubleSided = m_Renderer.GetDoubleSidedSpritesRenderData();

		if (singleSided.Masked.IsEmpty() && doubleSided.Masked.IsEmpty())
			return;

		EG_GPU_TIMING_SCOPED(cmd, "Masked Sprites shadow pass");
		EG_CPU_TIMING_SCOPED("Masked Sprites shadow pass");

		const auto& transformsBuffer = m_Renderer.GetSpritesTransformsBuffer();
		auto& stats = m_Renderer.GetStats();

		const uint32_t currentFrameIndex = RenderManager::GetCurrentFrameIndex();

		// For directional light
		const auto& dirLight = m_Renderer.GetDirectionalLight();
		if (m_Renderer.HasDirectionalLight() && dirLight.bCastsShadows)
		{
			EG_GPU_TIMING_SCOPED(cmd, "Masked Sprites: CSM Shadow pass");
			EG_CPU_TIMING_SCOPED("Masked Sprites: CSM Shadow pass");

			CreateIfNeededDirectionalLightShadowMaps();

			auto& pipeline = bDidDrawDL ? m_MaskedSDLPipeline : m_MaskedSDLPipelineClearing;
			const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
			const bool bTexturesDirty = texturesChangedFrame >= m_MaskedSpritesDLTexturesUpdatedFrames[currentFrameIndex];
			if (bTexturesDirty)
			{
				m_MaskedSDLPipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
				m_MaskedSDLPipelineClearing->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
				m_MaskedSpritesDLTexturesUpdatedFrames[currentFrameIndex] = texturesChangedFrame + 1;
			}
			pipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
			pipeline->SetBuffer(MaterialSystem::GetMaterialsRawBuffer(), EG_PERSISTENT_SET, EG_BINDING_RAW_MATERIALS);

			pipeline->SetBuffer(transformsBuffer, 1, 0);
			for (uint32_t i = 0; i < m_DLFramebuffers.size(); ++i)
			{
				cmd->SetGraphicsCullMode(CullMode::Front);
				RenderSpritesTask::Draw(cmd, pipeline, singleSided.Masked.ShadowCastingQuads, &dirLight.ViewProj[i], stats, m_DLFramebuffers[i]);
				cmd->SetGraphicsCullMode(CullMode::None);
				RenderSpritesTask::Draw(cmd, pipeline, doubleSided.Masked.ShadowCastingQuads, &dirLight.ViewProj[i], stats, m_DLFramebuffers[i]);
			}
			bDidDrawDL = true;
		}
		else
		{
			FreeDirectionalLightShadowMaps();
		}

		// Point lights
		if (m_PointLightIndices.size())
		{
			EG_GPU_TIMING_SCOPED(cmd, "Masked Sprites: Point Lights Shadow pass");
			EG_CPU_TIMING_SCOPED("Masked Sprites: Point Lights Shadow pass");

			auto& framebuffers = m_PLFramebuffers;
			auto& vpsBuffer = m_PLVPsBuffer;
			auto& pipeline = bDidDrawPL ? m_MaskedSPLPipeline : m_MaskedSPLPipelineClearing;

			const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
			const bool bTexturesDirty = texturesChangedFrame >= m_MaskedSpritesPLTexturesUpdatedFrames[currentFrameIndex];
			if (bTexturesDirty)
			{
				m_MaskedSPLPipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
				m_MaskedSPLPipelineClearing->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
				m_MaskedSpritesPLTexturesUpdatedFrames[currentFrameIndex] = texturesChangedFrame + 1;
			}
			pipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
			pipeline->SetBuffer(MaterialSystem::GetMaterialsRawBuffer(), EG_PERSISTENT_SET, EG_BINDING_RAW_MATERIALS);

			pipeline->SetBuffer(transformsBuffer, 1, 0);
			pipeline->SetBuffer(vpsBuffer, 1, 1);

			uint32_t i = 0;
			for (auto& index : m_PointLightIndices)
			{
				cmd->SetGraphicsCullMode(CullMode::Front);
				RenderSpritesTask::Draw(cmd, pipeline, singleSided.Opaque.ShadowCastingQuads, &i, stats, framebuffers[i]);
				cmd->SetGraphicsCullMode(CullMode::None);
				RenderSpritesTask::Draw(cmd, pipeline, doubleSided.Opaque.ShadowCastingQuads, &i, stats, framebuffers[i]);
				++i;
				bDidDrawPL = true;
			}
		}

		// Spot lights
		if (m_SpotLightIndices.size())
		{
			EG_GPU_TIMING_SCOPED(cmd, "Masked Sprites: Spot Lights Shadow pass");
			EG_CPU_TIMING_SCOPED("Masked Sprites: Spot Lights Shadow pass");

			auto& spotLights = m_Renderer.GetSpotLights();
			uint32_t spotLightsCount = 0;
			auto& framebuffers = m_SLFramebuffers;
			auto& pipeline = bDidDrawSL ? m_MaskedSSLPipeline : m_MaskedSSLPipelineClearing;

			const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
			const bool bTexturesDirty = texturesChangedFrame >= m_MaskedSpritesSLTexturesUpdatedFrames[currentFrameIndex];
			if (bTexturesDirty)
			{
				m_MaskedSSLPipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
				m_MaskedSSLPipelineClearing->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
				m_MaskedSpritesSLTexturesUpdatedFrames[currentFrameIndex] = texturesChangedFrame + 1;
			}
			pipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
			pipeline->SetBuffer(MaterialSystem::GetMaterialsRawBuffer(), EG_PERSISTENT_SET, EG_BINDING_RAW_MATERIALS);

			pipeline->SetBuffer(transformsBuffer, 1, 0);

			for (auto& index : m_SpotLightIndices)
			{
				auto& spotLight = spotLights[index];

				bDidDrawSL = true;
				const uint32_t& i = spotLightsCount;

				cmd->SetGraphicsCullMode(CullMode::Front);
				RenderSpritesTask::Draw(cmd, pipeline, singleSided.Opaque.ShadowCastingQuads, &spotLight.ViewProj, stats, framebuffers[i]);
				cmd->SetGraphicsCullMode(CullMode::None);
				RenderSpritesTask::Draw(cmd, pipeline, doubleSided.Opaque.ShadowCastingQuads, &spotLight.ViewProj, stats, framebuffers[i]);

				++spotLightsCount;
			}
		}
	}

	void ShadowPassTask::ShadowPassOpaqueLitTexts(const Ref<CommandBuffer>& cmd)
	{
		const auto& singleSided = m_Renderer.GetSingleSidedTextsRenderData();
		const auto& doubleSided = m_Renderer.GetDoubleSidedTextsRenderData();
		if (singleSided.Opaque.IsEmpty() && doubleSided.Opaque.IsEmpty())
			return;

		EG_GPU_TIMING_SCOPED(cmd, "Opaque Lit Texts shadow pass");
		EG_CPU_TIMING_SCOPED("Opaque Lit Texts shadow pass");

		const auto& transformsBuffer = m_Renderer.GetTextsTransformsBuffer();
		auto& stats = m_Renderer.GetStats();

		// For directional light
		const auto& dirLight = m_Renderer.GetDirectionalLight();
		if (m_Renderer.HasDirectionalLight() && dirLight.bCastsShadows)
		{
			EG_GPU_TIMING_SCOPED(cmd, "Opaque Lit Texts: CSM Shadow pass");
			EG_CPU_TIMING_SCOPED("Opaque Lit Texts: CSM Shadow pass");

			CreateIfNeededDirectionalLightShadowMaps();

			auto& pipeline = bDidDrawDL ? m_OpaqueLitTDLPipeline : m_OpaqueLitTDLPipelineClearing;
			pipeline->SetBuffer(transformsBuffer, 0, 0);
			pipeline->SetTextureArray(m_Renderer.GetAtlases(), 1, 0);
			for (uint32_t i = 0; i < m_DLFramebuffers.size(); ++i)
			{
				cmd->SetGraphicsCullMode(CullMode::Front);
				RenderTextLitTask::Draw(cmd, pipeline, singleSided.Opaque.ShadowCastingQuads, &dirLight.ViewProj[i], stats, m_DLFramebuffers[i]);
				cmd->SetGraphicsCullMode(CullMode::None);
				RenderTextLitTask::Draw(cmd, pipeline, doubleSided.Opaque.ShadowCastingQuads, &dirLight.ViewProj[i], stats, m_DLFramebuffers[i]);
			}
			bDidDrawDL = true;
		}
		else
		{
			FreeDirectionalLightShadowMaps();
		}

		// Point lights
		if (m_PointLightIndices.size())
		{
			EG_GPU_TIMING_SCOPED(cmd, "Opaque Lit Texts: Point Lights Shadow pass");
			EG_CPU_TIMING_SCOPED("Opaque Lit Texts: Point Lights Shadow pass");

			auto& framebuffers = m_PLFramebuffers;
			auto& vpsBuffer = m_PLVPsBuffer;
			auto& pipeline = bDidDrawPL ? m_OpaqueLitTPLPipeline : m_OpaqueLitTPLPipelineClearing;
			pipeline->SetBuffer(transformsBuffer, 0, 0);
			pipeline->SetBuffer(vpsBuffer, 0, 1);
			pipeline->SetTextureArray(m_Renderer.GetAtlases(), 1, 0);

			uint32_t i = 0;
			for (const auto& index : m_PointLightIndices)
			{
				cmd->SetGraphicsCullMode(CullMode::Front);
				RenderTextLitTask::Draw(cmd, pipeline, singleSided.Opaque.ShadowCastingQuads, &i, stats, framebuffers[i]);
				cmd->SetGraphicsCullMode(CullMode::None);
				RenderTextLitTask::Draw(cmd, pipeline, doubleSided.Opaque.ShadowCastingQuads, &i, stats, framebuffers[i]);
				bDidDrawPL = true;
				++i;
			}
		}

		// Spot lights
		if (m_SpotLightIndices.size())
		{
			EG_GPU_TIMING_SCOPED(cmd, "Opaque Lit Texts: Spot Lights Shadow pass");
			EG_CPU_TIMING_SCOPED("Opaque Lit Texts: Spot Lights Shadow pass");

			auto& spotLights = m_Renderer.GetSpotLights();
			uint32_t spotLightsCount = 0;
			auto& framebuffers = m_SLFramebuffers;
			auto& pipeline = bDidDrawSL ? m_OpaqueLitTSLPipeline : m_OpaqueLitTSLPipelineClearing;
			pipeline->SetBuffer(transformsBuffer, 0, 0);
			pipeline->SetTextureArray(m_Renderer.GetAtlases(), 1, 0);

			for (const auto& index : m_SpotLightIndices)
			{
				const auto& spotLight = spotLights[index];
				const uint32_t& i = spotLightsCount;

				cmd->SetGraphicsCullMode(CullMode::Front);
				RenderTextLitTask::Draw(cmd, pipeline, singleSided.Opaque.ShadowCastingQuads, &spotLight.ViewProj, stats, framebuffers[i]);
				cmd->SetGraphicsCullMode(CullMode::None);
				RenderTextLitTask::Draw(cmd, pipeline, doubleSided.Opaque.ShadowCastingQuads, &spotLight.ViewProj, stats, framebuffers[i]);

				++spotLightsCount;
				bDidDrawSL = true;
			}
		}
	}
	
	void ShadowPassTask::ShadowPassTranslucentLitTexts(const Ref<CommandBuffer>& cmd)
	{
		const auto& singleSided = m_Renderer.GetSingleSidedTextsRenderData();
		const auto& doubleSided = m_Renderer.GetDoubleSidedTextsRenderData();
		if (singleSided.Translucent.IsEmpty() && doubleSided.Translucent.IsEmpty())
			return;

		EG_GPU_TIMING_SCOPED(cmd, "Translucent Lit Texts shadow pass");
		EG_CPU_TIMING_SCOPED("Translucent Lit Texts shadow pass");

		const auto& transformsBuffer = m_Renderer.GetTextsTransformsBuffer();
		auto& stats = m_Renderer.GetStats();

		const uint32_t currentFrameIndex = RenderManager::GetCurrentFrameIndex();

		// For directional light
		const auto& dirLight = m_Renderer.GetDirectionalLight();
		if (m_Renderer.HasDirectionalLight() && dirLight.bCastsShadows)
		{
			EG_GPU_TIMING_SCOPED(cmd, "Translucent Lit Texts: CSM Shadow pass");
			EG_CPU_TIMING_SCOPED("Translucent Lit Texts: CSM Shadow pass");

			auto& pipeline = bDidDrawDL ?
				bDidDrawDLC ? m_TranslucentLitTDLPipeline : m_TranslucentLitTDLPipelineClearing :
				bDidDrawDLC ? m_TranslucentLitTDLPipeline_NoDepth : m_TranslucentLitTDLPipelineClearing_NoDepth;
			auto& framebuffers = bDidDrawDL ? m_DLCFramebuffers : m_DLCFramebuffers_NoDepth;

			CreateIfNeededColoredDirectionalLightShadowMaps();
			if (bDidDrawDL)
				m_DLCFramebuffers_NoDepth.clear();
			else
				m_DLCFramebuffers.clear();

			if (framebuffers.empty())
				InitColoredDirectionalLightFramebuffers(framebuffers, pipeline, bDidDrawDL);

			const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
			const bool bTexturesDirty = texturesChangedFrame >= m_TranslucentLitTextsDLTexturesUpdatedFrames[currentFrameIndex];
			if (bTexturesDirty)
			{
				m_TranslucentLitTDLPipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
				m_TranslucentLitTDLPipelineClearing->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
				m_TranslucentLitTDLPipeline_NoDepth->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
				m_TranslucentLitTDLPipelineClearing_NoDepth->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
				m_TranslucentLitTextsDLTexturesUpdatedFrames[currentFrameIndex] = texturesChangedFrame + 1;
			}
			pipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
			pipeline->SetBuffer(MaterialSystem::GetMaterialsRawBuffer(), EG_PERSISTENT_SET, EG_BINDING_RAW_MATERIALS);
			pipeline->SetBuffer(transformsBuffer, 1, 0);
			pipeline->SetTextureArray(m_Renderer.GetAtlases(), 3, 0);

			for (uint32_t i = 0; i < framebuffers.size(); ++i)
			{
				cmd->SetGraphicsCullMode(CullMode::Front);
				RenderTextLitTask::Draw(cmd, pipeline, singleSided.Translucent.ShadowCastingQuads, &dirLight.ViewProj[i], stats, framebuffers[i]);
				cmd->SetGraphicsCullMode(CullMode::None);
				RenderTextLitTask::Draw(cmd, pipeline, doubleSided.Translucent.ShadowCastingQuads, &dirLight.ViewProj[i], stats, framebuffers[i]);
			}
			bDidDrawDLC = true;
		}
		else
		{
			FreeColoredDirectionalLightShadowMaps();
		}

		// Point lights
		{
			const auto& framebuffers = bDidDrawPL ? m_PLCFramebuffers : m_PLCFramebuffers_NoDepth;

			if (m_PointLightIndices.size())
			{
				EG_GPU_TIMING_SCOPED(cmd, "Translucent Lit Texts: Point Lights Shadow pass");
				EG_CPU_TIMING_SCOPED("Translucent Lit Texts: Point Lights Shadow pass");

				auto& vpsBuffer = m_PLVPsBuffer;
				auto& pipeline = bDidDrawPL ?
					bDidDrawPLC ? m_TranslucentLitTPLPipeline : m_TranslucentLitTPLPipelineClearing :
					bDidDrawPLC ? m_TranslucentLitTPLPipeline_NoDepth : m_TranslucentLitTPLPipelineClearing_NoDepth;

				const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
				const bool bTexturesDirty = texturesChangedFrame >= m_TranslucentLitTextsPLTexturesUpdatedFrames[currentFrameIndex];
				if (bTexturesDirty)
				{
					m_TranslucentLitTPLPipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
					m_TranslucentLitTPLPipelineClearing->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
					m_TranslucentLitTPLPipeline_NoDepth->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
					m_TranslucentLitTPLPipelineClearing_NoDepth->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
					m_TranslucentLitTextsPLTexturesUpdatedFrames[currentFrameIndex] = texturesChangedFrame + 1;
				}
				pipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
				pipeline->SetBuffer(MaterialSystem::GetMaterialsRawBuffer(), EG_PERSISTENT_SET, EG_BINDING_RAW_MATERIALS);
				pipeline->SetBuffer(transformsBuffer, 1, 0);
				pipeline->SetBuffer(vpsBuffer, 1, 1);
				pipeline->SetTextureArray(m_Renderer.GetAtlases(), 3, 0);

				uint32_t pointLightsCount = 0;
				for (const auto& index : m_PointLightIndices)
				{
					const uint32_t& i = pointLightsCount;

					cmd->SetGraphicsCullMode(CullMode::Front);
					RenderTextLitTask::Draw(cmd, pipeline, singleSided.Translucent.ShadowCastingQuads, &i, stats, framebuffers[i]);
					cmd->SetGraphicsCullMode(CullMode::None);
					RenderTextLitTask::Draw(cmd, pipeline, doubleSided.Translucent.ShadowCastingQuads, &i, stats, framebuffers[i]);

					++pointLightsCount;
					bDidDrawPLC = true;
				}
			}
		}

		// Spot lights
		{
			const auto& framebuffers = bDidDrawSL ? m_SLCFramebuffers : m_SLCFramebuffers_NoDepth;

			if (m_SpotLightIndices.size())
			{
				EG_GPU_TIMING_SCOPED(cmd, "Translucent Lit Texts: Spot Lights Shadow pass");
				EG_CPU_TIMING_SCOPED("Translucent Lit Texts: Spot Lights Shadow pass");

				auto& spotLights = m_Renderer.GetSpotLights();

				auto& pipeline = bDidDrawSL ?
					bDidDrawSLC ? m_TranslucentLitTSLPipeline : m_TranslucentLitTSLPipelineClearing :
					bDidDrawSLC ? m_TranslucentLitTSLPipeline_NoDepth : m_TranslucentLitTSLPipelineClearing_NoDepth;

				const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
				const bool bTexturesDirty = texturesChangedFrame >= m_TranslucentLitTextsSLTexturesUpdatedFrames[currentFrameIndex];
				if (bTexturesDirty)
				{
					m_TranslucentLitTSLPipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
					m_TranslucentLitTSLPipelineClearing->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
					m_TranslucentLitTSLPipeline_NoDepth->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
					m_TranslucentLitTSLPipelineClearing_NoDepth->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
					m_TranslucentLitTextsSLTexturesUpdatedFrames[currentFrameIndex] = texturesChangedFrame + 1;
				}
				pipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
				pipeline->SetBuffer(MaterialSystem::GetMaterialsRawBuffer(), EG_PERSISTENT_SET, EG_BINDING_RAW_MATERIALS);
				pipeline->SetBuffer(transformsBuffer, 1, 0);
				pipeline->SetTextureArray(m_Renderer.GetAtlases(), 3, 0);

				uint32_t spotLightsCount = 0;
				for (const auto& index : m_SpotLightIndices)
				{
					const auto& spotLight = spotLights[index];
					const uint32_t& i = spotLightsCount;

					cmd->SetGraphicsCullMode(CullMode::Front);
					RenderTextLitTask::Draw(cmd, pipeline, singleSided.Translucent.ShadowCastingQuads, &spotLight.ViewProj, stats, framebuffers[i]);
					cmd->SetGraphicsCullMode(CullMode::None);
					RenderTextLitTask::Draw(cmd, pipeline, doubleSided.Translucent.ShadowCastingQuads, &spotLight.ViewProj, stats, framebuffers[i]);

					++spotLightsCount;
					bDidDrawSLC = true;
				}
			}
		}
	}
	
	void ShadowPassTask::ShadowPassMaskedLitTexts(const Ref<CommandBuffer>& cmd)
	{
		const auto& singleSided = m_Renderer.GetSingleSidedTextsRenderData();
		const auto& doubleSided = m_Renderer.GetDoubleSidedTextsRenderData();
		if (singleSided.Masked.IsEmpty() && doubleSided.Masked.IsEmpty())
			return;

		EG_GPU_TIMING_SCOPED(cmd, "Masked Lit Texts shadow pass");
		EG_CPU_TIMING_SCOPED("Masked Lit Texts shadow pass");

		const auto& transformsBuffer = m_Renderer.GetTextsTransformsBuffer();
		auto& stats = m_Renderer.GetStats();

		const uint32_t currentFrameIndex = RenderManager::GetCurrentFrameIndex();

		// For directional light
		const auto& dirLight = m_Renderer.GetDirectionalLight();
		if (m_Renderer.HasDirectionalLight() && dirLight.bCastsShadows)
		{
			EG_GPU_TIMING_SCOPED(cmd, "Masked Lit Texts: CSM Shadow pass");
			EG_CPU_TIMING_SCOPED("Masked Lit Texts: CSM Shadow pass");

			CreateIfNeededDirectionalLightShadowMaps();

			auto& pipeline = bDidDrawDL ? m_MaskedLitTDLPipeline : m_MaskedLitTDLPipelineClearing;
			const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
			const bool bTexturesDirty = texturesChangedFrame >= m_MaskedLitTextsDLTexturesUpdatedFrames[currentFrameIndex];
			if (bTexturesDirty)
			{
				m_MaskedLitTDLPipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
				m_MaskedLitTDLPipelineClearing->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
				m_MaskedLitTextsDLTexturesUpdatedFrames[currentFrameIndex] = texturesChangedFrame + 1;
			}
			pipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
			pipeline->SetBuffer(MaterialSystem::GetMaterialsRawBuffer(), EG_PERSISTENT_SET, EG_BINDING_RAW_MATERIALS);
			pipeline->SetBuffer(transformsBuffer, 1, 0);
			pipeline->SetTextureArray(m_Renderer.GetAtlases(), 3, 0);

			for (uint32_t i = 0; i < m_DLFramebuffers.size(); ++i)
			{
				cmd->SetGraphicsCullMode(CullMode::Front);
				RenderTextLitTask::Draw(cmd, pipeline, singleSided.Masked.ShadowCastingQuads, &dirLight.ViewProj[i], stats, m_DLFramebuffers[i]);
				cmd->SetGraphicsCullMode(CullMode::None);
				RenderTextLitTask::Draw(cmd, pipeline, doubleSided.Masked.ShadowCastingQuads, &dirLight.ViewProj[i], stats, m_DLFramebuffers[i]);
			}
			bDidDrawDL = true;
		}
		else
		{
			FreeDirectionalLightShadowMaps();
		}

		// Point lights
		if (m_PointLightIndices.size())
		{
			EG_GPU_TIMING_SCOPED(cmd, "Masked Lit Texts: Point Lights Shadow pass");
			EG_CPU_TIMING_SCOPED("Masked Lit Texts: Point Lights Shadow pass");

			auto& framebuffers = m_PLFramebuffers;
			auto& vpsBuffer = m_PLVPsBuffer;
			auto& pipeline = bDidDrawPL ? m_MaskedLitTPLPipeline : m_MaskedLitTPLPipelineClearing;

			const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
			const bool bTexturesDirty = texturesChangedFrame >= m_MaskedLitTextsPLTexturesUpdatedFrames[currentFrameIndex];
			if (bTexturesDirty)
			{
				m_MaskedLitTPLPipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
				m_MaskedLitTPLPipelineClearing->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
				m_MaskedLitTextsPLTexturesUpdatedFrames[currentFrameIndex] = texturesChangedFrame + 1;
			}
			pipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
			pipeline->SetBuffer(MaterialSystem::GetMaterialsRawBuffer(), EG_PERSISTENT_SET, EG_BINDING_RAW_MATERIALS);
			pipeline->SetBuffer(transformsBuffer, 1, 0);
			pipeline->SetBuffer(vpsBuffer, 1, 1);
			pipeline->SetTextureArray(m_Renderer.GetAtlases(), 3, 0);

			uint32_t i = 0;
			for (const auto& index : m_PointLightIndices)
			{
				cmd->SetGraphicsCullMode(CullMode::Front);
				RenderTextLitTask::Draw(cmd, pipeline, singleSided.Masked.ShadowCastingQuads, &i, stats, framebuffers[i]);
				cmd->SetGraphicsCullMode(CullMode::None);
				RenderTextLitTask::Draw(cmd, pipeline, doubleSided.Masked.ShadowCastingQuads, &i, stats, framebuffers[i]);

				++i;
				bDidDrawPL = true;
			}
		}

		// Spot lights
		if (m_SpotLightIndices.size())
		{
			EG_GPU_TIMING_SCOPED(cmd, "Masked Lit Texts: Spot Lights Shadow pass");
			EG_CPU_TIMING_SCOPED("Masked Lit Texts: Spot Lights Shadow pass");

			auto& spotLights = m_Renderer.GetSpotLights();
			uint32_t spotLightsCount = 0;
			auto& framebuffers = m_SLFramebuffers;
			auto& pipeline = bDidDrawSL ? m_MaskedLitTSLPipeline : m_MaskedLitTSLPipelineClearing;

			const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
			const bool bTexturesDirty = texturesChangedFrame >= m_MaskedLitTextsSLTexturesUpdatedFrames[currentFrameIndex];
			if (bTexturesDirty)
			{
				m_MaskedLitTSLPipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
				m_MaskedLitTSLPipelineClearing->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
				m_MaskedLitTextsSLTexturesUpdatedFrames[currentFrameIndex] = texturesChangedFrame + 1;
			}
			pipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
			pipeline->SetBuffer(MaterialSystem::GetMaterialsRawBuffer(), EG_PERSISTENT_SET, EG_BINDING_RAW_MATERIALS);
			pipeline->SetBuffer(transformsBuffer, 1, 0);
			pipeline->SetTextureArray(m_Renderer.GetAtlases(), 3, 0);

			for (const auto& index : m_SpotLightIndices)
			{
				const auto& spotLight = spotLights[index];
				const uint32_t& i = spotLightsCount;

				cmd->SetGraphicsCullMode(CullMode::Front);
				RenderTextLitTask::Draw(cmd, pipeline, singleSided.Masked.ShadowCastingQuads, &spotLight.ViewProj, stats, framebuffers[i]);
				cmd->SetGraphicsCullMode(CullMode::None);
				RenderTextLitTask::Draw(cmd, pipeline, doubleSided.Masked.ShadowCastingQuads, &spotLight.ViewProj, stats, framebuffers[i]);

				++spotLightsCount;
				bDidDrawSL = true;
			}
		}
	}

	void ShadowPassTask::ShadowPassUnlitTexts(const Ref<CommandBuffer>& cmd)
	{
		const auto& singleSided = m_Renderer.GetSingleSidedUnlitTextsRenderData();
		const auto& doubleSided = m_Renderer.GetDoubleSidedUnlitTextsRenderData();

		if (singleSided.Opaque.IsEmpty() && doubleSided.Opaque.IsEmpty())
			return;

		EG_GPU_TIMING_SCOPED(cmd, "Unlit Texts shadow pass");
		EG_CPU_TIMING_SCOPED("Unlit Texts shadow pass");

		const auto& transformsBuffer = m_Renderer.GetTextsTransformsBuffer();
		auto& stats = m_Renderer.GetStats();

		// For directional light
		const auto& dirLight = m_Renderer.GetDirectionalLight();
		if (m_Renderer.HasDirectionalLight() && dirLight.bCastsShadows)
		{
			EG_GPU_TIMING_SCOPED(cmd, "Unlit Texts: CSM Shadow pass");
			EG_CPU_TIMING_SCOPED("Unlit Texts: CSM Shadow pass");

			CreateIfNeededDirectionalLightShadowMaps();

			auto& pipeline = bDidDrawDL ? m_UnlitTDLPipeline : m_UnlitTDLPipelineClearing;
			pipeline->SetBuffer(transformsBuffer, 0, 0);
			pipeline->SetTextureArray(m_Renderer.GetAtlases(), 1, 0);
			for (uint32_t i = 0; i < m_DLFramebuffers.size(); ++i)
			{
				cmd->SetGraphicsCullMode(CullMode::Front);
				RenderTextUnlitTask::Draw(cmd, pipeline, singleSided.Opaque.ShadowCastingQuads, &dirLight.ViewProj[i], stats, m_DLFramebuffers[i]);
				cmd->SetGraphicsCullMode(CullMode::None);
				RenderTextUnlitTask::Draw(cmd, pipeline, doubleSided.Opaque.ShadowCastingQuads, &dirLight.ViewProj[i], stats, m_DLFramebuffers[i]);
			}
			bDidDrawDL = true;
		}
		else
		{
			FreeDirectionalLightShadowMaps();
		}

		// Point lights
		if (m_PointLightIndices.size())
		{
			EG_GPU_TIMING_SCOPED(cmd, "Unlit Texts: Point Lights Shadow pass");
			EG_CPU_TIMING_SCOPED("Unlit Texts: Point Lights Shadow pass");

			auto& framebuffers = m_PLFramebuffers;
			auto& vpsBuffer = m_PLVPsBuffer;
			auto& pipeline = bDidDrawPL ? m_UnlitTPLPipeline : m_UnlitTPLPipelineClearing;
			pipeline->SetBuffer(transformsBuffer, 0, 0);
			pipeline->SetBuffer(vpsBuffer, 0, 1);
			pipeline->SetTextureArray(m_Renderer.GetAtlases(), 1, 0);

			uint32_t i = 0;
			for (const auto& index : m_PointLightIndices)
			{
				cmd->SetGraphicsCullMode(CullMode::Front);
				RenderTextUnlitTask::Draw(cmd, pipeline, singleSided.Opaque.ShadowCastingQuads, &i, stats, framebuffers[i]);
				cmd->SetGraphicsCullMode(CullMode::None);
				RenderTextUnlitTask::Draw(cmd, pipeline, doubleSided.Opaque.ShadowCastingQuads, &i, stats, framebuffers[i]);

				++i;
				bDidDrawPL = true;
			}
		}

		// Spot lights
		if (m_SpotLightIndices.size())
		{
			EG_GPU_TIMING_SCOPED(cmd, "Unlit Texts: Spot Lights Shadow pass");
			EG_CPU_TIMING_SCOPED("Unlit Texts: Spot Lights Shadow pass");

			auto& spotLights = m_Renderer.GetSpotLights();
			uint32_t spotLightsCount = 0;
			auto& framebuffers = m_SLFramebuffers;
			auto& pipeline = bDidDrawSL ? m_UnlitTSLPipeline : m_UnlitTSLPipelineClearing;
			pipeline->SetBuffer(transformsBuffer, 0, 0);
			pipeline->SetTextureArray(m_Renderer.GetAtlases(), 1, 0);

			for (auto& index : m_SpotLightIndices)
			{
				auto& spotLight = spotLights[index];
				const uint32_t& i = spotLightsCount;

				cmd->SetGraphicsCullMode(CullMode::Front);
				RenderTextUnlitTask::Draw(cmd, pipeline, singleSided.Opaque.ShadowCastingQuads, &spotLight.ViewProj, stats, framebuffers[i]);
				cmd->SetGraphicsCullMode(CullMode::None);
				RenderTextUnlitTask::Draw(cmd, pipeline, doubleSided.Opaque.ShadowCastingQuads, &spotLight.ViewProj, stats, framebuffers[i]);

				++spotLightsCount;
				bDidDrawSL = true;
			}
		}
	}

	void ShadowPassTask::InitOpacityMaskedMeshPipelines()
	{
		Ref<Sampler> shadowMapSampler = Sampler::Create(FilterMode::Point, AddressMode::ClampToOpaqueWhite, CompareOperation::Never, 0.f, 0.f, 1.f);

		// For directional light
		{
			for (uint32_t i = 0; i < m_DLShadowMapSamplers.size(); ++i)
				m_DLShadowMapSamplers[i] = shadowMapSampler;

			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageLayoutType::Unknown;
			depthAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.Image = RenderManager::GetDummyDepthImage();
			depthAttachment.bWriteDepth = true;
			depthAttachment.ClearOperation = ClearOperation::Clear;
			depthAttachment.DepthClearValue = 0.f;
			depthAttachment.DepthCompareOp = CompareOperation::Greater;

			PipelineGraphicsState state;
			state.VertexShader = Shader::Create("shadow_maps/shadow_map_meshes.vert", ShaderType::Vertex);
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::Dynamic;
			state.PerInstanceAttribs = RenderMeshesTask::PerInstanceAttribs;

			m_OpacityMDLPipelineClearing = PipelineGraphics::Create(state);
			state.DepthStencilAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
			state.DepthStencilAttachment.ClearOperation = ClearOperation::Load;
			m_OpacityMDLPipeline = PipelineGraphics::Create(state);

			const ShaderDefines defines = { {"EG_MATERIALS_REQUIRED", ""} };
			state.VertexShader = Shader::Create("shadow_maps/shadow_map_meshes.vert", ShaderType::Vertex, defines);
			state.FragmentShader = Shader::Create("shadow_maps/shadow_map_masked.frag", ShaderType::Fragment);
			state.DepthStencilAttachment.InitialLayout = ImageLayoutType::Unknown;
			state.DepthStencilAttachment.ClearOperation = ClearOperation::Clear;
			m_MaskedMDLPipelineClearing = PipelineGraphics::Create(state);

			state.DepthStencilAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
			state.DepthStencilAttachment.ClearOperation = ClearOperation::Load;
			m_MaskedMDLPipeline = PipelineGraphics::Create(state);
		}

		// For point lights
		{
			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageLayoutType::Unknown;
			depthAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.Image = RenderManager::GetDummyDepthCubeImage();
			depthAttachment.ClearOperation = ClearOperation::Clear;
			depthAttachment.DepthClearValue = 0.f;
			depthAttachment.DepthCompareOp = CompareOperation::Greater;

			ShaderDefines defines;
			defines["EG_POINT_LIGHT_PASS"] = "";

			PipelineGraphicsState state;
			state.VertexShader = Shader::Create("shadow_maps/shadow_map_meshes.vert", ShaderType::Vertex, defines);
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::Dynamic;
			state.bEnableMultiViewRendering = true;
			state.MultiViewPasses = 6;
			state.PerInstanceAttribs = RenderMeshesTask::PerInstanceAttribs;

			m_OpacityMPLPipelineClearing = PipelineGraphics::Create(state);
			state.DepthStencilAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
			state.DepthStencilAttachment.ClearOperation = ClearOperation::Load;
			m_OpacityMPLPipeline = PipelineGraphics::Create(state);

			defines["EG_MATERIALS_REQUIRED"] = "";
			state.VertexShader = Shader::Create("shadow_maps/shadow_map_meshes.vert", ShaderType::Vertex, defines);
			state.FragmentShader = Shader::Create("shadow_maps/shadow_map_masked.frag", ShaderType::Fragment);
			state.DepthStencilAttachment.InitialLayout = ImageLayoutType::Unknown;
			state.DepthStencilAttachment.ClearOperation = ClearOperation::Clear;
			m_MaskedMPLPipelineClearing = PipelineGraphics::Create(state);

			state.DepthStencilAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
			state.DepthStencilAttachment.ClearOperation = ClearOperation::Load;
			m_MaskedMPLPipeline = PipelineGraphics::Create(state);

			std::fill(m_PLShadowMapSamplers.begin(), m_PLShadowMapSamplers.end(), shadowMapSampler);
		}

		// For Spot lights
		{
			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageLayoutType::Unknown;
			depthAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.Image = RenderManager::GetDummyDepthImage();
			depthAttachment.ClearOperation = ClearOperation::Clear;
			depthAttachment.DepthClearValue = 0.f;
			depthAttachment.DepthCompareOp = CompareOperation::Greater;

			ShaderDefines defines;
			defines["EG_SPOT_LIGHT_PASS"] = "";

			PipelineGraphicsState state;
			state.VertexShader = Shader::Create("shadow_maps/shadow_map_meshes.vert", ShaderType::Vertex, defines);
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::Dynamic;
			state.PerInstanceAttribs = RenderMeshesTask::PerInstanceAttribs;

			m_OpacityMSLPipelineClearing = PipelineGraphics::Create(state);
			state.DepthStencilAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
			state.DepthStencilAttachment.ClearOperation = ClearOperation::Load;
			m_OpacityMSLPipeline = PipelineGraphics::Create(state);

			defines["EG_MATERIALS_REQUIRED"] = "";
			state.VertexShader = Shader::Create("shadow_maps/shadow_map_meshes.vert", ShaderType::Vertex, defines);
			state.FragmentShader = Shader::Create("shadow_maps/shadow_map_masked.frag", ShaderType::Fragment);
			state.DepthStencilAttachment.InitialLayout = ImageLayoutType::Unknown;
			state.DepthStencilAttachment.ClearOperation = ClearOperation::Clear;
			m_MaskedMSLPipelineClearing = PipelineGraphics::Create(state);

			state.DepthStencilAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
			state.DepthStencilAttachment.ClearOperation = ClearOperation::Load;
			m_MaskedMSLPipeline = PipelineGraphics::Create(state);
		}
	}

	void ShadowPassTask::InitTranslucentMeshPipelines()
	{
		ShaderDefines fragmentDefines;
		if (bVolumetricLightsEnabled)
			fragmentDefines["EG_OUTPUT_DEPTH"] = "";

		// For directional light
		{
			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.Image = RenderManager::GetDummyDepthImage();
			depthAttachment.bWriteDepth = false;
			depthAttachment.ClearOperation = ClearOperation::Load;
			depthAttachment.DepthCompareOp = CompareOperation::Greater;

			const ShaderDefines vertexDefines = { {"EG_MATERIALS_REQUIRED", ""} };

			PipelineGraphicsState state;
			state.CullMode = CullMode::Dynamic;
			state.PerInstanceAttribs = RenderMeshesTask::PerInstanceAttribs;
			state.VertexShader = Shader::Create("shadow_maps/shadow_map_meshes.vert", ShaderType::Vertex, vertexDefines);

			ColorAttachment colorAttachment;
			colorAttachment.InitialLayout = ImageLayoutType::Unknown;
			colorAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
			colorAttachment.Image = RenderManager::GetDummyImage();
			colorAttachment.ClearOperation = ClearOperation::Clear;
			colorAttachment.ClearColor = glm::vec4(1.f);
			colorAttachment.bBlendEnabled = true;
			colorAttachment.BlendingState.BlendSrc = BlendFactor::Zero;
			colorAttachment.BlendingState.BlendDst = BlendFactor::SrcColor;
			colorAttachment.BlendingState.BlendOp = BlendOperation::Add;

			ColorAttachment depthColorAttachment;
			depthColorAttachment.InitialLayout = ImageLayoutType::Unknown;
			depthColorAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
			depthColorAttachment.Image = RenderManager::GetDummyImageR16();
			depthColorAttachment.ClearOperation = ClearOperation::Clear;
			depthColorAttachment.ClearColor = glm::vec4(0.f);
			depthColorAttachment.bBlendEnabled = true;
			depthColorAttachment.BlendingState.BlendSrc = BlendFactor::One;
			depthColorAttachment.BlendingState.BlendDst = BlendFactor::One;
			depthColorAttachment.BlendingState.BlendOp = BlendOperation::Max;

			state.DepthStencilAttachment = depthAttachment;
			state.ColorAttachments.push_back(colorAttachment);
			if (bVolumetricLightsEnabled)
				state.ColorAttachments.push_back(depthColorAttachment);
			state.FragmentShader = Shader::Create("shadow_maps/shadow_map_translucent.frag", ShaderType::Fragment, fragmentDefines);
			
			if (m_TranslucentMDLPipeline)
				m_TranslucentMDLPipeline->SetState(state);
			else
				m_TranslucentMDLPipeline = PipelineGraphics::Create(state);
			
			state.DepthStencilAttachment = DepthStencilAttachment{};
			if (m_TranslucentMDLPipeline_NoDepth)
				m_TranslucentMDLPipeline_NoDepth->SetState(state);
			else
				m_TranslucentMDLPipeline_NoDepth = PipelineGraphics::Create(state);
		}
	
		// For point lights
		{
			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.Image = RenderManager::GetDummyDepthCubeImage();
			depthAttachment.ClearOperation = ClearOperation::Load;
			depthAttachment.bWriteDepth = false;
			depthAttachment.DepthCompareOp = CompareOperation::Greater;

			ShaderDefines defines;
			defines["EG_POINT_LIGHT_PASS"] = "";
			defines["EG_MATERIALS_REQUIRED"] = "";

			PipelineGraphicsState state;
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::Dynamic;
			state.bEnableMultiViewRendering = true;
			state.MultiViewPasses = 6;
			state.PerInstanceAttribs = RenderMeshesTask::PerInstanceAttribs;

			state.VertexShader = Shader::Create("shadow_maps/shadow_map_meshes.vert", ShaderType::Vertex, defines);
			state.FragmentShader = Shader::Create("shadow_maps/shadow_map_translucent.frag", ShaderType::Fragment, fragmentDefines);

			ColorAttachment colorAttachment;
			colorAttachment.InitialLayout = ImageLayoutType::Unknown;
			colorAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
			colorAttachment.Image = RenderManager::GetDummyImageCube();
			colorAttachment.ClearOperation = ClearOperation::Clear;
			colorAttachment.ClearColor = glm::vec4(1.f);
			colorAttachment.bBlendEnabled = true;
			colorAttachment.BlendingState.BlendSrc = BlendFactor::Zero;
			colorAttachment.BlendingState.BlendDst = BlendFactor::SrcColor;
			colorAttachment.BlendingState.BlendOp = BlendOperation::Add;

			ColorAttachment depthColorAttachment;
			depthColorAttachment.InitialLayout = ImageLayoutType::Unknown;
			depthColorAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
			depthColorAttachment.Image = RenderManager::GetDummyImageR16Cube();
			depthColorAttachment.ClearOperation = ClearOperation::Clear;
			depthColorAttachment.ClearColor = glm::vec4(0.f);
			depthColorAttachment.bBlendEnabled = true;
			depthColorAttachment.BlendingState.BlendSrc = BlendFactor::One;
			depthColorAttachment.BlendingState.BlendDst = BlendFactor::One;
			depthColorAttachment.BlendingState.BlendOp = BlendOperation::Max;

			state.ColorAttachments.push_back(colorAttachment);
			if (bVolumetricLightsEnabled)
				state.ColorAttachments.push_back(depthColorAttachment);

			if (m_TranslucentMPLPipeline)
				m_TranslucentMPLPipeline->SetState(state);
			else
				m_TranslucentMPLPipeline = PipelineGraphics::Create(state);

			state.DepthStencilAttachment = DepthStencilAttachment{};
			if (m_TranslucentMPLPipeline_NoDepth)
				m_TranslucentMPLPipeline_NoDepth->SetState(state);
			else
				m_TranslucentMPLPipeline_NoDepth = PipelineGraphics::Create(state);
		}
	
		// For Spot lights
		{
			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.Image = RenderManager::GetDummyDepthImage();
			depthAttachment.ClearOperation = ClearOperation::Load;
			depthAttachment.bWriteDepth = false;
			depthAttachment.DepthCompareOp = CompareOperation::Greater;

			ShaderDefines defines;
			defines["EG_SPOT_LIGHT_PASS"] = "";
			defines["EG_MATERIALS_REQUIRED"] = "";

			PipelineGraphicsState state;
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::Dynamic;
			state.PerInstanceAttribs = RenderMeshesTask::PerInstanceAttribs;

			state.VertexShader = Shader::Create("shadow_maps/shadow_map_meshes.vert", ShaderType::Vertex, defines);
			state.FragmentShader = Shader::Create("shadow_maps/shadow_map_translucent.frag", ShaderType::Fragment, fragmentDefines);

			ColorAttachment colorAttachment;
			colorAttachment.InitialLayout = ImageLayoutType::Unknown;
			colorAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
			colorAttachment.Image = RenderManager::GetDummyImage();
			colorAttachment.ClearOperation = ClearOperation::Clear;
			colorAttachment.ClearColor = glm::vec4(1.f, 1.f, 1.f, 0.f);
			colorAttachment.bBlendEnabled = true;
			colorAttachment.BlendingState.BlendSrc = BlendFactor::Zero;
			colorAttachment.BlendingState.BlendDst = BlendFactor::SrcColor;
			colorAttachment.BlendingState.BlendOp = BlendOperation::Add;

			ColorAttachment depthColorAttachment;
			depthColorAttachment.InitialLayout = ImageLayoutType::Unknown;
			depthColorAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
			depthColorAttachment.Image = RenderManager::GetDummyImageR16();
			depthColorAttachment.ClearOperation = ClearOperation::Clear;
			depthColorAttachment.ClearColor = glm::vec4(0.f);
			depthColorAttachment.bBlendEnabled = true;
			depthColorAttachment.BlendingState.BlendSrc = BlendFactor::One;
			depthColorAttachment.BlendingState.BlendDst = BlendFactor::One;
			depthColorAttachment.BlendingState.BlendOp = BlendOperation::Max;

			state.ColorAttachments.push_back(colorAttachment);
			if (bVolumetricLightsEnabled)
				state.ColorAttachments.push_back(depthColorAttachment);

			if (m_TranslucentMSLPipeline)
				m_TranslucentMSLPipeline->SetState(state);
			else
				m_TranslucentMSLPipeline = PipelineGraphics::Create(state);

			state.DepthStencilAttachment = {};
			if (m_TranslucentMSLPipeline_NoDepth)
				m_TranslucentMSLPipeline_NoDepth->SetState(state);
			else
				m_TranslucentMSLPipeline_NoDepth = PipelineGraphics::Create(state);
		}
	}
	
	void ShadowPassTask::InitOpacitySkeletalMeshPipelines()
	{
		// For directional light
		{
			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.Image = RenderManager::GetDummyDepthImage();
			depthAttachment.ClearOperation = ClearOperation::Load;
			depthAttachment.DepthClearValue = 0.f;
			depthAttachment.DepthCompareOp = CompareOperation::Greater;

			PipelineGraphicsState state;
			state.VertexShader = Shader::Create("shadow_maps/shadow_map_skeletal_meshes.vert", ShaderType::Vertex);
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::Dynamic;
			state.PerInstanceAttribs = RenderSkeletalMeshesTask::PerInstanceAttribs;

			m_OpacitySMDLPipeline = PipelineGraphics::Create(state);

			state.DepthStencilAttachment.InitialLayout = ImageLayoutType::Unknown;
			state.DepthStencilAttachment.ClearOperation = ClearOperation::Clear;
			m_OpacitySMDLPipelineClearing = PipelineGraphics::Create(state);
		}

		// For point lights
		{
			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.Image = RenderManager::GetDummyDepthCubeImage();
			depthAttachment.ClearOperation = ClearOperation::Load;
			depthAttachment.DepthClearValue = 0.f;
			depthAttachment.DepthCompareOp = CompareOperation::Greater;

			ShaderDefines defines;
			defines["EG_POINT_LIGHT_PASS"] = "";

			PipelineGraphicsState state;
			state.VertexShader = Shader::Create("shadow_maps/shadow_map_skeletal_meshes.vert", ShaderType::Vertex, defines);
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::Dynamic;
			state.bEnableMultiViewRendering = true;
			state.MultiViewPasses = 6;
			state.PerInstanceAttribs = RenderSkeletalMeshesTask::PerInstanceAttribs;

			m_OpacitySMPLPipeline = PipelineGraphics::Create(state);

			state.DepthStencilAttachment.InitialLayout = ImageLayoutType::Unknown;
			state.DepthStencilAttachment.ClearOperation = ClearOperation::Clear;
			m_OpacitySMPLPipelineClearing = PipelineGraphics::Create(state);
		}

		// For Spot lights
		{
			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.Image = RenderManager::GetDummyDepthImage();
			depthAttachment.ClearOperation = ClearOperation::Load;
			depthAttachment.DepthClearValue = 0.f;
			depthAttachment.DepthCompareOp = CompareOperation::Greater;

			ShaderDefines defines;
			defines["EG_SPOT_LIGHT_PASS"] = "";

			PipelineGraphicsState state;
			state.VertexShader = Shader::Create("shadow_maps/shadow_map_skeletal_meshes.vert", ShaderType::Vertex, defines);
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::Dynamic;
			state.PerInstanceAttribs = RenderSkeletalMeshesTask::PerInstanceAttribs;

			m_OpacitySMSLPipeline = PipelineGraphics::Create(state);

			state.DepthStencilAttachment.InitialLayout = ImageLayoutType::Unknown;
			state.DepthStencilAttachment.ClearOperation = ClearOperation::Clear;
			m_OpacitySMSLPipelineClearing = PipelineGraphics::Create(state);
		}
	}

	void ShadowPassTask::InitMaskedSkeletalMeshPipelines()
	{
		// For directional light
		{
			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.Image = RenderManager::GetDummyDepthImage();
			depthAttachment.ClearOperation = ClearOperation::Load;
			depthAttachment.DepthClearValue = 0.f;
			depthAttachment.DepthCompareOp = CompareOperation::Greater;

			const ShaderDefines defines = { {"EG_MATERIALS_REQUIRED", ""} };
			PipelineGraphicsState state;
			state.VertexShader = Shader::Create("shadow_maps/shadow_map_skeletal_meshes.vert", ShaderType::Vertex, defines);
			state.FragmentShader = Shader::Create("shadow_maps/shadow_map_masked.frag", ShaderType::Fragment);
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::Dynamic;
			state.PerInstanceAttribs = RenderSkeletalMeshesTask::PerInstanceAttribs;

			m_MaskedSMDLPipeline = PipelineGraphics::Create(state);

			state.DepthStencilAttachment.InitialLayout = ImageLayoutType::Unknown;
			state.DepthStencilAttachment.ClearOperation = ClearOperation::Clear;
			m_MaskedSMDLPipelineClearing = PipelineGraphics::Create(state);
		}

		// For point lights
		{
			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.Image = RenderManager::GetDummyDepthCubeImage();
			depthAttachment.ClearOperation = ClearOperation::Load;
			depthAttachment.DepthClearValue = 0.f;
			depthAttachment.DepthCompareOp = CompareOperation::Greater;

			ShaderDefines defines;
			defines["EG_POINT_LIGHT_PASS"] = "";
			defines["EG_MATERIALS_REQUIRED"] = "";

			PipelineGraphicsState state;
			state.VertexShader = Shader::Create("shadow_maps/shadow_map_skeletal_meshes.vert", ShaderType::Vertex, defines);
			state.FragmentShader = Shader::Create("shadow_maps/shadow_map_masked.frag", ShaderType::Fragment);
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::Dynamic;
			state.bEnableMultiViewRendering = true;
			state.MultiViewPasses = 6;
			state.PerInstanceAttribs = RenderSkeletalMeshesTask::PerInstanceAttribs;

			m_MaskedSMPLPipeline = PipelineGraphics::Create(state);

			state.DepthStencilAttachment.InitialLayout = ImageLayoutType::Unknown;
			state.DepthStencilAttachment.ClearOperation = ClearOperation::Clear;
			m_MaskedSMPLPipelineClearing = PipelineGraphics::Create(state);
		}

		// For Spot lights
		{
			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.Image = RenderManager::GetDummyDepthImage();
			depthAttachment.ClearOperation = ClearOperation::Load;
			depthAttachment.DepthClearValue = 0.f;
			depthAttachment.DepthCompareOp = CompareOperation::Greater;

			ShaderDefines defines;
			defines["EG_SPOT_LIGHT_PASS"] = "";
			defines["EG_MATERIALS_REQUIRED"] = "";

			PipelineGraphicsState state;
			state.VertexShader = Shader::Create("shadow_maps/shadow_map_skeletal_meshes.vert", ShaderType::Vertex, defines);
			state.FragmentShader = Shader::Create("shadow_maps/shadow_map_masked.frag", ShaderType::Fragment);
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::Dynamic;
			state.PerInstanceAttribs = RenderSkeletalMeshesTask::PerInstanceAttribs;

			m_MaskedSMSLPipeline = PipelineGraphics::Create(state);

			state.DepthStencilAttachment.InitialLayout = ImageLayoutType::Unknown;
			state.DepthStencilAttachment.ClearOperation = ClearOperation::Clear;
			m_MaskedSMSLPipelineClearing = PipelineGraphics::Create(state);
		}
	}

	void ShadowPassTask::InitTranslucentSkeletalMeshPipelines()
	{
		ShaderDefines fragmentDefines;
		if (bVolumetricLightsEnabled)
			fragmentDefines["EG_OUTPUT_DEPTH"] = "";

		// For directional light
		{
			ColorAttachment colorAttachment;
			colorAttachment.InitialLayout = ImageLayoutType::Unknown;
			colorAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
			colorAttachment.Image = RenderManager::GetDummyImage();
			colorAttachment.ClearOperation = ClearOperation::Clear;
			colorAttachment.ClearColor = glm::vec4(1.f);
			colorAttachment.bBlendEnabled = true;
			colorAttachment.BlendingState.BlendSrc = BlendFactor::Zero;
			colorAttachment.BlendingState.BlendDst = BlendFactor::SrcColor;
			colorAttachment.BlendingState.BlendOp = BlendOperation::Add;

			ColorAttachment depthColorAttachment;
			depthColorAttachment.InitialLayout = ImageLayoutType::Unknown;
			depthColorAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
			depthColorAttachment.Image = RenderManager::GetDummyImageR16();
			depthColorAttachment.ClearOperation = ClearOperation::Clear;
			depthColorAttachment.ClearColor = glm::vec4(0.f);
			depthColorAttachment.bBlendEnabled = true;
			depthColorAttachment.BlendingState.BlendSrc = BlendFactor::One;
			depthColorAttachment.BlendingState.BlendDst = BlendFactor::One;
			depthColorAttachment.BlendingState.BlendOp = BlendOperation::Max;

			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.Image = RenderManager::GetDummyDepthImage();
			depthAttachment.ClearOperation = ClearOperation::Load;
			depthAttachment.bWriteDepth = false;
			depthAttachment.DepthCompareOp = CompareOperation::Greater;

			const ShaderDefines vertexDefines = { {"EG_MATERIALS_REQUIRED", ""} };

			PipelineGraphicsState state;
			state.CullMode = CullMode::Dynamic;
			state.PerInstanceAttribs = RenderSkeletalMeshesTask::PerInstanceAttribs;
			state.VertexShader = Shader::Create("shadow_maps/shadow_map_skeletal_meshes.vert", ShaderType::Vertex, vertexDefines);
			state.FragmentShader = Shader::Create("shadow_maps/shadow_map_translucent.frag", ShaderType::Fragment, fragmentDefines);
			state.DepthStencilAttachment = depthAttachment;
			state.ColorAttachments.push_back(colorAttachment);
			if (bVolumetricLightsEnabled)
				state.ColorAttachments.push_back(depthColorAttachment);

			if (m_TranslucentSMDLPipelineClearing)
				m_TranslucentSMDLPipelineClearing->SetState(state);
			else
				m_TranslucentSMDLPipelineClearing = PipelineGraphics::Create(state);

			state.DepthStencilAttachment = DepthStencilAttachment{};
			if (m_TranslucentSMDLPipelineClearing_NoDepth)
				m_TranslucentSMDLPipelineClearing_NoDepth->SetState(state);
			else
				m_TranslucentSMDLPipelineClearing_NoDepth = PipelineGraphics::Create(state);

			state.DepthStencilAttachment = depthAttachment;
			for (auto& attachment : state.ColorAttachments)
			{
				attachment.InitialLayout = ImageReadAccess::PixelShaderRead;
				attachment.ClearOperation = ClearOperation::Load;
			}
			if (m_TranslucentSMDLPipeline)
				m_TranslucentSMDLPipeline->SetState(state);
			else
				m_TranslucentSMDLPipeline = PipelineGraphics::Create(state);

			state.DepthStencilAttachment = DepthStencilAttachment{};
			if (m_TranslucentSMDLPipeline_NoDepth)
				m_TranslucentSMDLPipeline_NoDepth->SetState(state);
			else
				m_TranslucentSMDLPipeline_NoDepth = PipelineGraphics::Create(state);
		}

		// For point lights
		{
			ColorAttachment colorAttachment;
			colorAttachment.InitialLayout = ImageLayoutType::Unknown;
			colorAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
			colorAttachment.Image = RenderManager::GetDummyImageCube();
			colorAttachment.ClearOperation = ClearOperation::Clear;
			colorAttachment.ClearColor = glm::vec4(1.f);
			colorAttachment.bBlendEnabled = true;
			colorAttachment.BlendingState.BlendSrc = BlendFactor::Zero;
			colorAttachment.BlendingState.BlendDst = BlendFactor::SrcColor;
			colorAttachment.BlendingState.BlendOp = BlendOperation::Add;

			ColorAttachment depthColorAttachment;
			depthColorAttachment.InitialLayout = ImageLayoutType::Unknown;
			depthColorAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
			depthColorAttachment.Image = RenderManager::GetDummyImageR16Cube();
			depthColorAttachment.ClearOperation = ClearOperation::Clear;
			depthColorAttachment.ClearColor = glm::vec4(0.f);
			depthColorAttachment.bBlendEnabled = true;
			depthColorAttachment.BlendingState.BlendSrc = BlendFactor::One;
			depthColorAttachment.BlendingState.BlendDst = BlendFactor::One;
			depthColorAttachment.BlendingState.BlendOp = BlendOperation::Max;

			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.Image = RenderManager::GetDummyDepthCubeImage();
			depthAttachment.ClearOperation = ClearOperation::Load;
			depthAttachment.bWriteDepth = false;
			depthAttachment.DepthCompareOp = CompareOperation::Greater;

			ShaderDefines defines;
			defines["EG_POINT_LIGHT_PASS"] = "";
			defines["EG_MATERIALS_REQUIRED"] = "";

			PipelineGraphicsState state;
			state.VertexShader = Shader::Create("shadow_maps/shadow_map_skeletal_meshes.vert", ShaderType::Vertex, defines);
			state.FragmentShader = Shader::Create("shadow_maps/shadow_map_translucent.frag", ShaderType::Fragment, fragmentDefines);
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::Dynamic;
			state.bEnableMultiViewRendering = true;
			state.MultiViewPasses = 6;
			state.PerInstanceAttribs = RenderSkeletalMeshesTask::PerInstanceAttribs;

			state.ColorAttachments.push_back(colorAttachment);
			if (bVolumetricLightsEnabled)
				state.ColorAttachments.push_back(depthColorAttachment);

			if (m_TranslucentSMPLPipelineClearing)
				m_TranslucentSMPLPipelineClearing->SetState(state);
			else
				m_TranslucentSMPLPipelineClearing = PipelineGraphics::Create(state);

			state.DepthStencilAttachment = DepthStencilAttachment{};
			if (m_TranslucentSMPLPipelineClearing_NoDepth)
				m_TranslucentSMPLPipelineClearing_NoDepth->SetState(state);
			else
				m_TranslucentSMPLPipelineClearing_NoDepth = PipelineGraphics::Create(state);

			for (auto& attachment : state.ColorAttachments)
			{
				attachment.InitialLayout = ImageReadAccess::PixelShaderRead;
				attachment.ClearOperation = ClearOperation::Load;
			}

			if (m_TranslucentSMPLPipeline_NoDepth)
				m_TranslucentSMPLPipeline_NoDepth->SetState(state);
			else
				m_TranslucentSMPLPipeline_NoDepth = PipelineGraphics::Create(state);

			state.DepthStencilAttachment = depthAttachment;
			if (m_TranslucentSMPLPipeline)
				m_TranslucentSMPLPipeline->SetState(state);
			else
				m_TranslucentSMPLPipeline = PipelineGraphics::Create(state);
		}

		// For Spot lights
		{
			ColorAttachment colorAttachment;
			colorAttachment.InitialLayout = ImageLayoutType::Unknown;
			colorAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
			colorAttachment.Image = RenderManager::GetDummyImage();
			colorAttachment.ClearOperation = ClearOperation::Clear;
			colorAttachment.ClearColor = glm::vec4(1.f, 1.f, 1.f, 0.f);
			colorAttachment.bBlendEnabled = true;
			colorAttachment.BlendingState.BlendSrc = BlendFactor::Zero;
			colorAttachment.BlendingState.BlendDst = BlendFactor::SrcColor;
			colorAttachment.BlendingState.BlendOp = BlendOperation::Add;

			ColorAttachment depthColorAttachment;
			depthColorAttachment.InitialLayout = ImageLayoutType::Unknown;
			depthColorAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
			depthColorAttachment.Image = RenderManager::GetDummyImageR16();
			depthColorAttachment.ClearOperation = ClearOperation::Clear;
			depthColorAttachment.ClearColor = glm::vec4(0.f);
			depthColorAttachment.bBlendEnabled = true;
			depthColorAttachment.BlendingState.BlendSrc = BlendFactor::One;
			depthColorAttachment.BlendingState.BlendDst = BlendFactor::One;
			depthColorAttachment.BlendingState.BlendOp = BlendOperation::Max;

			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.Image = RenderManager::GetDummyDepthImage();
			depthAttachment.ClearOperation = ClearOperation::Load;
			depthAttachment.bWriteDepth = false;
			depthAttachment.DepthCompareOp = CompareOperation::Greater;

			ShaderDefines defines;
			defines["EG_SPOT_LIGHT_PASS"] = "";
			defines["EG_MATERIALS_REQUIRED"] = "";

			PipelineGraphicsState state;
			state.VertexShader = Shader::Create("shadow_maps/shadow_map_skeletal_meshes.vert", ShaderType::Vertex, defines);
			state.FragmentShader = Shader::Create("shadow_maps/shadow_map_translucent.frag", ShaderType::Fragment, fragmentDefines);
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::Dynamic;
			state.PerInstanceAttribs = RenderSkeletalMeshesTask::PerInstanceAttribs;
			state.ColorAttachments.push_back(colorAttachment);
			if (bVolumetricLightsEnabled)
				state.ColorAttachments.push_back(depthColorAttachment);

			if (m_TranslucentSMSLPipelineClearing)
				m_TranslucentSMSLPipelineClearing->SetState(state);
			else
				m_TranslucentSMSLPipelineClearing = PipelineGraphics::Create(state);

			state.DepthStencilAttachment = {};
			if (m_TranslucentSMSLPipelineClearing_NoDepth)
				m_TranslucentSMSLPipelineClearing_NoDepth->SetState(state);
			else
				m_TranslucentSMSLPipelineClearing_NoDepth = PipelineGraphics::Create(state);

			for (auto& attachment : state.ColorAttachments)
			{
				attachment.InitialLayout = ImageReadAccess::PixelShaderRead;
				attachment.ClearOperation = ClearOperation::Load;
			}

			if (m_TranslucentSMSLPipeline_NoDepth)
				m_TranslucentSMSLPipeline_NoDepth->SetState(state);
			else
				m_TranslucentSMSLPipeline_NoDepth = PipelineGraphics::Create(state);

			state.DepthStencilAttachment = depthAttachment;
			if (m_TranslucentSMSLPipeline)
				m_TranslucentSMSLPipeline->SetState(state);
			else
				m_TranslucentSMSLPipeline = PipelineGraphics::Create(state);
		}
	}

	void ShadowPassTask::InitOpacitySpritesPipelines()
	{
		// Directional light
		{
			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.Image = RenderManager::GetDummyDepthImage();
			depthAttachment.ClearOperation = ClearOperation::Load;
			depthAttachment.DepthClearValue = 0.f;
			depthAttachment.DepthCompareOp = CompareOperation::Greater;

			PipelineGraphicsState state;
			state.VertexShader = Shader::Create("shadow_maps/shadow_map_sprites.vert", ShaderType::Vertex);
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::Dynamic;
			state.FrontFace = FrontFaceMode::Clockwise;

			m_OpacitySDLPipeline = PipelineGraphics::Create(state);

			state.DepthStencilAttachment.InitialLayout = ImageLayoutType::Unknown;
			state.DepthStencilAttachment.ClearOperation = ClearOperation::Clear;
			m_OpacitySDLPipelineClearing = PipelineGraphics::Create(state);
		}

		// Point light
		{
			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.Image = RenderManager::GetDummyDepthCubeImage();
			depthAttachment.ClearOperation = ClearOperation::Load;
			depthAttachment.DepthClearValue = 0.f;
			depthAttachment.DepthCompareOp = CompareOperation::Greater;

			ShaderDefines plDefines;
			plDefines["EG_POINT_LIGHT_PASS"] = "";

			PipelineGraphicsState state;
			state.VertexShader = Shader::Create("shadow_maps/shadow_map_sprites.vert", ShaderType::Vertex, plDefines);
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::Dynamic;
			state.FrontFace = FrontFaceMode::Clockwise;
			state.bEnableMultiViewRendering = true;
			state.MultiViewPasses = 6;

			m_OpacitySPLPipeline = PipelineGraphics::Create(state);

			state.DepthStencilAttachment.InitialLayout = ImageLayoutType::Unknown;
			state.DepthStencilAttachment.ClearOperation = ClearOperation::Clear;
			m_OpacitySPLPipelineClearing = PipelineGraphics::Create(state);
		}

		// Spot light
		{
			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.Image = RenderManager::GetDummyDepthImage();
			depthAttachment.ClearOperation = ClearOperation::Load;
			depthAttachment.DepthClearValue = 0.f;
			depthAttachment.DepthCompareOp = CompareOperation::Greater;

			ShaderDefines slDefines;
			slDefines["EG_SPOT_LIGHT_PASS"] = "";

			PipelineGraphicsState state;
			state.VertexShader = Shader::Create("shadow_maps/shadow_map_sprites.vert", ShaderType::Vertex, slDefines);
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::Dynamic;
			state.FrontFace = FrontFaceMode::Clockwise;

			m_OpacitySSLPipeline = PipelineGraphics::Create(state);

			state.DepthStencilAttachment.InitialLayout = ImageLayoutType::Unknown;
			state.DepthStencilAttachment.ClearOperation = ClearOperation::Clear;
			m_OpacitySSLPipelineClearing = PipelineGraphics::Create(state);
		}
	}

	void ShadowPassTask::InitMaskedSpritesPipelines()
	{
		// Directional light
		{
			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.Image = RenderManager::GetDummyDepthImage();
			depthAttachment.ClearOperation = ClearOperation::Load;
			depthAttachment.DepthClearValue = 0.f;
			depthAttachment.DepthCompareOp = CompareOperation::Greater;

			PipelineGraphicsState state;
			state.VertexShader = Shader::Create("shadow_maps/shadow_map_sprites.vert", ShaderType::Vertex, { {"EG_MATERIALS_REQUIRED", ""} });
			state.FragmentShader = Shader::Create("shadow_maps/shadow_map_masked.frag", ShaderType::Fragment);
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::Dynamic;
			state.FrontFace = FrontFaceMode::Clockwise;

			m_MaskedSDLPipeline = PipelineGraphics::Create(state);

			state.DepthStencilAttachment.InitialLayout = ImageLayoutType::Unknown;
			state.DepthStencilAttachment.ClearOperation = ClearOperation::Clear;
			m_MaskedSDLPipelineClearing = PipelineGraphics::Create(state);
		}

		// Point light
		{
			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.Image = RenderManager::GetDummyDepthCubeImage();
			depthAttachment.ClearOperation = ClearOperation::Load;
			depthAttachment.DepthClearValue = 0.f;
			depthAttachment.DepthCompareOp = CompareOperation::Greater;

			ShaderDefines plDefines;
			plDefines["EG_POINT_LIGHT_PASS"] = "";
			plDefines["EG_MATERIALS_REQUIRED"] = "";

			PipelineGraphicsState state;
			state.VertexShader = Shader::Create("shadow_maps/shadow_map_sprites.vert", ShaderType::Vertex, plDefines);
			state.FragmentShader = Shader::Create("shadow_maps/shadow_map_masked.frag", ShaderType::Fragment);
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::Dynamic;
			state.FrontFace = FrontFaceMode::Clockwise;
			state.bEnableMultiViewRendering = true;
			state.MultiViewPasses = 6;

			m_MaskedSPLPipeline = PipelineGraphics::Create(state);

			state.DepthStencilAttachment.InitialLayout = ImageLayoutType::Unknown;
			state.DepthStencilAttachment.ClearOperation = ClearOperation::Clear;
			m_MaskedSPLPipelineClearing = PipelineGraphics::Create(state);
		}

		// Spot light
		{
			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.Image = RenderManager::GetDummyDepthImage();
			depthAttachment.ClearOperation = ClearOperation::Load;
			depthAttachment.DepthClearValue = 0.f;
			depthAttachment.DepthCompareOp = CompareOperation::Greater;

			ShaderDefines slDefines;
			slDefines["EG_SPOT_LIGHT_PASS"] = "";
			slDefines["EG_MATERIALS_REQUIRED"] = "";

			PipelineGraphicsState state;
			state.VertexShader = Shader::Create("shadow_maps/shadow_map_sprites.vert", ShaderType::Vertex, slDefines);
			state.FragmentShader = Shader::Create("shadow_maps/shadow_map_masked.frag", ShaderType::Fragment);
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::Dynamic;
			state.FrontFace = FrontFaceMode::Clockwise;

			m_MaskedSSLPipeline = PipelineGraphics::Create(state);

			state.DepthStencilAttachment.InitialLayout = ImageLayoutType::Unknown;
			state.DepthStencilAttachment.ClearOperation = ClearOperation::Clear;
			m_MaskedSSLPipelineClearing = PipelineGraphics::Create(state);
		}
	}
	
	void ShadowPassTask::InitTranslucentSpritesPipelines()
	{
		ShaderDefines fragmentDefines;
		if (bVolumetricLightsEnabled)
			fragmentDefines["EG_OUTPUT_DEPTH"] = "";

		// Directional light
		{
			ColorAttachment colorAttachment;
			colorAttachment.InitialLayout = ImageLayoutType::Unknown;
			colorAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
			colorAttachment.Image = RenderManager::GetDummyImage();
			colorAttachment.ClearOperation = ClearOperation::Clear;
			colorAttachment.ClearColor = glm::vec4(1.f, 1.f, 1.f, 0.f);
			colorAttachment.bBlendEnabled = true;
			colorAttachment.BlendingState.BlendSrc = BlendFactor::Zero;
			colorAttachment.BlendingState.BlendDst = BlendFactor::SrcColor;
			colorAttachment.BlendingState.BlendOp = BlendOperation::Add;

			ColorAttachment depthColorAttachment;
			depthColorAttachment.InitialLayout = ImageLayoutType::Unknown;
			depthColorAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
			depthColorAttachment.Image = RenderManager::GetDummyImageR16();
			depthColorAttachment.ClearOperation = ClearOperation::Clear;
			depthColorAttachment.ClearColor = glm::vec4(0.f);
			depthColorAttachment.bBlendEnabled = true;
			depthColorAttachment.BlendingState.BlendSrc = BlendFactor::One;
			depthColorAttachment.BlendingState.BlendDst = BlendFactor::One;
			depthColorAttachment.BlendingState.BlendOp = BlendOperation::Max;

			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.Image = RenderManager::GetDummyDepthImage();
			depthAttachment.ClearOperation = ClearOperation::Load;
			depthAttachment.bWriteDepth = false;
			depthAttachment.DepthCompareOp = CompareOperation::Greater;

			PipelineGraphicsState state;
			state.VertexShader = Shader::Create("shadow_maps/shadow_map_sprites.vert", ShaderType::Vertex, { {"EG_MATERIALS_REQUIRED", ""} });
			state.FragmentShader = Shader::Create("shadow_maps/shadow_map_translucent.frag", ShaderType::Fragment, fragmentDefines);
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::Dynamic;
			state.ColorAttachments.push_back(colorAttachment);
			if (bVolumetricLightsEnabled)
				state.ColorAttachments.push_back(depthColorAttachment);

			if (m_TranslucentSDLPipelineClearing)
				m_TranslucentSDLPipelineClearing->SetState(state);
			else
				m_TranslucentSDLPipelineClearing = PipelineGraphics::Create(state);

			state.DepthStencilAttachment = DepthStencilAttachment{};
			if (m_TranslucentSDLPipelineClearing_NoDepth)
				m_TranslucentSDLPipelineClearing_NoDepth->SetState(state);
			else
				m_TranslucentSDLPipelineClearing_NoDepth = PipelineGraphics::Create(state);

			state.DepthStencilAttachment = depthAttachment;
			for (auto& attachment : state.ColorAttachments)
			{
				attachment.InitialLayout = ImageReadAccess::PixelShaderRead;
				attachment.ClearOperation = ClearOperation::Load;
			}
			if (m_TranslucentSDLPipeline)
				m_TranslucentSDLPipeline->SetState(state);
			else
				m_TranslucentSDLPipeline = PipelineGraphics::Create(state);

			state.DepthStencilAttachment = DepthStencilAttachment{};
			if (m_TranslucentSDLPipeline_NoDepth)
				m_TranslucentSDLPipeline_NoDepth->SetState(state);
			else
				m_TranslucentSDLPipeline_NoDepth = PipelineGraphics::Create(state);
		}

		// Point light
		{
			ColorAttachment colorAttachment;
			colorAttachment.InitialLayout = ImageLayoutType::Unknown;
			colorAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
			colorAttachment.Image = RenderManager::GetDummyImageCube();
			colorAttachment.ClearOperation = ClearOperation::Clear;
			colorAttachment.ClearColor = glm::vec4(1.f, 1.f, 1.f, 0.f);
			colorAttachment.bBlendEnabled = true;
			colorAttachment.BlendingState.BlendSrc = BlendFactor::Zero;
			colorAttachment.BlendingState.BlendDst = BlendFactor::SrcColor;
			colorAttachment.BlendingState.BlendOp = BlendOperation::Add;

			ColorAttachment depthColorAttachment;
			depthColorAttachment.InitialLayout = ImageLayoutType::Unknown;
			depthColorAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
			depthColorAttachment.Image = RenderManager::GetDummyImageR16Cube();
			depthColorAttachment.ClearOperation = ClearOperation::Clear;
			depthColorAttachment.ClearColor = glm::vec4(0.f);
			depthColorAttachment.bBlendEnabled = true;
			depthColorAttachment.BlendingState.BlendSrc = BlendFactor::One;
			depthColorAttachment.BlendingState.BlendDst = BlendFactor::One;
			depthColorAttachment.BlendingState.BlendOp = BlendOperation::Max;

			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.Image = RenderManager::GetDummyDepthCubeImage();
			depthAttachment.ClearOperation = ClearOperation::Load;
			depthAttachment.bWriteDepth = false;
			depthAttachment.DepthCompareOp = CompareOperation::Greater;

			ShaderDefines plDefines;
			plDefines["EG_POINT_LIGHT_PASS"] = "";
			plDefines["EG_MATERIALS_REQUIRED"] = "";

			PipelineGraphicsState state;
			state.VertexShader = Shader::Create("shadow_maps/shadow_map_sprites.vert", ShaderType::Vertex, plDefines);
			state.FragmentShader = Shader::Create("shadow_maps/shadow_map_translucent.frag", ShaderType::Fragment, fragmentDefines);
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::Dynamic;
			state.bEnableMultiViewRendering = true;
			state.MultiViewPasses = 6;
			state.ColorAttachments.push_back(colorAttachment);
			if (bVolumetricLightsEnabled)
				state.ColorAttachments.push_back(depthColorAttachment);

			if (m_TranslucentSPLPipelineClearing)
				m_TranslucentSPLPipelineClearing->SetState(state);
			else
				m_TranslucentSPLPipelineClearing = PipelineGraphics::Create(state);

			state.DepthStencilAttachment = DepthStencilAttachment{};
			if (m_TranslucentSPLPipelineClearing_NoDepth)
				m_TranslucentSPLPipelineClearing_NoDepth->SetState(state);
			else
				m_TranslucentSPLPipelineClearing_NoDepth = PipelineGraphics::Create(state);

			for (auto& attachment : state.ColorAttachments)
			{
				attachment.InitialLayout = ImageReadAccess::PixelShaderRead;
				attachment.ClearOperation = ClearOperation::Load;
			}

			if (m_TranslucentSPLPipeline_NoDepth)
				m_TranslucentSPLPipeline_NoDepth->SetState(state);
			else
				m_TranslucentSPLPipeline_NoDepth = PipelineGraphics::Create(state);

			state.DepthStencilAttachment = depthAttachment;
			if (m_TranslucentSPLPipeline)
				m_TranslucentSPLPipeline->SetState(state);
			else
				m_TranslucentSPLPipeline = PipelineGraphics::Create(state);
		}

		// Spot light
		{
			ColorAttachment colorAttachment;
			colorAttachment.InitialLayout = ImageLayoutType::Unknown;
			colorAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
			colorAttachment.Image = RenderManager::GetDummyImage();
			colorAttachment.ClearOperation = ClearOperation::Clear;
			colorAttachment.ClearColor = glm::vec4(1.f);
			colorAttachment.bBlendEnabled = true;
			colorAttachment.BlendingState.BlendSrc = BlendFactor::Zero;
			colorAttachment.BlendingState.BlendDst = BlendFactor::SrcColor;
			colorAttachment.BlendingState.BlendOp = BlendOperation::Add;

			ColorAttachment depthColorAttachment;
			depthColorAttachment.InitialLayout = ImageLayoutType::Unknown;
			depthColorAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
			depthColorAttachment.Image = RenderManager::GetDummyImageR16();
			depthColorAttachment.ClearOperation = ClearOperation::Clear;
			depthColorAttachment.ClearColor = glm::vec4(0.f);
			depthColorAttachment.bBlendEnabled = true;
			depthColorAttachment.BlendingState.BlendSrc = BlendFactor::One;
			depthColorAttachment.BlendingState.BlendDst = BlendFactor::One;
			depthColorAttachment.BlendingState.BlendOp = BlendOperation::Max;

			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.Image = RenderManager::GetDummyDepthImage();
			depthAttachment.ClearOperation = ClearOperation::Load;
			depthAttachment.bWriteDepth = false;
			depthAttachment.DepthCompareOp = CompareOperation::Greater;

			ShaderDefines slDefines;
			slDefines["EG_SPOT_LIGHT_PASS"] = "";
			slDefines["EG_MATERIALS_REQUIRED"] = "";

			PipelineGraphicsState state;
			state.VertexShader = Shader::Create("shadow_maps/shadow_map_sprites.vert", ShaderType::Vertex, slDefines);
			state.FragmentShader = Shader::Create("shadow_maps/shadow_map_translucent.frag", ShaderType::Fragment, fragmentDefines);
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::Dynamic;
			state.ColorAttachments.push_back(colorAttachment);
			if (bVolumetricLightsEnabled)
				state.ColorAttachments.push_back(depthColorAttachment);

			if (m_TranslucentSSLPipelineClearing)
				m_TranslucentSSLPipelineClearing->SetState(state);
			else
				m_TranslucentSSLPipelineClearing = PipelineGraphics::Create(state);

			state.DepthStencilAttachment = {};
			if (m_TranslucentSSLPipelineClearing_NoDepth)
				m_TranslucentSSLPipelineClearing_NoDepth->SetState(state);
			else
				m_TranslucentSSLPipelineClearing_NoDepth = PipelineGraphics::Create(state);

			for (auto& attachment : state.ColorAttachments)
			{
				attachment.InitialLayout = ImageReadAccess::PixelShaderRead;
				attachment.ClearOperation = ClearOperation::Load;
			}

			if (m_TranslucentSSLPipeline_NoDepth)
				m_TranslucentSSLPipeline_NoDepth->SetState(state);
			else
				m_TranslucentSSLPipeline_NoDepth = PipelineGraphics::Create(state);

			state.DepthStencilAttachment = depthAttachment;
			if (m_TranslucentSSLPipeline)
				m_TranslucentSSLPipeline->SetState(state);
			else
				m_TranslucentSSLPipeline = PipelineGraphics::Create(state);
		}
	}

	void ShadowPassTask::InitOpaqueLitTextsPipelines()
	{
		Ref<Shader> fragShader = Shader::Create("shadow_maps/shadow_map_texts_lit.frag", ShaderType::Fragment);

		// Directional light
		{
			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.Image = RenderManager::GetDummyDepthImage();
			depthAttachment.ClearOperation = ClearOperation::Load;
			depthAttachment.DepthClearValue = 0.f;
			depthAttachment.DepthCompareOp = CompareOperation::Greater;

			PipelineGraphicsState state;
			state.VertexShader = Shader::Create("shadow_maps/shadow_map_texts_lit.vert", ShaderType::Vertex);
			state.FragmentShader = fragShader;
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::Dynamic;

			m_OpaqueLitTDLPipeline = PipelineGraphics::Create(state);

			state.DepthStencilAttachment.InitialLayout = ImageLayoutType::Unknown;
			state.DepthStencilAttachment.ClearOperation = ClearOperation::Clear;
			m_OpaqueLitTDLPipelineClearing = PipelineGraphics::Create(state);
		}

		// Point light
		{
			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.Image = RenderManager::GetDummyDepthCubeImage();
			depthAttachment.ClearOperation = ClearOperation::Load;
			depthAttachment.DepthClearValue = 0.f;
			depthAttachment.DepthCompareOp = CompareOperation::Greater;

			ShaderDefines plDefines;
			plDefines["EG_POINT_LIGHT_PASS"] = "";

			PipelineGraphicsState state;
			state.VertexShader = Shader::Create("shadow_maps/shadow_map_texts_lit.vert", ShaderType::Vertex, plDefines);
			state.FragmentShader = fragShader;
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::Dynamic;
			state.bEnableMultiViewRendering = true;
			state.MultiViewPasses = 6;

			m_OpaqueLitTPLPipeline = PipelineGraphics::Create(state);

			state.DepthStencilAttachment.InitialLayout = ImageLayoutType::Unknown;
			state.DepthStencilAttachment.ClearOperation = ClearOperation::Clear;
			m_OpaqueLitTPLPipelineClearing = PipelineGraphics::Create(state);
		}

		// Spot light
		{
			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.Image = RenderManager::GetDummyDepthImage();
			depthAttachment.ClearOperation = ClearOperation::Load;
			depthAttachment.DepthClearValue = 0.f;
			depthAttachment.DepthCompareOp = CompareOperation::Greater;

			ShaderDefines slDefines;
			slDefines["EG_SPOT_LIGHT_PASS"] = "";

			PipelineGraphicsState state;
			state.VertexShader = Shader::Create("shadow_maps/shadow_map_texts_lit.vert", ShaderType::Vertex, slDefines);
			state.FragmentShader = fragShader;
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::Dynamic;

			m_OpaqueLitTSLPipeline = PipelineGraphics::Create(state);

			state.DepthStencilAttachment.InitialLayout = ImageLayoutType::Unknown;
			state.DepthStencilAttachment.ClearOperation = ClearOperation::Clear;
			m_OpaqueLitTSLPipelineClearing = PipelineGraphics::Create(state);
		}
	}
	
	void ShadowPassTask::InitMaskedLitTextsPipelines()
	{
		Ref<Shader> fragShader = Shader::Create("shadow_maps/shadow_map_texts_lit.frag", ShaderType::Fragment, { {"EG_MASKED", ""}, {"EG_MATERIALS_REQUIRED", ""} });

		// Directional light
		{
			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.Image = RenderManager::GetDummyDepthImage();
			depthAttachment.ClearOperation = ClearOperation::Load;
			depthAttachment.DepthClearValue = 0.f;
			depthAttachment.DepthCompareOp = CompareOperation::Greater;

			PipelineGraphicsState state;
			state.VertexShader = Shader::Create("shadow_maps/shadow_map_texts_lit.vert", ShaderType::Vertex, { {"EG_MATERIALS_REQUIRED", ""} });
			state.FragmentShader = fragShader;
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::Dynamic;

			m_MaskedLitTDLPipeline = PipelineGraphics::Create(state);

			state.DepthStencilAttachment.InitialLayout = ImageLayoutType::Unknown;
			state.DepthStencilAttachment.ClearOperation = ClearOperation::Clear;
			m_MaskedLitTDLPipelineClearing = PipelineGraphics::Create(state);
		}

		// Point light
		{
			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.Image = RenderManager::GetDummyDepthCubeImage();
			depthAttachment.ClearOperation = ClearOperation::Load;
			depthAttachment.DepthClearValue = 0.f;
			depthAttachment.DepthCompareOp = CompareOperation::Greater;

			ShaderDefines plDefines;
			plDefines["EG_POINT_LIGHT_PASS"] = "";
			plDefines["EG_MATERIALS_REQUIRED"] = "";

			PipelineGraphicsState state;
			state.VertexShader = Shader::Create("shadow_maps/shadow_map_texts_lit.vert", ShaderType::Vertex, plDefines);
			state.FragmentShader = fragShader;
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::Dynamic;
			state.bEnableMultiViewRendering = true;
			state.MultiViewPasses = 6;

			m_MaskedLitTPLPipeline = PipelineGraphics::Create(state);

			state.DepthStencilAttachment.InitialLayout = ImageLayoutType::Unknown;
			state.DepthStencilAttachment.ClearOperation = ClearOperation::Clear;
			m_MaskedLitTPLPipelineClearing = PipelineGraphics::Create(state);
		}

		// Spot light
		{
			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.Image = RenderManager::GetDummyDepthImage();
			depthAttachment.ClearOperation = ClearOperation::Load;
			depthAttachment.DepthClearValue = 0.f;
			depthAttachment.DepthCompareOp = CompareOperation::Greater;

			ShaderDefines slDefines;
			slDefines["EG_SPOT_LIGHT_PASS"] = "";
			slDefines["EG_MATERIALS_REQUIRED"] = "";

			PipelineGraphicsState state;
			state.VertexShader = Shader::Create("shadow_maps/shadow_map_texts_lit.vert", ShaderType::Vertex, slDefines);
			state.FragmentShader = fragShader;
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::Dynamic;

			m_MaskedLitTSLPipeline = PipelineGraphics::Create(state);

			state.DepthStencilAttachment.InitialLayout = ImageLayoutType::Unknown;
			state.DepthStencilAttachment.ClearOperation = ClearOperation::Clear;
			m_MaskedLitTSLPipelineClearing = PipelineGraphics::Create(state);
		}
	}
	
	void ShadowPassTask::InitTranslucentLitTextsPipelines()
	{
		ShaderDefines fragmentDefines;
		fragmentDefines["EG_TRANSLUCENT"] = "";
		fragmentDefines["EG_MATERIALS_REQUIRED"] = "";
		if (bVolumetricLightsEnabled)
			fragmentDefines["EG_OUTPUT_DEPTH"] = "";

		Ref<Shader> fragShader = Shader::Create("shadow_maps/shadow_map_texts_lit.frag", ShaderType::Fragment, fragmentDefines);

		// Directional light
		{
			ColorAttachment colorAttachment;
			colorAttachment.InitialLayout = ImageLayoutType::Unknown;
			colorAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
			colorAttachment.Image = RenderManager::GetDummyImage();
			colorAttachment.ClearOperation = ClearOperation::Clear;
			colorAttachment.ClearColor = glm::vec4(1.f, 1.f, 1.f, 0.f);
			colorAttachment.bBlendEnabled = true;
			colorAttachment.BlendingState.BlendSrc = BlendFactor::Zero;
			colorAttachment.BlendingState.BlendDst = BlendFactor::SrcColor;
			colorAttachment.BlendingState.BlendOp = BlendOperation::Add;

			ColorAttachment depthColorAttachment;
			depthColorAttachment.InitialLayout = ImageLayoutType::Unknown;
			depthColorAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
			depthColorAttachment.Image = RenderManager::GetDummyImageR16();
			depthColorAttachment.ClearOperation = ClearOperation::Clear;
			depthColorAttachment.ClearColor = glm::vec4(0.f);
			depthColorAttachment.bBlendEnabled = true;
			depthColorAttachment.BlendingState.BlendSrc = BlendFactor::One;
			depthColorAttachment.BlendingState.BlendDst = BlendFactor::One;
			depthColorAttachment.BlendingState.BlendOp = BlendOperation::Max;

			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.Image = RenderManager::GetDummyDepthImage();
			depthAttachment.ClearOperation = ClearOperation::Load;
			depthAttachment.bWriteDepth = false;
			depthAttachment.DepthCompareOp = CompareOperation::Greater;

			PipelineGraphicsState state;
			state.VertexShader = Shader::Create("shadow_maps/shadow_map_texts_lit.vert", ShaderType::Vertex, { {"EG_MATERIALS_REQUIRED", ""} });
			state.FragmentShader = fragShader;
			state.ColorAttachments.push_back(colorAttachment);
			if (bVolumetricLightsEnabled)
				state.ColorAttachments.push_back(depthColorAttachment);
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::Dynamic;

			if (m_TranslucentLitTDLPipelineClearing)
				m_TranslucentLitTDLPipelineClearing->SetState(state);
			else
				m_TranslucentLitTDLPipelineClearing = PipelineGraphics::Create(state);

			state.DepthStencilAttachment = DepthStencilAttachment{};
			if (m_TranslucentLitTDLPipelineClearing_NoDepth)
				m_TranslucentLitTDLPipelineClearing_NoDepth->SetState(state);
			else
				m_TranslucentLitTDLPipelineClearing_NoDepth = PipelineGraphics::Create(state);

			state.DepthStencilAttachment = depthAttachment;
			for (auto& attachment : state.ColorAttachments)
			{
				attachment.InitialLayout = ImageReadAccess::PixelShaderRead;
				attachment.ClearOperation = ClearOperation::Load;
			}

			if (m_TranslucentLitTDLPipeline)
				m_TranslucentLitTDLPipeline->SetState(state);
			else
				m_TranslucentLitTDLPipeline = PipelineGraphics::Create(state);

			state.DepthStencilAttachment = DepthStencilAttachment{};
			if (m_TranslucentLitTDLPipeline_NoDepth)
				m_TranslucentLitTDLPipeline_NoDepth->SetState(state);
			else
				m_TranslucentLitTDLPipeline_NoDepth = PipelineGraphics::Create(state);
		}

		// Point light
		{
			ColorAttachment colorAttachment;
			colorAttachment.InitialLayout = ImageLayoutType::Unknown;
			colorAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
			colorAttachment.Image = RenderManager::GetDummyImageCube();
			colorAttachment.ClearOperation = ClearOperation::Clear;
			colorAttachment.ClearColor = glm::vec4(1.f, 1.f, 1.f, 0.f);
			colorAttachment.bBlendEnabled = true;
			colorAttachment.BlendingState.BlendSrc = BlendFactor::Zero;
			colorAttachment.BlendingState.BlendDst = BlendFactor::SrcColor;
			colorAttachment.BlendingState.BlendOp = BlendOperation::Add;

			ColorAttachment depthColorAttachment;
			depthColorAttachment.InitialLayout = ImageLayoutType::Unknown;
			depthColorAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
			depthColorAttachment.Image = RenderManager::GetDummyImageR16Cube();
			depthColorAttachment.ClearOperation = ClearOperation::Clear;
			depthColorAttachment.ClearColor = glm::vec4(0.f);
			depthColorAttachment.bBlendEnabled = true;
			depthColorAttachment.BlendingState.BlendSrc = BlendFactor::One;
			depthColorAttachment.BlendingState.BlendDst = BlendFactor::One;
			depthColorAttachment.BlendingState.BlendOp = BlendOperation::Max;

			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.Image = RenderManager::GetDummyDepthCubeImage();
			depthAttachment.ClearOperation = ClearOperation::Load;
			depthAttachment.bWriteDepth = false;
			depthAttachment.DepthCompareOp = CompareOperation::Greater;

			ShaderDefines plDefines;
			plDefines["EG_POINT_LIGHT_PASS"] = "";
			plDefines["EG_MATERIALS_REQUIRED"] = "";

			PipelineGraphicsState state;
			state.VertexShader = Shader::Create("shadow_maps/shadow_map_texts_lit.vert", ShaderType::Vertex, plDefines);
			state.FragmentShader = fragShader;
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::Dynamic;
			state.bEnableMultiViewRendering = true;
			state.MultiViewPasses = 6;
			state.ColorAttachments.push_back(colorAttachment);
			if (bVolumetricLightsEnabled)
				state.ColorAttachments.push_back(depthColorAttachment);

			if (m_TranslucentLitTPLPipelineClearing)
				m_TranslucentLitTPLPipelineClearing->SetState(state);
			else
				m_TranslucentLitTPLPipelineClearing = PipelineGraphics::Create(state);

			state.DepthStencilAttachment = {};
			if (m_TranslucentLitTPLPipelineClearing_NoDepth)
				m_TranslucentLitTPLPipelineClearing_NoDepth->SetState(state);
			else
				m_TranslucentLitTPLPipelineClearing_NoDepth = PipelineGraphics::Create(state);

			for (auto& attachment : state.ColorAttachments)
			{
				attachment.InitialLayout = ImageReadAccess::PixelShaderRead;
				attachment.ClearOperation = ClearOperation::Load;
			}

			if (m_TranslucentLitTPLPipeline_NoDepth)
				m_TranslucentLitTPLPipeline_NoDepth->SetState(state);
			else
				m_TranslucentLitTPLPipeline_NoDepth = PipelineGraphics::Create(state);

			state.DepthStencilAttachment = depthAttachment;
			if (m_TranslucentLitTPLPipeline)
				m_TranslucentLitTPLPipeline->SetState(state);
			else
				m_TranslucentLitTPLPipeline = PipelineGraphics::Create(state);
		}

		// Spot light
		{
			ColorAttachment colorAttachment;
			colorAttachment.InitialLayout = ImageLayoutType::Unknown;
			colorAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
			colorAttachment.Image = RenderManager::GetDummyImage();
			colorAttachment.ClearOperation = ClearOperation::Clear;
			colorAttachment.ClearColor = glm::vec4(1.f, 1.f, 1.f, 0.f);
			colorAttachment.bBlendEnabled = true;
			colorAttachment.BlendingState.BlendSrc = BlendFactor::Zero;
			colorAttachment.BlendingState.BlendDst = BlendFactor::SrcColor;
			colorAttachment.BlendingState.BlendOp = BlendOperation::Add;

			ColorAttachment depthColorAttachment;
			depthColorAttachment.InitialLayout = ImageLayoutType::Unknown;
			depthColorAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
			depthColorAttachment.Image = RenderManager::GetDummyImageR16();
			depthColorAttachment.ClearOperation = ClearOperation::Clear;
			depthColorAttachment.ClearColor = glm::vec4(0.f);
			depthColorAttachment.bBlendEnabled = true;
			depthColorAttachment.BlendingState.BlendSrc = BlendFactor::One;
			depthColorAttachment.BlendingState.BlendDst = BlendFactor::One;
			depthColorAttachment.BlendingState.BlendOp = BlendOperation::Max;

			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.Image = RenderManager::GetDummyDepthImage();
			depthAttachment.ClearOperation = ClearOperation::Load;
			depthAttachment.bWriteDepth = false;
			depthAttachment.DepthCompareOp = CompareOperation::Greater;

			ShaderDefines slDefines;
			slDefines["EG_SPOT_LIGHT_PASS"] = "";
			slDefines["EG_MATERIALS_REQUIRED"] = "";

			PipelineGraphicsState state;
			state.VertexShader = Shader::Create("shadow_maps/shadow_map_texts_lit.vert", ShaderType::Vertex, slDefines);
			state.FragmentShader = fragShader;
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::Dynamic;
			state.ColorAttachments.push_back(colorAttachment);
			if (bVolumetricLightsEnabled)
				state.ColorAttachments.push_back(depthColorAttachment);

			if (m_TranslucentLitTSLPipelineClearing)
				m_TranslucentLitTSLPipelineClearing->SetState(state);
			else
				m_TranslucentLitTSLPipelineClearing = PipelineGraphics::Create(state);

			state.DepthStencilAttachment = {};
			if (m_TranslucentLitTSLPipelineClearing_NoDepth)
				m_TranslucentLitTSLPipelineClearing_NoDepth->SetState(state);
			else
				m_TranslucentLitTSLPipelineClearing_NoDepth = PipelineGraphics::Create(state);

			for (auto& attachment : state.ColorAttachments)
			{
				attachment.InitialLayout = ImageReadAccess::PixelShaderRead;
				attachment.ClearOperation = ClearOperation::Load;
			}

			if (m_TranslucentLitTSLPipeline_NoDepth)
				m_TranslucentLitTSLPipeline_NoDepth->SetState(state);
			else
				m_TranslucentLitTSLPipeline_NoDepth = PipelineGraphics::Create(state);

			state.DepthStencilAttachment = depthAttachment;
			if (m_TranslucentLitTSLPipeline)
				m_TranslucentLitTSLPipeline->SetState(state);
			else
				m_TranslucentLitTSLPipeline = PipelineGraphics::Create(state);
		}
	}

	void ShadowPassTask::InitUnlitTextsPipelines()
	{
		Ref<Shader> fragShader = Shader::Create("shadow_maps/shadow_map_texts_unlit.frag", ShaderType::Fragment);

		// Directional light
		{
			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.Image = RenderManager::GetDummyDepthImage();
			depthAttachment.ClearOperation = ClearOperation::Load;
			depthAttachment.DepthClearValue = 0.f;
			depthAttachment.DepthCompareOp = CompareOperation::Greater;

			PipelineGraphicsState state;
			state.VertexShader = Shader::Create("shadow_maps/shadow_map_texts_unlit.vert", ShaderType::Vertex);
			state.FragmentShader = fragShader;
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::None;

			m_UnlitTDLPipeline = PipelineGraphics::Create(state);

			state.DepthStencilAttachment.InitialLayout = ImageLayoutType::Unknown;
			state.DepthStencilAttachment.ClearOperation = ClearOperation::Clear;
			m_UnlitTDLPipelineClearing = PipelineGraphics::Create(state);
		}

		// Point light
		{
			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.Image = RenderManager::GetDummyDepthCubeImage();
			depthAttachment.ClearOperation = ClearOperation::Load;
			depthAttachment.DepthClearValue = 0.f;
			depthAttachment.DepthCompareOp = CompareOperation::Greater;

			ShaderDefines plDefines;
			plDefines["EG_POINT_LIGHT_PASS"] = "";

			PipelineGraphicsState state;
			state.VertexShader = Shader::Create("shadow_maps/shadow_map_texts_unlit.vert", ShaderType::Vertex, plDefines);
			state.FragmentShader = fragShader;
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::None;
			state.bEnableMultiViewRendering = true;
			state.MultiViewPasses = 6;

			m_UnlitTPLPipeline = PipelineGraphics::Create(state);

			state.DepthStencilAttachment.InitialLayout = ImageLayoutType::Unknown;
			state.DepthStencilAttachment.ClearOperation = ClearOperation::Clear;
			m_UnlitTPLPipelineClearing = PipelineGraphics::Create(state);
		}

		// Spot light
		{
			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.Image = RenderManager::GetDummyDepthImage();
			depthAttachment.ClearOperation = ClearOperation::Load;
			depthAttachment.DepthClearValue = 0.f;
			depthAttachment.DepthCompareOp = CompareOperation::Greater;

			ShaderDefines slDefines;
			slDefines["EG_SPOT_LIGHT_PASS"] = "";

			PipelineGraphicsState state;
			state.VertexShader = Shader::Create("shadow_maps/shadow_map_texts_unlit.vert", ShaderType::Vertex, slDefines);
			state.FragmentShader = fragShader;
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::None;

			m_UnlitTSLPipeline = PipelineGraphics::Create(state);

			state.DepthStencilAttachment.InitialLayout = ImageLayoutType::Unknown;
			state.DepthStencilAttachment.ClearOperation = ClearOperation::Clear;
			m_UnlitTSLPipelineClearing = PipelineGraphics::Create(state);
		}
	}
	
	void ShadowPassTask::CreateIfNeededDirectionalLightShadowMaps()
	{
		if (m_DLShadowMaps[0] != RenderManager::GetDummyDepthImage())
			return;

		InitDirectionalLightShadowMaps();
	}

	void ShadowPassTask::InitDirectionalLightShadowMaps()
	{
		m_DLShadowMaps.resize(EG_CASCADES_COUNT);
		m_DLFramebuffers.resize(EG_CASCADES_COUNT);

		const auto& csmSizes = m_Settings.DirLightShadowMapSizes;
		for (uint32_t i = 0; i < EG_CASCADES_COUNT; ++i)
		{
			const glm::uvec3 size = glm::uvec3(csmSizes[i], csmSizes[i], 1);
			m_DLShadowMaps[i] = CreateDepthImage(size, std::string("CSMShadowMap") + std::to_string(i), false);
		}

		const void* renderPassHandle = m_OpacityMDLPipeline->GetRenderPassHandle();
		for (uint32_t i = 0; i < EG_CASCADES_COUNT; ++i)
			m_DLFramebuffers[i] = Framebuffer::Create({ m_DLShadowMaps[i] }, glm::uvec2(csmSizes[i]), renderPassHandle);
	}
	
	void ShadowPassTask::FreeDirectionalLightShadowMaps()
	{
		std::fill(m_DLShadowMaps.begin(), m_DLShadowMaps.end(), RenderManager::GetDummyDepthImage());
		m_DLFramebuffers.clear();
	}
	
	void ShadowPassTask::CreateIfNeededColoredDirectionalLightShadowMaps()
	{
		if (m_DLCShadowMaps[0] != RenderManager::GetDummyImage())
			return;

		InitColoredDirectionalLightShadowMaps();
	}

	void ShadowPassTask::InitColoredDirectionalLightShadowMaps()
	{
		m_DLCShadowMaps.resize(EG_CASCADES_COUNT);
		if (bVolumetricLightsEnabled)
			m_DLCDShadowMaps.resize(EG_CASCADES_COUNT);
		else
			std::fill(m_DLCDShadowMaps.begin(), m_DLCDShadowMaps.end(), RenderManager::GetDummyImageR16());

		const auto& csmSizes = m_Settings.DirLightShadowMapSizes;
		for (uint32_t i = 0; i < EG_CASCADES_COUNT; ++i)
		{
			const glm::uvec3 size = glm::uvec3(csmSizes[i], csmSizes[i], 1);
			m_DLCShadowMaps[i] = CreateColoredFilterImage(size, std::string("CSMShadowMap_Colored") + std::to_string(i), false);
			if (bVolumetricLightsEnabled)
				m_DLCDShadowMaps[i] = CreateDepthImage16(size, std::string("CSMShadowMap_Colored_Depth") + std::to_string(i), false);
		}
	}

	void ShadowPassTask::InitColoredDirectionalLightFramebuffers(std::vector<Ref<Framebuffer>>& framebuffers, const Ref<PipelineGraphics>& pipeline, bool bIncludeDepth)
	{
		const auto& csmSizes = m_Settings.DirLightShadowMapSizes;
		const auto& nonTraslucentShadowMaps = m_DLShadowMaps;
		const void* renderPassHandle = pipeline->GetRenderPassHandle();
		framebuffers.resize(EG_CASCADES_COUNT);
		for (uint32_t i = 0; i < EG_CASCADES_COUNT; ++i)
		{
			std::vector<Ref<Image>> attachments;
			attachments.reserve(3);

			attachments.push_back(m_DLCShadowMaps[i]);
			if (bVolumetricLightsEnabled)
				attachments.push_back(m_DLCDShadowMaps[i]);
			if (bIncludeDepth)
				attachments.push_back(nonTraslucentShadowMaps[i]);

			framebuffers[i] = Framebuffer::Create(attachments, glm::uvec2(csmSizes[i]), renderPassHandle);
		}
	}
	
	void ShadowPassTask::FreeColoredDirectionalLightShadowMaps()
	{
		std::fill(m_DLCShadowMaps.begin(), m_DLCShadowMaps.end(), RenderManager::GetDummyImage());
		std::fill(m_DLCDShadowMaps.begin(), m_DLCDShadowMaps.end(), RenderManager::GetDummyImageR16());
		m_DLCFramebuffers.clear();
		m_DLCFramebuffers_NoDepth.clear();
	}
	
	void ShadowPassTask::HandleColoredPointLightShadowMaps()
	{
		m_PLCShadowMaps.clear();
		m_PLCDShadowMaps.clear();
		m_PLCFramebuffers.clear();
		m_PLCFramebuffers_NoDepth.clear();
	}

	void ShadowPassTask::HandleColoredSpotLightShadowMaps()
	{
		m_SLCShadowMaps.clear();
		m_SLCDShadowMaps.clear();
		m_SLCFramebuffers.clear();
		m_SLCFramebuffers_NoDepth.clear();
	}
}
