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
		for (float f = 0.25f; f < 1.f; f += 0.25f)
		{
			if (k > f)
				scaler *= 2u; // Resolution is getting two times lower each 25% of the distance
			else
				break; // early exit
		}

		constexpr glm::uvec2 minRes = glm::uvec2(64u);

		glm::uvec2 size = glm::uvec2(m_Settings.PointLightShadowMapSize / scaler);
		size = glm::max(minRes, size);

		return size;
	}

	static glm::uvec2 GetPointLightSMSize2(uint pointLightShadowMapSize, float distance, float radius, float maxDistance)
	{
		float importance = radius / glm::max(distance, 0.001f);

		// Normalize importance
		float scale = glm::clamp(importance, 0.0f, 1.0f);
		uint32_t size = uint32_t(pointLightShadowMapSize * scale);

		size = glm::max(64u, size);
		size = std::bit_ceil(size);

		return glm::uvec2(size);
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
		depthSpecs.Usage = ImageUsage::DepthStencilAttachment | ImageUsage::Sampled | ImageUsage::TransferDst;
		depthSpecs.bIsCube = bCube;
		depthSpecs.Size = size;
		return Image::Create(depthSpecs, debugName);
	}

	static Ref<Image> CreateColoredFilterImage(glm::uvec3 size, const std::string& debugName, bool bCube)
	{
		ImageSpecifications specs;
		specs.Format = ImageFormat::R8G8B8A8_UNorm;
		specs.Usage = ImageUsage::ColorAttachment | ImageUsage::Sampled | ImageUsage::TransferDst;
		specs.bIsCube = bCube;
		specs.Size = size;
		return Image::Create(specs, debugName);
	}

	static Ref<Image> CreateDepthImage16(glm::uvec3 size, const std::string& debugName, bool bCube)
	{
		ImageSpecifications depthSpecs;
		depthSpecs.Format = ImageFormat::R16_Float;
		depthSpecs.Usage = ImageUsage::ColorAttachment | ImageUsage::Sampled | ImageUsage::TransferDst;
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

		HandlePointLightResources(cmd);
		HandleSpotLightResources(cmd);
		HandleDirectionalLightResources(cmd);

		// Clears framebuffers if they weren't
		// It's required so that shadowmaps don't have invalid values and shading translucent objects to work correctly
		ClearShadowMaps(cmd);

		ShadowPassOpaqueMeshes(cmd);
		ShadowPassMaskedMeshes(cmd);
		ShadowPassOpaqueSkeletalMeshes(cmd);
		ShadowPassMaskedSkeletalMeshes(cmd);
		ShadowPassOpaqueSprites(cmd);
		ShadowPassMaskedSprites(cmd);
		ShadowPassOpaqueLitTexts(cmd);
		ShadowPassMaskedLitTexts(cmd);
		ShadowPassUnlitTexts(cmd);
		
		if (bTranslucencyShadowsEnabled)
		{
			ShadowPassTranslucentMeshes(cmd);
			ShadowPassTranslucentSkeletalMeshes(cmd);
			ShadowPassTranslucentSprites(cmd);
			ShadowPassTranslucentLitTexts(cmd);
		}

		PrepareShadowMapsForSampling(cmd);
	}

	void ShadowPassTask::HandlePointLightResources(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "Shadow pass. Handle PointLight Resources");
		EG_CPU_TIMING_SCOPED("Shadow pass. Handle PointLight Resources");

		const auto& pointLights = m_Renderer.GetCulledPointLights();
		auto& framebuffers = m_PLFramebuffers;
		auto& shadowMaps = m_PLShadowMaps;
		auto& coloredShadowMaps = m_PLCShadowMaps;
		auto& depthShadowMaps = m_PLCDShadowMaps;
		const auto& pipeline = m_OpacityMPLPipeline;

		auto& translucentFramebuffers = m_PLCFramebuffers;
		auto& translucentPipeline = m_TranslucentMPLPipeline;

		m_PLVPs.clear();
		m_PointLightIndices.clear();
		uint32_t pointLightsCount = 0;
		const glm::vec3 cameraPos = m_Renderer.GetViewPosition();
		const float shadowMaxDistance = m_Renderer.GetShadowMaxDistance();
		const auto& plMatrices = m_Renderer.GetPointLightMatrices();
		for (size_t plIndex = 0; plIndex < pointLights.size(); ++plIndex)
		{
			auto& pointLight = pointLights[plIndex];
			if (pointLight.ShadowMapIndex == EG_INVALID_SHADOW_MAP)
				continue;

			for (uint32_t i = 0; i < 6; ++i)
			{
				m_PLVPs.push_back(plMatrices[pointLight.ViewProjOffset + i]);
			}

			m_PointLightIndices.push_back(plIndex);
			const float distanceToCamera = glm::length(cameraPos - pointLight.Position);
			const uint32_t& i = pointLightsCount;

			const glm::uvec3 smSize = glm::uvec3(GetPointLightSMSize2(m_Settings.PointLightShadowMapSize, distanceToCamera, pointLight.Radius, shadowMaxDistance), 1u);
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
					attachments.push_back(shadowMaps[i]);

					Ref<Framebuffer> fb = Framebuffer::Create(attachments, smSize, translucentPipeline->GetRenderPassHandle());
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
		}
	}
	
	void ShadowPassTask::HandleSpotLightResources(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "Shadow pass. Handle SpotLight Resources");
		EG_CPU_TIMING_SCOPED("Shadow pass. Handle SpotLight Resources");

		const auto& spotLights = m_Renderer.GetCulledSpotLights();
		auto& framebuffers = m_SLFramebuffers;
		auto& shadowMaps = m_SLShadowMaps;
		auto& coloredShadowMaps = m_SLCShadowMaps;
		auto& depthShadowMaps = m_SLCDShadowMaps;
		const auto& pipeline = m_OpacityMSLPipeline;

		auto& translucentFramebuffers = m_SLCFramebuffers;
		auto& translucentPipeline = m_TranslucentMSLPipeline;

		uint32_t spotLightsCount = 0;
		const glm::vec3 cameraPos = m_Renderer.GetViewPosition();
		const float shadowMaxDistance = m_Renderer.GetShadowMaxDistance();
		m_SpotLightIndices.clear();
		for (size_t slIndex = 0; slIndex < spotLights.size(); ++slIndex)
		{
			auto& spotLight = spotLights[slIndex];
			if (spotLight.ShadowMapIndex == EG_INVALID_SHADOW_MAP)
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
					attachments.push_back(shadowMaps[i]);

					Ref<Framebuffer> fb = Framebuffer::Create(attachments, smSize, translucentPipeline->GetRenderPassHandle());
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
		}
	}

	void ShadowPassTask::HandleDirectionalLightResources(const Ref<CommandBuffer>& cmd)
	{
		const auto& dirLight = m_Renderer.GetDirectionalLight();
		if (m_Renderer.HasDirectionalLight() && dirLight.bCastsShadows)
		{
			CreateIfNeededDirectionalLightShadowMaps();
			CreateIfNeededColoredDirectionalLightShadowMaps();
			if (m_DLCFramebuffers.empty())
				InitColoredDirectionalLightFramebuffers(m_DLCFramebuffers, m_TranslucentMDLPipeline);
		}
		else
		{
			FreeDirectionalLightShadowMaps();
			FreeColoredDirectionalLightShadowMaps();
		}
	}

	void ShadowPassTask::ClearShadowMaps(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "Shadow pass. Clearing shadow-maps");
		EG_CPU_TIMING_SCOPED("Shadow pass. Clearing shadow-maps");

		constexpr float depthClearValue = 0.0f;
		constexpr glm::vec4 depthClearValue4 = glm::vec4(depthClearValue);
		constexpr glm::vec4 coloredClearValue = glm::vec4(1, 1, 1, 0);

		for (const auto& sm : m_PLShadowMaps)
			cmd->ClearDepthStencilImage(sm, depthClearValue, 0, sm->GetLayout(), ImageLayoutType::DepthStencilWrite);
		for (const auto& sm : m_SLShadowMaps)
			cmd->ClearDepthStencilImage(sm, depthClearValue, 0, sm->GetLayout(), ImageLayoutType::DepthStencilWrite);

		for (const auto& sm : m_PLCShadowMaps)
			cmd->ClearColorImage(sm, coloredClearValue, sm->GetLayout(), ImageLayoutType::RenderTarget);
		for (const auto& sm : m_SLCShadowMaps)
			cmd->ClearColorImage(sm, coloredClearValue, sm->GetLayout(), ImageLayoutType::RenderTarget);

		for (const auto& sm : m_PLCDShadowMaps)
			cmd->ClearColorImage(sm, depthClearValue4, sm->GetLayout(), ImageLayoutType::RenderTarget);
		for (const auto& sm : m_SLCDShadowMaps)
			cmd->ClearColorImage(sm, depthClearValue4, sm->GetLayout(), ImageLayoutType::RenderTarget);

		const auto& dirLight = m_Renderer.GetDirectionalLight();
		if (m_Renderer.HasDirectionalLight() && dirLight.bCastsShadows)
		{
			for (const auto& sm : m_DLShadowMaps)
				cmd->ClearDepthStencilImage(sm, depthClearValue, 0, sm->GetLayout(), ImageLayoutType::DepthStencilWrite);
			for (const auto& sm : m_DLCShadowMaps)
				cmd->ClearColorImage(sm, coloredClearValue, sm->GetLayout(), ImageLayoutType::RenderTarget);
			for (const auto& sm : m_DLCDShadowMaps)
				cmd->ClearColorImage(sm, depthClearValue4, sm->GetLayout(), ImageLayoutType::RenderTarget);
		}
	}

	void ShadowPassTask::PrepareShadowMapsForSampling(const Ref<CommandBuffer>& cmd)
	{
		for (const auto& sm : m_PLShadowMaps)
			cmd->TransitionLayout(sm, sm->GetLayout(), ImageReadAccess::PixelShaderRead);
		for (const auto& sm : m_SLShadowMaps)
			cmd->TransitionLayout(sm, sm->GetLayout(), ImageReadAccess::PixelShaderRead);
		for (const auto& sm : m_DLShadowMaps)
			cmd->TransitionLayout(sm, sm->GetLayout(), ImageReadAccess::PixelShaderRead);

		for (const auto& sm : m_PLCShadowMaps)
			cmd->TransitionLayout(sm, sm->GetLayout(), ImageReadAccess::PixelShaderRead);
		for (const auto& sm : m_SLCShadowMaps)
			cmd->TransitionLayout(sm, sm->GetLayout(), ImageReadAccess::PixelShaderRead);
		for (const auto& sm : m_DLCShadowMaps)
			cmd->TransitionLayout(sm, sm->GetLayout(), ImageReadAccess::PixelShaderRead);

		for (const auto& sm : m_PLCDShadowMaps)
			cmd->TransitionLayout(sm, sm->GetLayout(), ImageReadAccess::PixelShaderRead);
		for (const auto& sm : m_SLCDShadowMaps)
			cmd->TransitionLayout(sm, sm->GetLayout(), ImageReadAccess::PixelShaderRead);
		for (const auto& sm : m_DLCDShadowMaps)
			cmd->TransitionLayout(sm, sm->GetLayout(), ImageReadAccess::PixelShaderRead);
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
			InitTranslucentSkeletalMeshPipelines();
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
					InitColoredDirectionalLightFramebuffers(m_DLCFramebuffers, m_TranslucentMDLPipeline);
				}
			}
		}
		else if (bTranslucencyShadowsEnabled && (bVolumetricChanged || bTranslucencyShadowsChanged))
		{
			if (m_DLShadowMaps[0] != RenderManager::GetDummyDepthImage()) // Init only if main DirLight shadows maps are initialized
				InitColoredDirectionalLightShadowMaps();
			
			if (m_DLCShadowMaps[0] != RenderManager::GetDummyImage())
			{
				InitColoredDirectionalLightFramebuffers(m_DLCFramebuffers, m_TranslucentMDLPipeline);
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

	void ShadowPassTask::ShadowPassOpaqueMeshes(const Ref<CommandBuffer>& cmd)
	{
		const auto& meshes = m_Renderer.GetCulledStaticMeshes();
		const auto& singleSided = meshes.SingleSided.BlendModes[uint32_t(MaterialBlendMode::Opaque)];
		const auto& doubleSided = meshes.DoubleSided.BlendModes[uint32_t(MaterialBlendMode::Opaque)];
		if (singleSided.GetNumMeshes() == 0 && doubleSided.GetNumMeshes() == 0)
			return;

		EG_GPU_TIMING_SCOPED(cmd, "Opacity Meshes shadow pass");
		EG_CPU_TIMING_SCOPED("Opacity Meshes shadow pass");

		const auto& buffers = m_Renderer.GetStaticMeshesBuffers();
		const auto& transformsBuffer = m_Renderer.GetMeshTransformsBuffer();
		const auto& lightMatrices = m_Renderer.GetLightMatrices();
		const auto& dirLight = m_Renderer.GetDirectionalLight();
		auto& stats = m_Renderer.GetStats();

		// For directional light
		if (m_Renderer.HasDirectionalLight() && dirLight.bCastsShadows)
		{
			EG_GPU_TIMING_SCOPED(cmd, "Opacity Meshes: CSM Shadow pass");
			EG_CPU_TIMING_SCOPED("Opacity Meshes: CSM Shadow pass");

			auto& pipeline = m_OpacityMDLPipeline;
			pipeline->SetBuffer(transformsBuffer, 0, 0);
			for (uint32_t i = 0; i < m_DLFramebuffers.size(); ++i)
			{
				const auto& viewProj = lightMatrices[dirLight.ViewProjOffset + i];
				RenderMeshesTask::DrawUnculledShadowCasters(cmd, pipeline, buffers, meshes, MaterialBlendMode::Opaque, stats, &viewProj, m_DLFramebuffers[i]);
			}
		}

		// For point lights
		if (!m_PointLightIndices.empty())
		{
			const auto& framebuffers = m_PLFramebuffers;
			const auto& pointLights = m_Renderer.GetCulledPointLights();
			{
				auto& vpsBuffer = m_PLVPsBuffer;
				auto& pipeline = m_OpacityMPLPipeline;
				pipeline->SetBuffer(transformsBuffer, 0, 0);
				pipeline->SetBuffer(vpsBuffer, 0, 1);
				{
					EG_GPU_TIMING_SCOPED(cmd, "Opacity Meshes: Point Lights Shadow pass");
					EG_CPU_TIMING_SCOPED("Opacity Meshes: Point Lights Shadow pass");

					for (size_t i = 0; i < m_PointLightIndices.size(); ++i)
					{
						const size_t index = m_PointLightIndices[i];
						const auto& pointLight = pointLights[index];
						EG_CORE_ASSERT(pointLight.ShadowMapIndex == i);

						RenderMeshesTask::DrawUnculledShadowCasters(cmd, pipeline, buffers, meshes, MaterialBlendMode::Opaque, stats, &i, framebuffers[i]);
					}
				}
			}
		}

		// For spot lights
		if (!m_SpotLightIndices.empty())
		{
			const auto& spotLights = m_Renderer.GetCulledSpotLights();
			auto& framebuffers = m_SLFramebuffers;
			{
				auto& pipeline = m_OpacityMSLPipeline;
				pipeline->SetBuffer(transformsBuffer, 0, 0);
				{
					EG_GPU_TIMING_SCOPED(cmd, "Opacity Meshes: Spot Lights Shadow pass");
					EG_CPU_TIMING_SCOPED("Opacity Meshes: Spot Lights Shadow pass");

					for (size_t i = 0; i < m_SpotLightIndices.size(); ++i)
					{
						const size_t index = m_SpotLightIndices[i];
						const auto& spotLight = spotLights[index];
						const auto& viewProj = lightMatrices[spotLight.ViewProjOffset];
						EG_CORE_ASSERT(spotLight.ShadowMapIndex == i);

						RenderMeshesTask::DrawUnculledShadowCasters(cmd, pipeline, buffers, meshes, MaterialBlendMode::Opaque, stats, &viewProj, framebuffers[i]);
					}
				}
			}
		}
	}

	void ShadowPassTask::ShadowPassTranslucentMeshes(const Ref<CommandBuffer>& cmd)
	{
		const auto& meshes = m_Renderer.GetCulledStaticMeshes();
		const auto& singleSided = meshes.SingleSided.BlendModes[uint32_t(MaterialBlendMode::Translucent)];
		const auto& doubleSided = meshes.DoubleSided.BlendModes[uint32_t(MaterialBlendMode::Translucent)];
		if (singleSided.GetNumMeshes() == 0 && doubleSided.GetNumMeshes() == 0)
			return;

		EG_GPU_TIMING_SCOPED(cmd, "Translucent Meshes shadow pass");
		EG_CPU_TIMING_SCOPED("Translucent Meshes shadow pass");

		const auto& buffers = m_Renderer.GetStaticMeshesBuffers();
		const auto& transformsBuffer = m_Renderer.GetMeshTransformsBuffer();
		const auto& lightMatrices = m_Renderer.GetLightMatrices();
		const auto& dirLight = m_Renderer.GetDirectionalLight();
		auto& stats = m_Renderer.GetStats();
		const uint32_t currentFrameIndex = RenderManager::GetCurrentFrameIndex();

		// For directional light
		if (m_Renderer.HasDirectionalLight() && dirLight.bCastsShadows)
		{
			EG_GPU_TIMING_SCOPED(cmd, "Translucent Meshes: CSM Shadow pass");
			EG_CPU_TIMING_SCOPED("Translucent Meshes: CSM Shadow pass");

			auto& pipeline = m_TranslucentMDLPipeline;
			auto& framebuffers = m_DLCFramebuffers;

			pipeline->SetBuffer(transformsBuffer, 1, 0);

			const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
			const bool bTexturesDirty = texturesChangedFrame >= m_TranslucentMeshesDLTexturesUpdatedFrames[currentFrameIndex];
			if (bTexturesDirty)
			{
				m_TranslucentMDLPipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
				m_TranslucentMeshesDLTexturesUpdatedFrames[currentFrameIndex] = texturesChangedFrame + 1;
			}
			pipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
			pipeline->SetBuffer(MaterialSystem::GetMaterialsRawBuffer(), EG_PERSISTENT_SET, EG_BINDING_RAW_MATERIALS);

			for (uint32_t i = 0; i < framebuffers.size(); ++i)
			{
				const auto& viewProj = lightMatrices[dirLight.ViewProjOffset + i];
				RenderMeshesTask::DrawUnculledShadowCasters(cmd, pipeline, buffers, meshes, MaterialBlendMode::Translucent, stats, &viewProj, framebuffers[i]);
			}
		}

		// For point lights
		if (!m_PointLightIndices.empty())
		{
			const auto& framebuffers = m_PLCFramebuffers;
			{
				auto& vpsBuffer = m_PLVPsBuffer;
				auto& pipeline = m_TranslucentMPLPipeline;
				pipeline->SetBuffer(transformsBuffer, 1, 0);
				pipeline->SetBuffer(vpsBuffer, 1, 1);

				const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
				const bool bTexturesDirty = texturesChangedFrame >= m_TranslucentMeshesPLTexturesUpdatedFrames[currentFrameIndex];
				if (bTexturesDirty)
				{
					m_TranslucentMPLPipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
					m_TranslucentMeshesPLTexturesUpdatedFrames[currentFrameIndex] = texturesChangedFrame + 1;
				}
				pipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
				pipeline->SetBuffer(MaterialSystem::GetMaterialsRawBuffer(), EG_PERSISTENT_SET, EG_BINDING_RAW_MATERIALS);

				{
					EG_GPU_TIMING_SCOPED(cmd, "Translucent Meshes: Point Lights Shadow pass");
					EG_CPU_TIMING_SCOPED("Translucent Meshes: Point Lights Shadow pass");

					for (size_t i = 0; i < m_PointLightIndices.size(); ++i)
					{
						RenderMeshesTask::DrawUnculledShadowCasters(cmd, pipeline, buffers, meshes, MaterialBlendMode::Translucent, stats, &i, framebuffers[i]);
					}
				}
			}
		}

		// For spot lights
		if (!m_SpotLightIndices.empty())
		{
			const auto& spotLights = m_Renderer.GetCulledSpotLights();
			const auto& framebuffers = m_SLCFramebuffers;

			{
				auto& pipeline = m_TranslucentMSLPipeline;
				pipeline->SetBuffer(transformsBuffer, 1, 0);

				const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
				const bool bTexturesDirty = texturesChangedFrame >= m_TranslucentMeshesSLTexturesUpdatedFrames[currentFrameIndex];
				if (bTexturesDirty)
				{
					m_TranslucentMSLPipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
					m_TranslucentMeshesSLTexturesUpdatedFrames[currentFrameIndex] = texturesChangedFrame + 1;
				}
				pipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
				pipeline->SetBuffer(MaterialSystem::GetMaterialsRawBuffer(), EG_PERSISTENT_SET, EG_BINDING_RAW_MATERIALS);

				{
					EG_GPU_TIMING_SCOPED(cmd, "Translucent Meshes: Spot Lights Shadow pass");
					EG_CPU_TIMING_SCOPED("Translucent Meshes: Spot Lights Shadow pass");

					for (size_t i = 0; i < m_SpotLightIndices.size(); ++i)
					{
						const size_t index = m_SpotLightIndices[i];
						const auto& spotLight = spotLights[index];
						const auto& viewProj = lightMatrices[spotLight.ViewProjOffset];
						RenderMeshesTask::DrawUnculledShadowCasters(cmd, pipeline, buffers, meshes, MaterialBlendMode::Translucent, stats, &viewProj, framebuffers[i]);
					}
				}
			}
		}
	}
	
	void ShadowPassTask::ShadowPassMaskedMeshes(const Ref<CommandBuffer>& cmd)
	{
		const auto& meshes = m_Renderer.GetCulledStaticMeshes();
		const auto& singleSided = meshes.SingleSided.BlendModes[uint32_t(MaterialBlendMode::Masked)];
		const auto& doubleSided = meshes.DoubleSided.BlendModes[uint32_t(MaterialBlendMode::Masked)];
		if (singleSided.GetNumMeshes() == 0 && doubleSided.GetNumMeshes() == 0)
			return;

		EG_GPU_TIMING_SCOPED(cmd, "Masked Meshes shadow pass");
		EG_CPU_TIMING_SCOPED("Masked Meshes shadow pass");

		const auto& buffers = m_Renderer.GetStaticMeshesBuffers();
		const auto& transformsBuffer = m_Renderer.GetMeshTransformsBuffer();
		const auto& lightMatrices = m_Renderer.GetLightMatrices();
		const auto& dirLight = m_Renderer.GetDirectionalLight();
		auto& stats = m_Renderer.GetStats();

		const uint32_t currentFrameIndex = RenderManager::GetCurrentFrameIndex();

		// For directional light
		if (m_Renderer.HasDirectionalLight() && dirLight.bCastsShadows)
		{
			EG_GPU_TIMING_SCOPED(cmd, "Masked Meshes: CSM Shadow pass");
			EG_CPU_TIMING_SCOPED("Masked Meshes: CSM Shadow pass");

			auto& pipeline = m_MaskedMDLPipeline;
			pipeline->SetBuffer(transformsBuffer, 1, 0);

			const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
			const bool bTexturesDirty = texturesChangedFrame >= m_MaskedMeshesDLTexturesUpdatedFrames[currentFrameIndex];
			if (bTexturesDirty)
			{
				m_MaskedMDLPipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
				m_MaskedMeshesDLTexturesUpdatedFrames[currentFrameIndex] = texturesChangedFrame + 1;
			}
			pipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
			pipeline->SetBuffer(MaterialSystem::GetMaterialsRawBuffer(), EG_PERSISTENT_SET, EG_BINDING_RAW_MATERIALS);

			for (uint32_t i = 0; i < m_DLFramebuffers.size(); ++i)
			{
				const auto& viewProj = lightMatrices[dirLight.ViewProjOffset + i];
				RenderMeshesTask::DrawUnculledShadowCasters(cmd, pipeline, buffers, meshes, MaterialBlendMode::Masked, stats, &viewProj, m_DLFramebuffers[i]);
			}
		}

		// For point lights
		if (!m_PointLightIndices.empty())
		{
			auto& framebuffers = m_PLFramebuffers;
			{
				auto& vpsBuffer = m_PLVPsBuffer;
				auto& pipeline = m_MaskedMPLPipeline;
				pipeline->SetBuffer(transformsBuffer, 1, 0);
				pipeline->SetBuffer(vpsBuffer, 1, 1);

				const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
				const bool bTexturesDirty = texturesChangedFrame >= m_MaskedMeshesPLTexturesUpdatedFrames[currentFrameIndex];
				if (bTexturesDirty)
				{
					m_MaskedMPLPipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
					m_MaskedMeshesPLTexturesUpdatedFrames[currentFrameIndex] = texturesChangedFrame + 1;
				}
				pipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
				pipeline->SetBuffer(MaterialSystem::GetMaterialsRawBuffer(), EG_PERSISTENT_SET, EG_BINDING_RAW_MATERIALS);

				{
					EG_GPU_TIMING_SCOPED(cmd, "Masked Meshes: Point Lights Shadow pass");
					EG_CPU_TIMING_SCOPED("Masked Meshes: Point Lights Shadow pass");

					for (size_t i = 0; i < m_PointLightIndices.size(); ++i)
					{
						RenderMeshesTask::DrawUnculledShadowCasters(cmd, pipeline, buffers, meshes, MaterialBlendMode::Masked, stats, &i, framebuffers[i]);
					}
				}
			}
		}

		// For spot lights
		if (!m_SpotLightIndices.empty())
		{
			const auto& spotLights = m_Renderer.GetCulledSpotLights();
			auto& framebuffers = m_SLFramebuffers;
			{
				auto& pipeline = m_MaskedMSLPipeline;
				pipeline->SetBuffer(transformsBuffer, 1, 0);

				const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
				const bool bTexturesDirty = texturesChangedFrame >= m_MaskedMeshesSLTexturesUpdatedFrames[currentFrameIndex];
				if (bTexturesDirty)
				{
					m_MaskedMSLPipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
					m_MaskedMeshesSLTexturesUpdatedFrames[currentFrameIndex] = texturesChangedFrame + 1;
				}
				pipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
				pipeline->SetBuffer(MaterialSystem::GetMaterialsRawBuffer(), EG_PERSISTENT_SET, EG_BINDING_RAW_MATERIALS);

				{
					EG_GPU_TIMING_SCOPED(cmd, "Masked Meshes: Spot Lights Shadow pass");
					EG_CPU_TIMING_SCOPED("Masked Meshes: Spot Lights Shadow pass");

					for (size_t i = 0; i < m_SpotLightIndices.size(); ++i)
					{
						const size_t index = m_SpotLightIndices[i];
						const auto& spotLight = spotLights[index];
						const auto& viewProj = lightMatrices[spotLight.ViewProjOffset];
						RenderMeshesTask::DrawUnculledShadowCasters(cmd, pipeline, buffers, meshes, MaterialBlendMode::Masked, stats, &viewProj, framebuffers[i]);
					}
				}
			}
		}
	}
	
	void ShadowPassTask::ShadowPassOpaqueSkeletalMeshes(const Ref<CommandBuffer>& cmd)
	{
		const auto& meshes = m_Renderer.GetCulledSkeletalMeshes();
		const auto& ivb = meshes.UnculledInstanceBuffer;
		const auto& singleSided = meshes.SingleSided.BlendModes[uint32_t(MaterialBlendMode::Opaque)];
		const auto& doubleSided = meshes.DoubleSided.BlendModes[uint32_t(MaterialBlendMode::Opaque)];
		if (singleSided.GetNumMeshes() == 0 && doubleSided.GetNumMeshes() == 0)
			return;

		EG_GPU_TIMING_SCOPED(cmd, "Opacity Skeletal Meshes shadow pass");
		EG_CPU_TIMING_SCOPED("Opacity Skeletal Meshes shadow pass");

		const auto& skinnedVertices = m_Renderer.GetSkinnedVertices();
		const auto& buffers = m_Renderer.GetSkeletalMeshesBuffers();
		const auto& lightMatrices = m_Renderer.GetLightMatrices();
		const auto& dirLight = m_Renderer.GetDirectionalLight();
		auto& stats = m_Renderer.GetStats();

		// For directional light
		if (m_Renderer.HasDirectionalLight() && dirLight.bCastsShadows)
		{
			EG_GPU_TIMING_SCOPED(cmd, "Opacity Skeletal Meshes: CSM Shadow pass");
			EG_CPU_TIMING_SCOPED("Opacity Skeletal Meshes: CSM Shadow pass");
			
			auto& pipeline = m_OpacitySMDLPipeline;
			pipeline->SetBuffer(skinnedVertices, 0, 0);
			pipeline->SetBuffer(ivb, 0, 1);
			for (uint32_t i = 0; i < m_DLFramebuffers.size(); ++i)
			{
				const auto& viewProj = lightMatrices[dirLight.ViewProjOffset + i];
				RenderSkeletalMeshesTask::DrawUnculledShadowCasters(cmd, pipeline, buffers, meshes, MaterialBlendMode::Opaque, stats, &viewProj, m_DLFramebuffers[i]);
			}
		}

		// For point lights
		if (!m_PointLightIndices.empty())
		{
			const auto& framebuffers = m_PLFramebuffers;
			{
				auto& vpsBuffer = m_PLVPsBuffer;
				auto& pipeline = m_OpacitySMPLPipeline;
				pipeline->SetBuffer(skinnedVertices, 0, 0);
				pipeline->SetBuffer(ivb, 0, 1);
				pipeline->SetBuffer(vpsBuffer, 0, 2);
				{
					EG_GPU_TIMING_SCOPED(cmd, "Opacity Skeletal Meshes: Point Lights Shadow pass");
					EG_CPU_TIMING_SCOPED("Opacity Skeletal Meshes: Point Lights Shadow pass");

					for (size_t i = 0; i < m_PointLightIndices.size(); ++i)
					{
						RenderSkeletalMeshesTask::DrawUnculledShadowCasters(cmd, pipeline, buffers, meshes, MaterialBlendMode::Opaque, stats, &i, framebuffers[i]);
					}
				}
			}
		}

		// For spot lights
		if (!m_SpotLightIndices.empty())
		{
			const auto& spotLights = m_Renderer.GetCulledSpotLights();
			auto& framebuffers = m_SLFramebuffers;
			{
				auto& pipeline = m_OpacitySMSLPipeline;
				pipeline->SetBuffer(skinnedVertices, 0, 0);
				pipeline->SetBuffer(ivb, 0, 1);
				{
					EG_GPU_TIMING_SCOPED(cmd, "Opacity Skeletal Meshes: Spot Lights Shadow pass");
					EG_CPU_TIMING_SCOPED("Opacity Skeletal Meshes: Spot Lights Shadow pass");

					for (size_t i = 0; i < m_SpotLightIndices.size(); ++i)
					{
						const size_t index = m_SpotLightIndices[i];
						const auto& spotLight = spotLights[index];
						const auto& viewProj = lightMatrices[spotLight.ViewProjOffset];
						RenderSkeletalMeshesTask::DrawUnculledShadowCasters(cmd, pipeline, buffers, meshes, MaterialBlendMode::Opaque, stats, &viewProj, framebuffers[i]);
					}
				}
			}
		}
	}

	void ShadowPassTask::ShadowPassTranslucentSkeletalMeshes(const Ref<CommandBuffer>& cmd)
	{
		const auto& meshes = m_Renderer.GetCulledSkeletalMeshes();
		const auto& ivb = meshes.UnculledInstanceBuffer;
		const auto& singleSided = meshes.SingleSided.BlendModes[uint32_t(MaterialBlendMode::Translucent)];
		const auto& doubleSided = meshes.DoubleSided.BlendModes[uint32_t(MaterialBlendMode::Translucent)];
		if (singleSided.GetNumMeshes() == 0 && doubleSided.GetNumMeshes() == 0)
			return;

		EG_GPU_TIMING_SCOPED(cmd, "Translucent Skeletal Meshes shadow pass");
		EG_CPU_TIMING_SCOPED("Translucent Skeletal Meshes shadow pass");

		const auto& skinnedVertices = m_Renderer.GetSkinnedVertices();
		const auto& buffers = m_Renderer.GetSkeletalMeshesBuffers();
		const auto& lightMatrices = m_Renderer.GetLightMatrices();
		const auto& dirLight = m_Renderer.GetDirectionalLight();
		auto& stats = m_Renderer.GetStats();
		const uint32_t currentFrameIndex = RenderManager::GetCurrentFrameIndex();

		// For directional light
		if (m_Renderer.HasDirectionalLight() && dirLight.bCastsShadows)
		{
			EG_GPU_TIMING_SCOPED(cmd, "Translucent Skeletal Meshes: CSM Shadow pass");
			EG_CPU_TIMING_SCOPED("Translucent Skeletal Meshes: CSM Shadow pass");

			auto& pipeline = m_TranslucentSMDLPipeline;
			auto& framebuffers = m_DLCFramebuffers;

			pipeline->SetBuffer(skinnedVertices, 1, 0);
			pipeline->SetBuffer(ivb, 1, 1);

			const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
			const bool bTexturesDirty = texturesChangedFrame >= m_TranslucentSkeletalMeshesDLTexturesUpdatedFrames[currentFrameIndex];
			if (bTexturesDirty)
			{
				m_TranslucentSMDLPipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
				m_TranslucentSkeletalMeshesDLTexturesUpdatedFrames[currentFrameIndex] = texturesChangedFrame + 1;
			}
			pipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
			pipeline->SetBuffer(MaterialSystem::GetMaterialsRawBuffer(), EG_PERSISTENT_SET, EG_BINDING_RAW_MATERIALS);

			for (uint32_t i = 0; i < framebuffers.size(); ++i)
			{
				const auto& viewProj = lightMatrices[dirLight.ViewProjOffset + i];
				RenderSkeletalMeshesTask::DrawUnculledShadowCasters(cmd, pipeline, buffers, meshes, MaterialBlendMode::Translucent, stats, &viewProj, framebuffers[i]);
			}
		}

		// For point lights
		if (!m_PointLightIndices.empty())
		{
			const auto& framebuffers = m_PLCFramebuffers;
			{
				auto& vpsBuffer = m_PLVPsBuffer;
				auto& pipeline = m_TranslucentSMPLPipeline;
				pipeline->SetBuffer(skinnedVertices, 1, 0);
				pipeline->SetBuffer(ivb, 1, 1);
				pipeline->SetBuffer(vpsBuffer, 1, 2);

				const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
				const bool bTexturesDirty = texturesChangedFrame >= m_TranslucentSkeletalMeshesPLTexturesUpdatedFrames[currentFrameIndex];
				if (bTexturesDirty)
				{
					m_TranslucentSMPLPipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
					m_TranslucentSkeletalMeshesDLTexturesUpdatedFrames[currentFrameIndex] = texturesChangedFrame + 1;
				}
				pipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
				pipeline->SetBuffer(MaterialSystem::GetMaterialsRawBuffer(), EG_PERSISTENT_SET, EG_BINDING_RAW_MATERIALS);

				{
					EG_GPU_TIMING_SCOPED(cmd, "Translucent Skeletal Meshes: Point Lights Shadow pass");
					EG_CPU_TIMING_SCOPED("Translucent Skeletal Meshes: Point Lights Shadow pass");

					for (size_t i = 0; i < m_PointLightIndices.size(); ++i)
					{
						RenderSkeletalMeshesTask::DrawUnculledShadowCasters(cmd, pipeline, buffers, meshes, MaterialBlendMode::Translucent, stats, &i, framebuffers[i]);
					}
				}
			}
		}

		// For spot lights
		if (!m_SpotLightIndices.empty())
		{
			const auto& spotLights = m_Renderer.GetCulledSpotLights();
			const auto& framebuffers = m_SLCFramebuffers;

			{
				auto& pipeline = m_TranslucentSMSLPipeline;
				pipeline->SetBuffer(skinnedVertices, 1, 0);
				pipeline->SetBuffer(ivb, 1, 1);

				const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
				const bool bTexturesDirty = texturesChangedFrame >= m_TranslucentSkeletalMeshesSLTexturesUpdatedFrames[currentFrameIndex];
				if (bTexturesDirty)
				{
					m_TranslucentSMSLPipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
					m_TranslucentSkeletalMeshesSLTexturesUpdatedFrames[currentFrameIndex] = texturesChangedFrame + 1;
				}
				pipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
				pipeline->SetBuffer(MaterialSystem::GetMaterialsRawBuffer(), EG_PERSISTENT_SET, EG_BINDING_RAW_MATERIALS);

				{
					EG_GPU_TIMING_SCOPED(cmd, "Translucent Skeletal Meshes: Spot Lights Shadow pass");
					EG_CPU_TIMING_SCOPED("Translucent Skeletal Meshes: Spot Lights Shadow pass");

					for (size_t i = 0; i < m_SpotLightIndices.size(); ++i)
					{
						const size_t index = m_SpotLightIndices[i];
						const auto& spotLight = spotLights[index];
						const auto& viewProj = lightMatrices[spotLight.ViewProjOffset];
						RenderSkeletalMeshesTask::DrawUnculledShadowCasters(cmd, pipeline, buffers, meshes, MaterialBlendMode::Translucent, stats, &viewProj, framebuffers[i]);
					}
				}
			}
		}
	}
	
	void ShadowPassTask::ShadowPassMaskedSkeletalMeshes(const Ref<CommandBuffer>& cmd)
	{
		const auto& meshes = m_Renderer.GetCulledSkeletalMeshes();
		const auto& ivb = meshes.UnculledInstanceBuffer;
		const auto& singleSided = meshes.SingleSided.BlendModes[uint32_t(MaterialBlendMode::Masked)];
		const auto& doubleSided = meshes.DoubleSided.BlendModes[uint32_t(MaterialBlendMode::Masked)];
		if (singleSided.GetNumMeshes() == 0 && doubleSided.GetNumMeshes() == 0)
			return;

		EG_GPU_TIMING_SCOPED(cmd, "Masked Skeletal Meshes shadow pass");
		EG_CPU_TIMING_SCOPED("Masked Skeletal Meshes shadow pass");

		const auto& skinnedVertices = m_Renderer.GetSkinnedVertices();
		const auto& buffers = m_Renderer.GetSkeletalMeshesBuffers();
		const auto& lightMatrices = m_Renderer.GetLightMatrices();
		const auto& dirLight = m_Renderer.GetDirectionalLight();
		auto& stats = m_Renderer.GetStats();

		const uint32_t currentFrameIndex = RenderManager::GetCurrentFrameIndex();

		// For directional light
		if (m_Renderer.HasDirectionalLight() && dirLight.bCastsShadows)
		{
			EG_GPU_TIMING_SCOPED(cmd, "Masked Skeletal Meshes: CSM Shadow pass");
			EG_CPU_TIMING_SCOPED("Masked Skeletal Meshes: CSM Shadow pass");

			auto& pipeline = m_MaskedSMDLPipeline;

			pipeline->SetBuffer(skinnedVertices, 1, 0);
			pipeline->SetBuffer(ivb, 1, 1);

			const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
			const bool bTexturesDirty = texturesChangedFrame >= m_MaskedSkeletalMeshesDLTexturesUpdatedFrames[currentFrameIndex];
			if (bTexturesDirty)
			{
				m_MaskedSMDLPipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
				m_MaskedSkeletalMeshesDLTexturesUpdatedFrames[currentFrameIndex] = texturesChangedFrame + 1;
			}
			pipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
			pipeline->SetBuffer(MaterialSystem::GetMaterialsRawBuffer(), EG_PERSISTENT_SET, EG_BINDING_RAW_MATERIALS);

			for (uint32_t i = 0; i < m_DLFramebuffers.size(); ++i)
			{
				const auto& viewProj = lightMatrices[dirLight.ViewProjOffset + i];
				RenderSkeletalMeshesTask::DrawUnculledShadowCasters(cmd, pipeline, buffers, meshes, MaterialBlendMode::Masked, stats, &viewProj, m_DLFramebuffers[i]);
			}
		}

		// For point lights
		if (!m_PointLightIndices.empty())
		{
			auto& framebuffers = m_PLFramebuffers;

			{
				auto& vpsBuffer = m_PLVPsBuffer;
				auto& pipeline = m_MaskedSMPLPipeline;
				pipeline->SetBuffer(skinnedVertices, 1, 0);
				pipeline->SetBuffer(ivb, 1, 1);
				pipeline->SetBuffer(vpsBuffer, 1, 2);

				const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
				const bool bTexturesDirty = texturesChangedFrame >= m_MaskedSkeletalMeshesPLTexturesUpdatedFrames[currentFrameIndex];
				if (bTexturesDirty)
				{
					m_MaskedSMPLPipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
					m_MaskedSkeletalMeshesPLTexturesUpdatedFrames[currentFrameIndex] = texturesChangedFrame + 1;
				}
				pipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
				pipeline->SetBuffer(MaterialSystem::GetMaterialsRawBuffer(), EG_PERSISTENT_SET, EG_BINDING_RAW_MATERIALS);

				{
					EG_GPU_TIMING_SCOPED(cmd, "Masked Skeletal Meshes: Point Lights Shadow pass");
					EG_CPU_TIMING_SCOPED("Masked Skeletal Meshes: Point Lights Shadow pass");

					for (size_t i = 0; i < m_PointLightIndices.size(); ++i)
					{
						RenderSkeletalMeshesTask::DrawUnculledShadowCasters(cmd, pipeline, buffers, meshes, MaterialBlendMode::Masked, stats, &i, framebuffers[i]);
					}
				}
			}
		}

		// For spot lights
		if (!m_SpotLightIndices.empty())
		{
			const auto& spotLights = m_Renderer.GetCulledSpotLights();
			auto& framebuffers = m_SLFramebuffers;
			{
				auto& pipeline = m_MaskedSMSLPipeline;
				pipeline->SetBuffer(skinnedVertices, 1, 0);
				pipeline->SetBuffer(ivb, 1, 1);

				const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
				const bool bTexturesDirty = texturesChangedFrame >= m_MaskedSkeletalMeshesSLTexturesUpdatedFrames[currentFrameIndex];
				if (bTexturesDirty)
				{
					m_MaskedSMSLPipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
					m_MaskedSkeletalMeshesSLTexturesUpdatedFrames[currentFrameIndex] = texturesChangedFrame + 1;
				}
				pipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
				pipeline->SetBuffer(MaterialSystem::GetMaterialsRawBuffer(), EG_PERSISTENT_SET, EG_BINDING_RAW_MATERIALS);

				{
					EG_GPU_TIMING_SCOPED(cmd, "Masked Skeletal Meshes: Spot Lights Shadow pass");
					EG_CPU_TIMING_SCOPED("Masked Skeletal Meshes: Spot Lights Shadow pass");

					for (size_t i = 0; i < m_SpotLightIndices.size(); ++i)
					{
						const size_t index = m_SpotLightIndices[i];
						const auto& spotLight = spotLights[index];
						const auto& viewProj = lightMatrices[spotLight.ViewProjOffset];
						RenderSkeletalMeshesTask::DrawUnculledShadowCasters(cmd, pipeline, buffers, meshes, MaterialBlendMode::Masked, stats, &viewProj, framebuffers[i]);
					}
				}
			}
		}
	}
	
	void ShadowPassTask::ShadowPassOpaqueSprites(const Ref<CommandBuffer>& cmd)
	{
		const auto& singleSided = m_Renderer.GetSingleSidedSpritesRenderData();
		const auto& doubleSided = m_Renderer.GetDoubleSidedSpritesRenderData();

		if (singleSided.Opaque.IsEmpty() && doubleSided.Opaque.IsEmpty())
			return;

		EG_GPU_TIMING_SCOPED(cmd, "Opacity Sprites shadow pass");
		EG_CPU_TIMING_SCOPED("Opacity Sprites shadow pass");

		const auto& transformsBuffer = m_Renderer.GetSpritesTransformsBuffer();
		const auto& lightMatrices = m_Renderer.GetLightMatrices();
		auto& stats = m_Renderer.GetStats();

		// For directional light
		const auto& dirLight = m_Renderer.GetDirectionalLight();
		if (m_Renderer.HasDirectionalLight() && dirLight.bCastsShadows)
		{
			EG_GPU_TIMING_SCOPED(cmd, "Opacity Sprites: CSM Shadow pass");
			EG_CPU_TIMING_SCOPED("Opacity Sprites: CSM Shadow pass");

			auto& pipeline = m_OpacitySDLPipeline;
			pipeline->SetBuffer(transformsBuffer, 0, 0);
			for (uint32_t i = 0; i < m_DLFramebuffers.size(); ++i)
			{
				const auto& viewProj = lightMatrices[dirLight.ViewProjOffset + i];
				cmd->SetGraphicsCullMode(CullMode::Front);
				RenderSpritesTask::Draw(cmd, pipeline, singleSided.Opaque.ShadowCastingQuads, &viewProj, stats, m_DLFramebuffers[i]);
				cmd->SetGraphicsCullMode(CullMode::None);
				RenderSpritesTask::Draw(cmd, pipeline, doubleSided.Opaque.ShadowCastingQuads, &viewProj, stats, m_DLFramebuffers[i]);
			}
		}

		// Point lights
		if (m_PointLightIndices.size())
		{
			EG_GPU_TIMING_SCOPED(cmd, "Opacity Sprites: Point Lights Shadow pass");
			EG_CPU_TIMING_SCOPED("Opacity Sprites: Point Lights Shadow pass");

			auto& framebuffers = m_PLFramebuffers;
			auto& vpsBuffer = m_PLVPsBuffer;
			auto& pipeline = m_OpacitySPLPipeline;
			pipeline->SetBuffer(transformsBuffer, 0, 0);
			pipeline->SetBuffer(vpsBuffer, 0, 1);

			for (size_t i = 0; i < m_PointLightIndices.size(); ++i)
			{
				cmd->SetGraphicsCullMode(CullMode::Front);
				RenderSpritesTask::Draw(cmd, pipeline, singleSided.Opaque.ShadowCastingQuads, &i, stats, framebuffers[i]);
				cmd->SetGraphicsCullMode(CullMode::None);
				RenderSpritesTask::Draw(cmd, pipeline, doubleSided.Opaque.ShadowCastingQuads, &i, stats, framebuffers[i]);
			}
		}

		// Spot lights
		if (m_SpotLightIndices.size())
		{
			EG_GPU_TIMING_SCOPED(cmd, "Opacity Sprites: Spot Lights Shadow pass");
			EG_CPU_TIMING_SCOPED("Opacity Sprites: Spot Lights Shadow pass");

			const auto& spotLights = m_Renderer.GetCulledSpotLights();
			auto& framebuffers = m_SLFramebuffers;

			auto& pipeline = m_OpacitySSLPipeline;
			pipeline->SetBuffer(transformsBuffer, 0, 0);

			for (size_t i = 0; i < m_SpotLightIndices.size(); ++i)
			{
				const size_t index = m_SpotLightIndices[i];
				const auto& spotLight = spotLights[index];
				const auto& viewProj = lightMatrices[spotLight.ViewProjOffset];

				cmd->SetGraphicsCullMode(CullMode::Front);
				RenderSpritesTask::Draw(cmd, pipeline, singleSided.Opaque.ShadowCastingQuads, &viewProj, stats, framebuffers[i]);
				cmd->SetGraphicsCullMode(CullMode::None);
				RenderSpritesTask::Draw(cmd, pipeline, doubleSided.Opaque.ShadowCastingQuads, &viewProj, stats, framebuffers[i]);
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
		const auto& lightMatrices = m_Renderer.GetLightMatrices();
		auto& stats = m_Renderer.GetStats();

		const uint32_t currentFrameIndex = RenderManager::GetCurrentFrameIndex();

		// For directional light
		const auto& dirLight = m_Renderer.GetDirectionalLight();
		if (m_Renderer.HasDirectionalLight() && dirLight.bCastsShadows)
		{
			EG_GPU_TIMING_SCOPED(cmd, "Translucent Sprites: CSM Shadow pass");
			EG_CPU_TIMING_SCOPED("Translucent Sprites: CSM Shadow pass");

			auto& pipeline = m_TranslucentSDLPipeline;
			auto& framebuffers = m_DLCFramebuffers;

			const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
			const bool bTexturesDirty = texturesChangedFrame >= m_TranslucentSpritesDLTexturesUpdatedFrames[currentFrameIndex];
			if (bTexturesDirty)
			{
				m_TranslucentSDLPipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
				m_TranslucentSpritesDLTexturesUpdatedFrames[currentFrameIndex] = texturesChangedFrame + 1;
			}
			pipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
			pipeline->SetBuffer(MaterialSystem::GetMaterialsRawBuffer(), EG_PERSISTENT_SET, EG_BINDING_RAW_MATERIALS);
			pipeline->SetBuffer(transformsBuffer, 1, 0);

			for (uint32_t i = 0; i < framebuffers.size(); ++i)
			{
				const auto& viewProj = lightMatrices[dirLight.ViewProjOffset + i];
				cmd->SetGraphicsCullMode(CullMode::Front);
				RenderSpritesTask::Draw(cmd, pipeline, singleSided.Translucent.ShadowCastingQuads, &viewProj, stats, framebuffers[i]);
				cmd->SetGraphicsCullMode(CullMode::None);
				RenderSpritesTask::Draw(cmd, pipeline, doubleSided.Translucent.ShadowCastingQuads, &viewProj, stats, framebuffers[i]);
			}
		}

		// Point lights
		{
			const auto& framebuffers = m_PLCFramebuffers;

			if (m_PointLightIndices.size())
			{
				EG_GPU_TIMING_SCOPED(cmd, "Translucent Sprites: Point Lights Shadow pass");
				EG_CPU_TIMING_SCOPED("Translucent Sprites: Point Lights Shadow pass");

				auto& vpsBuffer = m_PLVPsBuffer;
				auto& pipeline = m_TranslucentSPLPipeline;

				const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
				const bool bTexturesDirty = texturesChangedFrame >= m_TranslucentSpritesPLTexturesUpdatedFrames[currentFrameIndex];
				if (bTexturesDirty)
				{
					m_TranslucentSPLPipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
					m_TranslucentSpritesPLTexturesUpdatedFrames[currentFrameIndex] = texturesChangedFrame + 1;
				}
				pipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
				pipeline->SetBuffer(MaterialSystem::GetMaterialsRawBuffer(), EG_PERSISTENT_SET, EG_BINDING_RAW_MATERIALS);

				pipeline->SetBuffer(transformsBuffer, 1, 0);
				pipeline->SetBuffer(vpsBuffer, 1, 1);

				for (size_t i = 0; i < m_PointLightIndices.size(); ++i)
				{
					cmd->SetGraphicsCullMode(CullMode::Front);
					RenderSpritesTask::Draw(cmd, pipeline, singleSided.Translucent.ShadowCastingQuads, &i, stats, framebuffers[i]);
					cmd->SetGraphicsCullMode(CullMode::None);
					RenderSpritesTask::Draw(cmd, pipeline, doubleSided.Translucent.ShadowCastingQuads, &i, stats, framebuffers[i]);
				}
			}
		}

		// Spot lights
		{
			const auto& framebuffers = m_SLCFramebuffers;

			if (m_SpotLightIndices.size())
			{
				EG_GPU_TIMING_SCOPED(cmd, "Translucent Sprites: Spot Lights Shadow pass");
				EG_CPU_TIMING_SCOPED("Translucent Sprites: Spot Lights Shadow pass");

				auto& spotLights = m_Renderer.GetCulledSpotLights();
				auto& pipeline = m_TranslucentSSLPipeline;

				const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
				const bool bTexturesDirty = texturesChangedFrame >= m_TranslucentSpritesSLTexturesUpdatedFrames[currentFrameIndex];
				if (bTexturesDirty)
				{
					m_TranslucentSSLPipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
					m_TranslucentSpritesSLTexturesUpdatedFrames[currentFrameIndex] = texturesChangedFrame + 1;
				}
				pipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
				pipeline->SetBuffer(MaterialSystem::GetMaterialsRawBuffer(), EG_PERSISTENT_SET, EG_BINDING_RAW_MATERIALS);

				pipeline->SetBuffer(transformsBuffer, 1, 0);

				for (size_t i = 0; i < m_SpotLightIndices.size(); ++i)
				{
					const size_t index = m_SpotLightIndices[i];
					const auto& spotLight = spotLights[index];
					const auto& viewProj = lightMatrices[spotLight.ViewProjOffset];

					cmd->SetGraphicsCullMode(CullMode::Front);
					RenderSpritesTask::Draw(cmd, pipeline, singleSided.Translucent.ShadowCastingQuads, &viewProj, stats, framebuffers[i]);
					cmd->SetGraphicsCullMode(CullMode::None);
					RenderSpritesTask::Draw(cmd, pipeline, doubleSided.Translucent.ShadowCastingQuads, &viewProj, stats, framebuffers[i]);
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
		const auto& lightMatrices = m_Renderer.GetLightMatrices();
		auto& stats = m_Renderer.GetStats();

		const uint32_t currentFrameIndex = RenderManager::GetCurrentFrameIndex();

		// For directional light
		const auto& dirLight = m_Renderer.GetDirectionalLight();
		if (m_Renderer.HasDirectionalLight() && dirLight.bCastsShadows)
		{
			EG_GPU_TIMING_SCOPED(cmd, "Masked Sprites: CSM Shadow pass");
			EG_CPU_TIMING_SCOPED("Masked Sprites: CSM Shadow pass");

			auto& pipeline = m_MaskedSDLPipeline;
			const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
			const bool bTexturesDirty = texturesChangedFrame >= m_MaskedSpritesDLTexturesUpdatedFrames[currentFrameIndex];
			if (bTexturesDirty)
			{
				m_MaskedSDLPipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
				m_MaskedSpritesDLTexturesUpdatedFrames[currentFrameIndex] = texturesChangedFrame + 1;
			}
			pipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
			pipeline->SetBuffer(MaterialSystem::GetMaterialsRawBuffer(), EG_PERSISTENT_SET, EG_BINDING_RAW_MATERIALS);

			pipeline->SetBuffer(transformsBuffer, 1, 0);
			for (uint32_t i = 0; i < m_DLFramebuffers.size(); ++i)
			{
				const auto& viewProj = lightMatrices[dirLight.ViewProjOffset + i];
				cmd->SetGraphicsCullMode(CullMode::Front);
				RenderSpritesTask::Draw(cmd, pipeline, singleSided.Masked.ShadowCastingQuads, &viewProj, stats, m_DLFramebuffers[i]);
				cmd->SetGraphicsCullMode(CullMode::None);
				RenderSpritesTask::Draw(cmd, pipeline, doubleSided.Masked.ShadowCastingQuads, &viewProj, stats, m_DLFramebuffers[i]);
			}
		}

		// Point lights
		if (m_PointLightIndices.size())
		{
			EG_GPU_TIMING_SCOPED(cmd, "Masked Sprites: Point Lights Shadow pass");
			EG_CPU_TIMING_SCOPED("Masked Sprites: Point Lights Shadow pass");

			auto& framebuffers = m_PLFramebuffers;
			auto& vpsBuffer = m_PLVPsBuffer;
			auto& pipeline = m_MaskedSPLPipeline;

			const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
			const bool bTexturesDirty = texturesChangedFrame >= m_MaskedSpritesPLTexturesUpdatedFrames[currentFrameIndex];
			if (bTexturesDirty)
			{
				m_MaskedSPLPipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
				m_MaskedSpritesPLTexturesUpdatedFrames[currentFrameIndex] = texturesChangedFrame + 1;
			}
			pipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
			pipeline->SetBuffer(MaterialSystem::GetMaterialsRawBuffer(), EG_PERSISTENT_SET, EG_BINDING_RAW_MATERIALS);

			pipeline->SetBuffer(transformsBuffer, 1, 0);
			pipeline->SetBuffer(vpsBuffer, 1, 1);

			for (size_t i = 0; i < m_PointLightIndices.size(); ++i)
			{
				cmd->SetGraphicsCullMode(CullMode::Front);
				RenderSpritesTask::Draw(cmd, pipeline, singleSided.Opaque.ShadowCastingQuads, &i, stats, framebuffers[i]);
				cmd->SetGraphicsCullMode(CullMode::None);
				RenderSpritesTask::Draw(cmd, pipeline, doubleSided.Opaque.ShadowCastingQuads, &i, stats, framebuffers[i]);
			}
		}

		// Spot lights
		if (m_SpotLightIndices.size())
		{
			EG_GPU_TIMING_SCOPED(cmd, "Masked Sprites: Spot Lights Shadow pass");
			EG_CPU_TIMING_SCOPED("Masked Sprites: Spot Lights Shadow pass");

			auto& spotLights = m_Renderer.GetCulledSpotLights();
			auto& framebuffers = m_SLFramebuffers;
			auto& pipeline = m_MaskedSSLPipeline;

			const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
			const bool bTexturesDirty = texturesChangedFrame >= m_MaskedSpritesSLTexturesUpdatedFrames[currentFrameIndex];
			if (bTexturesDirty)
			{
				m_MaskedSSLPipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
				m_MaskedSpritesSLTexturesUpdatedFrames[currentFrameIndex] = texturesChangedFrame + 1;
			}
			pipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
			pipeline->SetBuffer(MaterialSystem::GetMaterialsRawBuffer(), EG_PERSISTENT_SET, EG_BINDING_RAW_MATERIALS);

			pipeline->SetBuffer(transformsBuffer, 1, 0);

			for (size_t i = 0; i < m_SpotLightIndices.size(); ++i)
			{
				const size_t index = m_SpotLightIndices[i];
				auto& spotLight = spotLights[index];
				const auto& viewProj = lightMatrices[spotLight.ViewProjOffset];

				cmd->SetGraphicsCullMode(CullMode::Front);
				RenderSpritesTask::Draw(cmd, pipeline, singleSided.Opaque.ShadowCastingQuads, &viewProj, stats, framebuffers[i]);
				cmd->SetGraphicsCullMode(CullMode::None);
				RenderSpritesTask::Draw(cmd, pipeline, doubleSided.Opaque.ShadowCastingQuads, &viewProj, stats, framebuffers[i]);
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
		const auto& lightMatrices = m_Renderer.GetLightMatrices();
		auto& stats = m_Renderer.GetStats();

		// For directional light
		const auto& dirLight = m_Renderer.GetDirectionalLight();
		if (m_Renderer.HasDirectionalLight() && dirLight.bCastsShadows)
		{
			EG_GPU_TIMING_SCOPED(cmd, "Opaque Lit Texts: CSM Shadow pass");
			EG_CPU_TIMING_SCOPED("Opaque Lit Texts: CSM Shadow pass");

			auto& pipeline = m_OpaqueLitTDLPipeline;
			pipeline->SetBuffer(transformsBuffer, 0, 0);
			pipeline->SetTextureArray(m_Renderer.GetAtlases(), 1, 0);
			for (uint32_t i = 0; i < m_DLFramebuffers.size(); ++i)
			{
				const auto& viewProj = lightMatrices[dirLight.ViewProjOffset + i];
				cmd->SetGraphicsCullMode(CullMode::Front);
				RenderTextLitTask::Draw(cmd, pipeline, singleSided.Opaque.ShadowCastingQuads, &viewProj, stats, m_DLFramebuffers[i]);
				cmd->SetGraphicsCullMode(CullMode::None);
				RenderTextLitTask::Draw(cmd, pipeline, doubleSided.Opaque.ShadowCastingQuads, &viewProj, stats, m_DLFramebuffers[i]);
			}
		}

		// Point lights
		if (m_PointLightIndices.size())
		{
			EG_GPU_TIMING_SCOPED(cmd, "Opaque Lit Texts: Point Lights Shadow pass");
			EG_CPU_TIMING_SCOPED("Opaque Lit Texts: Point Lights Shadow pass");

			auto& framebuffers = m_PLFramebuffers;
			auto& vpsBuffer = m_PLVPsBuffer;
			auto& pipeline = m_OpaqueLitTPLPipeline;
			pipeline->SetBuffer(transformsBuffer, 0, 0);
			pipeline->SetBuffer(vpsBuffer, 0, 1);
			pipeline->SetTextureArray(m_Renderer.GetAtlases(), 1, 0);

			for (size_t i = 0; i < m_PointLightIndices.size(); ++i)
			{
				cmd->SetGraphicsCullMode(CullMode::Front);
				RenderTextLitTask::Draw(cmd, pipeline, singleSided.Opaque.ShadowCastingQuads, &i, stats, framebuffers[i]);
				cmd->SetGraphicsCullMode(CullMode::None);
				RenderTextLitTask::Draw(cmd, pipeline, doubleSided.Opaque.ShadowCastingQuads, &i, stats, framebuffers[i]);
			}
		}

		// Spot lights
		if (m_SpotLightIndices.size())
		{
			EG_GPU_TIMING_SCOPED(cmd, "Opaque Lit Texts: Spot Lights Shadow pass");
			EG_CPU_TIMING_SCOPED("Opaque Lit Texts: Spot Lights Shadow pass");

			auto& spotLights = m_Renderer.GetCulledSpotLights();
			auto& framebuffers = m_SLFramebuffers;
			auto& pipeline = m_OpaqueLitTSLPipeline;
			pipeline->SetBuffer(transformsBuffer, 0, 0);
			pipeline->SetTextureArray(m_Renderer.GetAtlases(), 1, 0);

			for (size_t i = 0; i < m_SpotLightIndices.size(); ++i)
			{
				const size_t index = m_SpotLightIndices[i];
				const auto& spotLight = spotLights[index];
				const auto& viewProj = lightMatrices[spotLight.ViewProjOffset];

				cmd->SetGraphicsCullMode(CullMode::Front);
				RenderTextLitTask::Draw(cmd, pipeline, singleSided.Opaque.ShadowCastingQuads, &viewProj, stats, framebuffers[i]);
				cmd->SetGraphicsCullMode(CullMode::None);
				RenderTextLitTask::Draw(cmd, pipeline, doubleSided.Opaque.ShadowCastingQuads, &viewProj, stats, framebuffers[i]);
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
		const auto& lightMatrices = m_Renderer.GetLightMatrices();
		auto& stats = m_Renderer.GetStats();

		const uint32_t currentFrameIndex = RenderManager::GetCurrentFrameIndex();

		// For directional light
		const auto& dirLight = m_Renderer.GetDirectionalLight();
		if (m_Renderer.HasDirectionalLight() && dirLight.bCastsShadows)
		{
			EG_GPU_TIMING_SCOPED(cmd, "Translucent Lit Texts: CSM Shadow pass");
			EG_CPU_TIMING_SCOPED("Translucent Lit Texts: CSM Shadow pass");

			auto& pipeline = m_TranslucentLitTDLPipeline;
			auto& framebuffers = m_DLCFramebuffers;

			const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
			const bool bTexturesDirty = texturesChangedFrame >= m_TranslucentLitTextsDLTexturesUpdatedFrames[currentFrameIndex];
			if (bTexturesDirty)
			{
				m_TranslucentLitTDLPipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
				m_TranslucentLitTextsDLTexturesUpdatedFrames[currentFrameIndex] = texturesChangedFrame + 1;
			}
			pipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
			pipeline->SetBuffer(MaterialSystem::GetMaterialsRawBuffer(), EG_PERSISTENT_SET, EG_BINDING_RAW_MATERIALS);
			pipeline->SetBuffer(transformsBuffer, 1, 0);
			pipeline->SetTextureArray(m_Renderer.GetAtlases(), 3, 0);

			for (uint32_t i = 0; i < framebuffers.size(); ++i)
			{
				const auto& viewProj = lightMatrices[dirLight.ViewProjOffset + i];
				cmd->SetGraphicsCullMode(CullMode::Front);
				RenderTextLitTask::Draw(cmd, pipeline, singleSided.Translucent.ShadowCastingQuads, &viewProj, stats, framebuffers[i]);
				cmd->SetGraphicsCullMode(CullMode::None);
				RenderTextLitTask::Draw(cmd, pipeline, doubleSided.Translucent.ShadowCastingQuads, &viewProj, stats, framebuffers[i]);
			}
		}

		// Point lights
		if (m_PointLightIndices.size())
		{
			EG_GPU_TIMING_SCOPED(cmd, "Translucent Lit Texts: Point Lights Shadow pass");
			EG_CPU_TIMING_SCOPED("Translucent Lit Texts: Point Lights Shadow pass");

			auto& vpsBuffer = m_PLVPsBuffer;
			auto& pipeline = m_TranslucentLitTPLPipeline;
			const auto& framebuffers = m_PLCFramebuffers;

			const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
			const bool bTexturesDirty = texturesChangedFrame >= m_TranslucentLitTextsPLTexturesUpdatedFrames[currentFrameIndex];
			if (bTexturesDirty)
			{
				m_TranslucentLitTPLPipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
				m_TranslucentLitTextsPLTexturesUpdatedFrames[currentFrameIndex] = texturesChangedFrame + 1;
			}
			pipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
			pipeline->SetBuffer(MaterialSystem::GetMaterialsRawBuffer(), EG_PERSISTENT_SET, EG_BINDING_RAW_MATERIALS);
			pipeline->SetBuffer(transformsBuffer, 1, 0);
			pipeline->SetBuffer(vpsBuffer, 1, 1);
			pipeline->SetTextureArray(m_Renderer.GetAtlases(), 3, 0);

			for (size_t i = 0; i < m_PointLightIndices.size(); ++i)
			{
				cmd->SetGraphicsCullMode(CullMode::Front);
				RenderTextLitTask::Draw(cmd, pipeline, singleSided.Translucent.ShadowCastingQuads, &i, stats, framebuffers[i]);
				cmd->SetGraphicsCullMode(CullMode::None);
				RenderTextLitTask::Draw(cmd, pipeline, doubleSided.Translucent.ShadowCastingQuads, &i, stats, framebuffers[i]);
			}
		}

		// Spot lights
		if (m_SpotLightIndices.size())
		{
			EG_GPU_TIMING_SCOPED(cmd, "Translucent Lit Texts: Spot Lights Shadow pass");
			EG_CPU_TIMING_SCOPED("Translucent Lit Texts: Spot Lights Shadow pass");

			auto& spotLights = m_Renderer.GetCulledSpotLights();
			auto& pipeline = m_TranslucentLitTSLPipeline;
			const auto& framebuffers = m_SLCFramebuffers;

			const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
			const bool bTexturesDirty = texturesChangedFrame >= m_TranslucentLitTextsSLTexturesUpdatedFrames[currentFrameIndex];
			if (bTexturesDirty)
			{
				m_TranslucentLitTSLPipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
				m_TranslucentLitTextsSLTexturesUpdatedFrames[currentFrameIndex] = texturesChangedFrame + 1;
			}
			pipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
			pipeline->SetBuffer(MaterialSystem::GetMaterialsRawBuffer(), EG_PERSISTENT_SET, EG_BINDING_RAW_MATERIALS);
			pipeline->SetBuffer(transformsBuffer, 1, 0);
			pipeline->SetTextureArray(m_Renderer.GetAtlases(), 3, 0);

			for (size_t i = 0; i < m_SpotLightIndices.size(); ++i)
			{
				const size_t index = m_SpotLightIndices[i];
				const auto& spotLight = spotLights[index];
				const auto& viewProj = lightMatrices[spotLight.ViewProjOffset];

				cmd->SetGraphicsCullMode(CullMode::Front);
				RenderTextLitTask::Draw(cmd, pipeline, singleSided.Translucent.ShadowCastingQuads, &viewProj, stats, framebuffers[i]);
				cmd->SetGraphicsCullMode(CullMode::None);
				RenderTextLitTask::Draw(cmd, pipeline, doubleSided.Translucent.ShadowCastingQuads, &viewProj, stats, framebuffers[i]);
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
		const auto& lightMatrices = m_Renderer.GetLightMatrices();
		auto& stats = m_Renderer.GetStats();

		const uint32_t currentFrameIndex = RenderManager::GetCurrentFrameIndex();

		// For directional light
		const auto& dirLight = m_Renderer.GetDirectionalLight();
		if (m_Renderer.HasDirectionalLight() && dirLight.bCastsShadows)
		{
			EG_GPU_TIMING_SCOPED(cmd, "Masked Lit Texts: CSM Shadow pass");
			EG_CPU_TIMING_SCOPED("Masked Lit Texts: CSM Shadow pass");

			auto& pipeline = m_MaskedLitTDLPipeline;
			const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
			const bool bTexturesDirty = texturesChangedFrame >= m_MaskedLitTextsDLTexturesUpdatedFrames[currentFrameIndex];
			if (bTexturesDirty)
			{
				m_MaskedLitTDLPipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
				m_MaskedLitTextsDLTexturesUpdatedFrames[currentFrameIndex] = texturesChangedFrame + 1;
			}
			pipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
			pipeline->SetBuffer(MaterialSystem::GetMaterialsRawBuffer(), EG_PERSISTENT_SET, EG_BINDING_RAW_MATERIALS);
			pipeline->SetBuffer(transformsBuffer, 1, 0);
			pipeline->SetTextureArray(m_Renderer.GetAtlases(), 3, 0);

			for (uint32_t i = 0; i < m_DLFramebuffers.size(); ++i)
			{
				const auto& viewProj = lightMatrices[dirLight.ViewProjOffset + i];
				cmd->SetGraphicsCullMode(CullMode::Front);
				RenderTextLitTask::Draw(cmd, pipeline, singleSided.Masked.ShadowCastingQuads, &viewProj, stats, m_DLFramebuffers[i]);
				cmd->SetGraphicsCullMode(CullMode::None);
				RenderTextLitTask::Draw(cmd, pipeline, doubleSided.Masked.ShadowCastingQuads, &viewProj, stats, m_DLFramebuffers[i]);
			}
		}

		// Point lights
		if (m_PointLightIndices.size())
		{
			EG_GPU_TIMING_SCOPED(cmd, "Masked Lit Texts: Point Lights Shadow pass");
			EG_CPU_TIMING_SCOPED("Masked Lit Texts: Point Lights Shadow pass");

			auto& framebuffers = m_PLFramebuffers;
			auto& vpsBuffer = m_PLVPsBuffer;
			auto& pipeline = m_MaskedLitTPLPipeline;

			const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
			const bool bTexturesDirty = texturesChangedFrame >= m_MaskedLitTextsPLTexturesUpdatedFrames[currentFrameIndex];
			if (bTexturesDirty)
			{
				m_MaskedLitTPLPipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
				m_MaskedLitTextsPLTexturesUpdatedFrames[currentFrameIndex] = texturesChangedFrame + 1;
			}
			pipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
			pipeline->SetBuffer(MaterialSystem::GetMaterialsRawBuffer(), EG_PERSISTENT_SET, EG_BINDING_RAW_MATERIALS);
			pipeline->SetBuffer(transformsBuffer, 1, 0);
			pipeline->SetBuffer(vpsBuffer, 1, 1);
			pipeline->SetTextureArray(m_Renderer.GetAtlases(), 3, 0);

			for (size_t i = 0; i < m_PointLightIndices.size(); ++i)
			{
				cmd->SetGraphicsCullMode(CullMode::Front);
				RenderTextLitTask::Draw(cmd, pipeline, singleSided.Masked.ShadowCastingQuads, &i, stats, framebuffers[i]);
				cmd->SetGraphicsCullMode(CullMode::None);
				RenderTextLitTask::Draw(cmd, pipeline, doubleSided.Masked.ShadowCastingQuads, &i, stats, framebuffers[i]);
			}
		}

		// Spot lights
		if (m_SpotLightIndices.size())
		{
			EG_GPU_TIMING_SCOPED(cmd, "Masked Lit Texts: Spot Lights Shadow pass");
			EG_CPU_TIMING_SCOPED("Masked Lit Texts: Spot Lights Shadow pass");

			auto& spotLights = m_Renderer.GetCulledSpotLights();
			auto& framebuffers = m_SLFramebuffers;
			auto& pipeline = m_MaskedLitTSLPipeline;

			const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
			const bool bTexturesDirty = texturesChangedFrame >= m_MaskedLitTextsSLTexturesUpdatedFrames[currentFrameIndex];
			if (bTexturesDirty)
			{
				m_MaskedLitTSLPipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
				m_MaskedLitTextsSLTexturesUpdatedFrames[currentFrameIndex] = texturesChangedFrame + 1;
			}
			pipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
			pipeline->SetBuffer(MaterialSystem::GetMaterialsRawBuffer(), EG_PERSISTENT_SET, EG_BINDING_RAW_MATERIALS);
			pipeline->SetBuffer(transformsBuffer, 1, 0);
			pipeline->SetTextureArray(m_Renderer.GetAtlases(), 3, 0);

			for (size_t i = 0; i < m_SpotLightIndices.size(); ++i)
			{
				const size_t index = m_SpotLightIndices[i];
				const auto& spotLight = spotLights[index];
				const auto& viewProj = lightMatrices[spotLight.ViewProjOffset];

				cmd->SetGraphicsCullMode(CullMode::Front);
				RenderTextLitTask::Draw(cmd, pipeline, singleSided.Masked.ShadowCastingQuads, &viewProj, stats, framebuffers[i]);
				cmd->SetGraphicsCullMode(CullMode::None);
				RenderTextLitTask::Draw(cmd, pipeline, doubleSided.Masked.ShadowCastingQuads, &viewProj, stats, framebuffers[i]);
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
		const auto& lightMatrices = m_Renderer.GetLightMatrices();
		auto& stats = m_Renderer.GetStats();

		// For directional light
		const auto& dirLight = m_Renderer.GetDirectionalLight();
		if (m_Renderer.HasDirectionalLight() && dirLight.bCastsShadows)
		{
			EG_GPU_TIMING_SCOPED(cmd, "Unlit Texts: CSM Shadow pass");
			EG_CPU_TIMING_SCOPED("Unlit Texts: CSM Shadow pass");

			auto& pipeline = m_UnlitTDLPipeline;
			pipeline->SetBuffer(transformsBuffer, 0, 0);
			pipeline->SetTextureArray(m_Renderer.GetAtlases(), 1, 0);
			for (uint32_t i = 0; i < m_DLFramebuffers.size(); ++i)
			{
				const auto& viewProj = lightMatrices[dirLight.ViewProjOffset + i];
				cmd->SetGraphicsCullMode(CullMode::Front);
				RenderTextUnlitTask::Draw(cmd, pipeline, singleSided.Opaque.ShadowCastingQuads, &viewProj, stats, m_DLFramebuffers[i]);
				cmd->SetGraphicsCullMode(CullMode::None);
				RenderTextUnlitTask::Draw(cmd, pipeline, doubleSided.Opaque.ShadowCastingQuads, &viewProj, stats, m_DLFramebuffers[i]);
			}
		}

		// Point lights
		if (m_PointLightIndices.size())
		{
			EG_GPU_TIMING_SCOPED(cmd, "Unlit Texts: Point Lights Shadow pass");
			EG_CPU_TIMING_SCOPED("Unlit Texts: Point Lights Shadow pass");

			auto& framebuffers = m_PLFramebuffers;
			auto& vpsBuffer = m_PLVPsBuffer;
			auto& pipeline = m_UnlitTPLPipeline;
			pipeline->SetBuffer(transformsBuffer, 0, 0);
			pipeline->SetBuffer(vpsBuffer, 0, 1);
			pipeline->SetTextureArray(m_Renderer.GetAtlases(), 1, 0);

			for (size_t i = 0; i < m_PointLightIndices.size(); ++i)
			{
				cmd->SetGraphicsCullMode(CullMode::Front);
				RenderTextUnlitTask::Draw(cmd, pipeline, singleSided.Opaque.ShadowCastingQuads, &i, stats, framebuffers[i]);
				cmd->SetGraphicsCullMode(CullMode::None);
				RenderTextUnlitTask::Draw(cmd, pipeline, doubleSided.Opaque.ShadowCastingQuads, &i, stats, framebuffers[i]);
			}
		}

		// Spot lights
		if (m_SpotLightIndices.size())
		{
			EG_GPU_TIMING_SCOPED(cmd, "Unlit Texts: Spot Lights Shadow pass");
			EG_CPU_TIMING_SCOPED("Unlit Texts: Spot Lights Shadow pass");

			auto& spotLights = m_Renderer.GetCulledSpotLights();
			uint32_t spotLightsCount = 0;
			auto& framebuffers = m_SLFramebuffers;
			auto& pipeline = m_UnlitTSLPipeline;
			pipeline->SetBuffer(transformsBuffer, 0, 0);
			pipeline->SetTextureArray(m_Renderer.GetAtlases(), 1, 0);

			for (size_t i = 0; i < m_SpotLightIndices.size(); ++i)
			{
				const size_t index = m_SpotLightIndices[i];
				auto& spotLight = spotLights[index];
				const auto& viewProj = lightMatrices[spotLight.ViewProjOffset];

				cmd->SetGraphicsCullMode(CullMode::Front);
				RenderTextUnlitTask::Draw(cmd, pipeline, singleSided.Opaque.ShadowCastingQuads, &viewProj, stats, framebuffers[i]);
				cmd->SetGraphicsCullMode(CullMode::None);
				RenderTextUnlitTask::Draw(cmd, pipeline, doubleSided.Opaque.ShadowCastingQuads, &viewProj, stats, framebuffers[i]);
			}
		}
	}

	void ShadowPassTask::InitOpacityMaskedMeshPipelines()
	{
		Ref<Sampler> shadowMapSampler = Sampler::Create(FilterMode::Point, AddressMode::ClampToOpaqueBlack, CompareOperation::Never, 0.f, 0.f, 1.f);

		// For directional light
		{
			for (uint32_t i = 0; i < m_DLShadowMapSamplers.size(); ++i)
				m_DLShadowMapSamplers[i] = shadowMapSampler;

			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
			depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
			depthAttachment.Image = RenderManager::GetDummyDepthImage();
			depthAttachment.bWriteDepth = true;
			depthAttachment.ClearOperation = ClearOperation::Load;
			depthAttachment.DepthCompareOp = CompareOperation::Greater;

			PipelineGraphicsState state;
			state.VertexShader = Shader::Create("shadow_maps/shadow_map_meshes.vert", ShaderType::Vertex);
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::Dynamic;
			state.PerInstanceAttribs = RenderMeshesTask::PerInstanceAttribs;
			m_OpacityMDLPipeline = PipelineGraphics::Create(state);

			const ShaderDefines defines = { {"EG_MATERIALS_REQUIRED", ""} };
			state.VertexShader = Shader::Create("shadow_maps/shadow_map_meshes.vert", ShaderType::Vertex, defines);
			state.FragmentShader = Shader::Create("shadow_maps/shadow_map_masked.frag", ShaderType::Fragment);
			m_MaskedMDLPipeline = PipelineGraphics::Create(state);
		}

		// For point lights
		{
			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
			depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
			depthAttachment.Image = RenderManager::GetDummyDepthCubeImage();
			depthAttachment.ClearOperation = ClearOperation::Load;
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
			m_OpacityMPLPipeline = PipelineGraphics::Create(state);

			defines["EG_MATERIALS_REQUIRED"] = "";
			state.VertexShader = Shader::Create("shadow_maps/shadow_map_meshes.vert", ShaderType::Vertex, defines);
			state.FragmentShader = Shader::Create("shadow_maps/shadow_map_masked.frag", ShaderType::Fragment);
			m_MaskedMPLPipeline = PipelineGraphics::Create(state);

			std::fill(m_PLShadowMapSamplers.begin(), m_PLShadowMapSamplers.end(), shadowMapSampler);
		}

		// For Spot lights
		{
			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
			depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
			depthAttachment.Image = RenderManager::GetDummyDepthImage();
			depthAttachment.ClearOperation = ClearOperation::Load;
			depthAttachment.DepthCompareOp = CompareOperation::Greater;

			ShaderDefines defines;
			defines["EG_SPOT_LIGHT_PASS"] = "";

			PipelineGraphicsState state;
			state.VertexShader = Shader::Create("shadow_maps/shadow_map_meshes.vert", ShaderType::Vertex, defines);
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::Dynamic;
			state.PerInstanceAttribs = RenderMeshesTask::PerInstanceAttribs;
			m_OpacityMSLPipeline = PipelineGraphics::Create(state);

			defines["EG_MATERIALS_REQUIRED"] = "";
			state.VertexShader = Shader::Create("shadow_maps/shadow_map_meshes.vert", ShaderType::Vertex, defines);
			state.FragmentShader = Shader::Create("shadow_maps/shadow_map_masked.frag", ShaderType::Fragment);
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
			depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
			depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
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
			colorAttachment.InitialLayout = ImageLayoutType::RenderTarget;
			colorAttachment.FinalLayout = ImageLayoutType::RenderTarget;
			colorAttachment.Image = RenderManager::GetDummyImage();
			colorAttachment.ClearOperation = ClearOperation::Load;
			colorAttachment.bBlendEnabled = true;
			colorAttachment.BlendingState.BlendSrc = BlendFactor::Zero;
			colorAttachment.BlendingState.BlendDst = BlendFactor::SrcColor;
			colorAttachment.BlendingState.BlendOp = BlendOperation::Add;

			ColorAttachment depthColorAttachment;
			depthColorAttachment.InitialLayout = ImageLayoutType::RenderTarget;
			depthColorAttachment.FinalLayout = ImageLayoutType::RenderTarget;
			depthColorAttachment.Image = RenderManager::GetDummyImageR16();
			depthColorAttachment.ClearOperation = ClearOperation::Load;
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
		}
	
		// For point lights
		{
			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
			depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
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
			colorAttachment.InitialLayout = ImageLayoutType::RenderTarget;
			colorAttachment.FinalLayout = ImageLayoutType::RenderTarget;
			colorAttachment.Image = RenderManager::GetDummyImageCube();
			colorAttachment.ClearOperation = ClearOperation::Load;
			colorAttachment.bBlendEnabled = true;
			colorAttachment.BlendingState.BlendSrc = BlendFactor::Zero;
			colorAttachment.BlendingState.BlendDst = BlendFactor::SrcColor;
			colorAttachment.BlendingState.BlendOp = BlendOperation::Add;

			ColorAttachment depthColorAttachment;
			depthColorAttachment.InitialLayout = ImageLayoutType::RenderTarget;
			depthColorAttachment.FinalLayout = ImageLayoutType::RenderTarget;
			depthColorAttachment.Image = RenderManager::GetDummyImageR16Cube();
			depthColorAttachment.ClearOperation = ClearOperation::Load;
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
		}
	
		// For Spot lights
		{
			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
			depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
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
			colorAttachment.InitialLayout = ImageLayoutType::RenderTarget;
			colorAttachment.FinalLayout = ImageLayoutType::RenderTarget;
			colorAttachment.Image = RenderManager::GetDummyImage();
			colorAttachment.ClearOperation = ClearOperation::Load;
			colorAttachment.bBlendEnabled = true;
			colorAttachment.BlendingState.BlendSrc = BlendFactor::Zero;
			colorAttachment.BlendingState.BlendDst = BlendFactor::SrcColor;
			colorAttachment.BlendingState.BlendOp = BlendOperation::Add;

			ColorAttachment depthColorAttachment;
			depthColorAttachment.InitialLayout = ImageLayoutType::RenderTarget;
			depthColorAttachment.FinalLayout = ImageLayoutType::RenderTarget;
			depthColorAttachment.Image = RenderManager::GetDummyImageR16();
			depthColorAttachment.ClearOperation = ClearOperation::Load;
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
		}
	}
	
	void ShadowPassTask::InitOpacitySkeletalMeshPipelines()
	{
		// For directional light
		{
			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
			depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
			depthAttachment.Image = RenderManager::GetDummyDepthImage();
			depthAttachment.ClearOperation = ClearOperation::Load;
			depthAttachment.DepthCompareOp = CompareOperation::Greater;

			PipelineGraphicsState state;
			state.VertexShader = Shader::Create("shadow_maps/shadow_map_skeletal_meshes.vert", ShaderType::Vertex);
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::Dynamic;
			state.PerInstanceAttribs = RenderSkeletalMeshesTask::PerInstanceAttribs;

			m_OpacitySMDLPipeline = PipelineGraphics::Create(state);
		}

		// For point lights
		{
			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
			depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
			depthAttachment.Image = RenderManager::GetDummyDepthCubeImage();
			depthAttachment.ClearOperation = ClearOperation::Load;
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
		}

		// For Spot lights
		{
			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
			depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
			depthAttachment.Image = RenderManager::GetDummyDepthImage();
			depthAttachment.ClearOperation = ClearOperation::Load;
			depthAttachment.DepthCompareOp = CompareOperation::Greater;

			ShaderDefines defines;
			defines["EG_SPOT_LIGHT_PASS"] = "";

			PipelineGraphicsState state;
			state.VertexShader = Shader::Create("shadow_maps/shadow_map_skeletal_meshes.vert", ShaderType::Vertex, defines);
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::Dynamic;
			state.PerInstanceAttribs = RenderSkeletalMeshesTask::PerInstanceAttribs;

			m_OpacitySMSLPipeline = PipelineGraphics::Create(state);
		}
	}

	void ShadowPassTask::InitMaskedSkeletalMeshPipelines()
	{
		// For directional light
		{
			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
			depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
			depthAttachment.Image = RenderManager::GetDummyDepthImage();
			depthAttachment.ClearOperation = ClearOperation::Load;
			depthAttachment.DepthCompareOp = CompareOperation::Greater;

			const ShaderDefines defines = { {"EG_MATERIALS_REQUIRED", ""} };
			PipelineGraphicsState state;
			state.VertexShader = Shader::Create("shadow_maps/shadow_map_skeletal_meshes.vert", ShaderType::Vertex, defines);
			state.FragmentShader = Shader::Create("shadow_maps/shadow_map_masked.frag", ShaderType::Fragment);
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::Dynamic;
			state.PerInstanceAttribs = RenderSkeletalMeshesTask::PerInstanceAttribs;

			m_MaskedSMDLPipeline = PipelineGraphics::Create(state);
		}

		// For point lights
		{
			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
			depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
			depthAttachment.Image = RenderManager::GetDummyDepthCubeImage();
			depthAttachment.ClearOperation = ClearOperation::Load;
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
		}

		// For Spot lights
		{
			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
			depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
			depthAttachment.Image = RenderManager::GetDummyDepthImage();
			depthAttachment.ClearOperation = ClearOperation::Load;
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
			colorAttachment.InitialLayout = ImageLayoutType::RenderTarget;
			colorAttachment.FinalLayout = ImageLayoutType::RenderTarget;
			colorAttachment.Image = RenderManager::GetDummyImage();
			colorAttachment.ClearOperation = ClearOperation::Load;
			colorAttachment.bBlendEnabled = true;
			colorAttachment.BlendingState.BlendSrc = BlendFactor::Zero;
			colorAttachment.BlendingState.BlendDst = BlendFactor::SrcColor;
			colorAttachment.BlendingState.BlendOp = BlendOperation::Add;

			ColorAttachment depthColorAttachment;
			depthColorAttachment.InitialLayout = ImageLayoutType::RenderTarget;
			depthColorAttachment.FinalLayout = ImageLayoutType::RenderTarget;
			depthColorAttachment.Image = RenderManager::GetDummyImageR16();
			depthColorAttachment.ClearOperation = ClearOperation::Load;
			depthColorAttachment.bBlendEnabled = true;
			depthColorAttachment.BlendingState.BlendSrc = BlendFactor::One;
			depthColorAttachment.BlendingState.BlendDst = BlendFactor::One;
			depthColorAttachment.BlendingState.BlendOp = BlendOperation::Max;

			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
			depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
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

			if (m_TranslucentSMDLPipeline)
				m_TranslucentSMDLPipeline->SetState(state);
			else
				m_TranslucentSMDLPipeline = PipelineGraphics::Create(state);
		}

		// For point lights
		{
			ColorAttachment colorAttachment;
			colorAttachment.InitialLayout = ImageLayoutType::RenderTarget;
			colorAttachment.FinalLayout = ImageLayoutType::RenderTarget;
			colorAttachment.Image = RenderManager::GetDummyImageCube();
			colorAttachment.ClearOperation = ClearOperation::Load;
			colorAttachment.bBlendEnabled = true;
			colorAttachment.BlendingState.BlendSrc = BlendFactor::Zero;
			colorAttachment.BlendingState.BlendDst = BlendFactor::SrcColor;
			colorAttachment.BlendingState.BlendOp = BlendOperation::Add;

			ColorAttachment depthColorAttachment;
			depthColorAttachment.InitialLayout = ImageLayoutType::RenderTarget;
			depthColorAttachment.FinalLayout = ImageLayoutType::RenderTarget;
			depthColorAttachment.Image = RenderManager::GetDummyImageR16Cube();
			depthColorAttachment.ClearOperation = ClearOperation::Load;
			depthColorAttachment.bBlendEnabled = true;
			depthColorAttachment.BlendingState.BlendSrc = BlendFactor::One;
			depthColorAttachment.BlendingState.BlendDst = BlendFactor::One;
			depthColorAttachment.BlendingState.BlendOp = BlendOperation::Max;

			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
			depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
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

			if (m_TranslucentSMPLPipeline)
				m_TranslucentSMPLPipeline->SetState(state);
			else
				m_TranslucentSMPLPipeline = PipelineGraphics::Create(state);
		}

		// For Spot lights
		{
			ColorAttachment colorAttachment;
			colorAttachment.InitialLayout = ImageLayoutType::RenderTarget;
			colorAttachment.FinalLayout = ImageLayoutType::RenderTarget;
			colorAttachment.Image = RenderManager::GetDummyImage();
			colorAttachment.ClearOperation = ClearOperation::Load;
			colorAttachment.bBlendEnabled = true;
			colorAttachment.BlendingState.BlendSrc = BlendFactor::Zero;
			colorAttachment.BlendingState.BlendDst = BlendFactor::SrcColor;
			colorAttachment.BlendingState.BlendOp = BlendOperation::Add;

			ColorAttachment depthColorAttachment;
			depthColorAttachment.InitialLayout = ImageLayoutType::RenderTarget;
			depthColorAttachment.FinalLayout = ImageLayoutType::RenderTarget;
			depthColorAttachment.Image = RenderManager::GetDummyImageR16();
			depthColorAttachment.ClearOperation = ClearOperation::Load;
			depthColorAttachment.bBlendEnabled = true;
			depthColorAttachment.BlendingState.BlendSrc = BlendFactor::One;
			depthColorAttachment.BlendingState.BlendDst = BlendFactor::One;
			depthColorAttachment.BlendingState.BlendOp = BlendOperation::Max;

			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
			depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
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
			depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
			depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
			depthAttachment.Image = RenderManager::GetDummyDepthImage();
			depthAttachment.ClearOperation = ClearOperation::Load;
			depthAttachment.DepthCompareOp = CompareOperation::Greater;

			PipelineGraphicsState state;
			state.VertexShader = Shader::Create("shadow_maps/shadow_map_sprites.vert", ShaderType::Vertex);
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::Dynamic;
			state.FrontFace = FrontFaceMode::Clockwise;

			m_OpacitySDLPipeline = PipelineGraphics::Create(state);
		}

		// Point light
		{
			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
			depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
			depthAttachment.Image = RenderManager::GetDummyDepthCubeImage();
			depthAttachment.ClearOperation = ClearOperation::Load;
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
		}

		// Spot light
		{
			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
			depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
			depthAttachment.Image = RenderManager::GetDummyDepthImage();
			depthAttachment.ClearOperation = ClearOperation::Load;
			depthAttachment.DepthCompareOp = CompareOperation::Greater;

			ShaderDefines slDefines;
			slDefines["EG_SPOT_LIGHT_PASS"] = "";

			PipelineGraphicsState state;
			state.VertexShader = Shader::Create("shadow_maps/shadow_map_sprites.vert", ShaderType::Vertex, slDefines);
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::Dynamic;
			state.FrontFace = FrontFaceMode::Clockwise;

			m_OpacitySSLPipeline = PipelineGraphics::Create(state);
		}
	}

	void ShadowPassTask::InitMaskedSpritesPipelines()
	{
		// Directional light
		{
			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
			depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
			depthAttachment.Image = RenderManager::GetDummyDepthImage();
			depthAttachment.ClearOperation = ClearOperation::Load;
			depthAttachment.DepthCompareOp = CompareOperation::Greater;

			PipelineGraphicsState state;
			state.VertexShader = Shader::Create("shadow_maps/shadow_map_sprites.vert", ShaderType::Vertex, { {"EG_MATERIALS_REQUIRED", ""} });
			state.FragmentShader = Shader::Create("shadow_maps/shadow_map_masked.frag", ShaderType::Fragment);
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::Dynamic;
			state.FrontFace = FrontFaceMode::Clockwise;

			m_MaskedSDLPipeline = PipelineGraphics::Create(state);
		}

		// Point light
		{
			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
			depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
			depthAttachment.Image = RenderManager::GetDummyDepthCubeImage();
			depthAttachment.ClearOperation = ClearOperation::Load;
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
		}

		// Spot light
		{
			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
			depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
			depthAttachment.Image = RenderManager::GetDummyDepthImage();
			depthAttachment.ClearOperation = ClearOperation::Load;
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
			colorAttachment.InitialLayout = ImageLayoutType::RenderTarget;
			colorAttachment.FinalLayout = ImageLayoutType::RenderTarget;
			colorAttachment.Image = RenderManager::GetDummyImage();
			colorAttachment.ClearOperation = ClearOperation::Load;
			colorAttachment.bBlendEnabled = true;
			colorAttachment.BlendingState.BlendSrc = BlendFactor::Zero;
			colorAttachment.BlendingState.BlendDst = BlendFactor::SrcColor;
			colorAttachment.BlendingState.BlendOp = BlendOperation::Add;

			ColorAttachment depthColorAttachment;
			depthColorAttachment.InitialLayout = ImageLayoutType::RenderTarget;
			depthColorAttachment.FinalLayout = ImageLayoutType::RenderTarget;
			depthColorAttachment.Image = RenderManager::GetDummyImageR16();
			depthColorAttachment.ClearOperation = ClearOperation::Load;
			depthColorAttachment.bBlendEnabled = true;
			depthColorAttachment.BlendingState.BlendSrc = BlendFactor::One;
			depthColorAttachment.BlendingState.BlendDst = BlendFactor::One;
			depthColorAttachment.BlendingState.BlendOp = BlendOperation::Max;

			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
			depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
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

			if (m_TranslucentSDLPipeline)
				m_TranslucentSDLPipeline->SetState(state);
			else
				m_TranslucentSDLPipeline = PipelineGraphics::Create(state);
		}

		// Point light
		{
			ColorAttachment colorAttachment;
			colorAttachment.InitialLayout = ImageLayoutType::RenderTarget;
			colorAttachment.FinalLayout = ImageLayoutType::RenderTarget;
			colorAttachment.Image = RenderManager::GetDummyImageCube();
			colorAttachment.ClearOperation = ClearOperation::Load;
			colorAttachment.bBlendEnabled = true;
			colorAttachment.BlendingState.BlendSrc = BlendFactor::Zero;
			colorAttachment.BlendingState.BlendDst = BlendFactor::SrcColor;
			colorAttachment.BlendingState.BlendOp = BlendOperation::Add;

			ColorAttachment depthColorAttachment;
			depthColorAttachment.InitialLayout = ImageLayoutType::RenderTarget;
			depthColorAttachment.FinalLayout = ImageLayoutType::RenderTarget;
			depthColorAttachment.Image = RenderManager::GetDummyImageR16Cube();
			depthColorAttachment.ClearOperation = ClearOperation::Load;
			depthColorAttachment.bBlendEnabled = true;
			depthColorAttachment.BlendingState.BlendSrc = BlendFactor::One;
			depthColorAttachment.BlendingState.BlendDst = BlendFactor::One;
			depthColorAttachment.BlendingState.BlendOp = BlendOperation::Max;

			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
			depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
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

			if (m_TranslucentSPLPipeline)
				m_TranslucentSPLPipeline->SetState(state);
			else
				m_TranslucentSPLPipeline = PipelineGraphics::Create(state);
		}

		// Spot light
		{
			ColorAttachment colorAttachment;
			colorAttachment.InitialLayout = ImageLayoutType::RenderTarget;
			colorAttachment.FinalLayout = ImageLayoutType::RenderTarget;
			colorAttachment.Image = RenderManager::GetDummyImage();
			colorAttachment.ClearOperation = ClearOperation::Load;
			colorAttachment.bBlendEnabled = true;
			colorAttachment.BlendingState.BlendSrc = BlendFactor::Zero;
			colorAttachment.BlendingState.BlendDst = BlendFactor::SrcColor;
			colorAttachment.BlendingState.BlendOp = BlendOperation::Add;

			ColorAttachment depthColorAttachment;
			depthColorAttachment.InitialLayout = ImageLayoutType::RenderTarget;
			depthColorAttachment.FinalLayout = ImageLayoutType::RenderTarget;
			depthColorAttachment.Image = RenderManager::GetDummyImageR16();
			depthColorAttachment.ClearOperation = ClearOperation::Load;
			depthColorAttachment.bBlendEnabled = true;
			depthColorAttachment.BlendingState.BlendSrc = BlendFactor::One;
			depthColorAttachment.BlendingState.BlendDst = BlendFactor::One;
			depthColorAttachment.BlendingState.BlendOp = BlendOperation::Max;

			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
			depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
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
			depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
			depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
			depthAttachment.Image = RenderManager::GetDummyDepthImage();
			depthAttachment.ClearOperation = ClearOperation::Load;
			depthAttachment.DepthCompareOp = CompareOperation::Greater;

			PipelineGraphicsState state;
			state.VertexShader = Shader::Create("shadow_maps/shadow_map_texts_lit.vert", ShaderType::Vertex);
			state.FragmentShader = fragShader;
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::Dynamic;

			m_OpaqueLitTDLPipeline = PipelineGraphics::Create(state);
		}

		// Point light
		{
			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
			depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
			depthAttachment.Image = RenderManager::GetDummyDepthCubeImage();
			depthAttachment.ClearOperation = ClearOperation::Load;
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
		}

		// Spot light
		{
			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
			depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
			depthAttachment.Image = RenderManager::GetDummyDepthImage();
			depthAttachment.ClearOperation = ClearOperation::Load;
			depthAttachment.DepthCompareOp = CompareOperation::Greater;

			ShaderDefines slDefines;
			slDefines["EG_SPOT_LIGHT_PASS"] = "";

			PipelineGraphicsState state;
			state.VertexShader = Shader::Create("shadow_maps/shadow_map_texts_lit.vert", ShaderType::Vertex, slDefines);
			state.FragmentShader = fragShader;
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::Dynamic;

			m_OpaqueLitTSLPipeline = PipelineGraphics::Create(state);
		}
	}
	
	void ShadowPassTask::InitMaskedLitTextsPipelines()
	{
		Ref<Shader> fragShader = Shader::Create("shadow_maps/shadow_map_texts_lit.frag", ShaderType::Fragment, { {"EG_MASKED", ""}, {"EG_MATERIALS_REQUIRED", ""} });

		// Directional light
		{
			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
			depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
			depthAttachment.Image = RenderManager::GetDummyDepthImage();
			depthAttachment.ClearOperation = ClearOperation::Load;
			depthAttachment.DepthCompareOp = CompareOperation::Greater;

			PipelineGraphicsState state;
			state.VertexShader = Shader::Create("shadow_maps/shadow_map_texts_lit.vert", ShaderType::Vertex, { {"EG_MATERIALS_REQUIRED", ""} });
			state.FragmentShader = fragShader;
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::Dynamic;

			m_MaskedLitTDLPipeline = PipelineGraphics::Create(state);
		}

		// Point light
		{
			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
			depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
			depthAttachment.Image = RenderManager::GetDummyDepthCubeImage();
			depthAttachment.ClearOperation = ClearOperation::Load;
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
		}

		// Spot light
		{
			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
			depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
			depthAttachment.Image = RenderManager::GetDummyDepthImage();
			depthAttachment.ClearOperation = ClearOperation::Load;
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
			colorAttachment.InitialLayout = ImageLayoutType::RenderTarget;
			colorAttachment.FinalLayout = ImageLayoutType::RenderTarget;
			colorAttachment.Image = RenderManager::GetDummyImage();
			colorAttachment.ClearOperation = ClearOperation::Load;
			colorAttachment.bBlendEnabled = true;
			colorAttachment.BlendingState.BlendSrc = BlendFactor::Zero;
			colorAttachment.BlendingState.BlendDst = BlendFactor::SrcColor;
			colorAttachment.BlendingState.BlendOp = BlendOperation::Add;

			ColorAttachment depthColorAttachment;
			depthColorAttachment.InitialLayout = ImageLayoutType::RenderTarget;
			depthColorAttachment.FinalLayout = ImageLayoutType::RenderTarget;
			depthColorAttachment.Image = RenderManager::GetDummyImageR16();
			depthColorAttachment.ClearOperation = ClearOperation::Load;
			depthColorAttachment.bBlendEnabled = true;
			depthColorAttachment.BlendingState.BlendSrc = BlendFactor::One;
			depthColorAttachment.BlendingState.BlendDst = BlendFactor::One;
			depthColorAttachment.BlendingState.BlendOp = BlendOperation::Max;

			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
			depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
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

			if (m_TranslucentLitTDLPipeline)
				m_TranslucentLitTDLPipeline->SetState(state);
			else
				m_TranslucentLitTDLPipeline = PipelineGraphics::Create(state);
		}

		// Point light
		{
			ColorAttachment colorAttachment;
			colorAttachment.InitialLayout = ImageLayoutType::RenderTarget;
			colorAttachment.FinalLayout = ImageLayoutType::RenderTarget;
			colorAttachment.Image = RenderManager::GetDummyImageCube();
			colorAttachment.ClearOperation = ClearOperation::Load;
			colorAttachment.bBlendEnabled = true;
			colorAttachment.BlendingState.BlendSrc = BlendFactor::Zero;
			colorAttachment.BlendingState.BlendDst = BlendFactor::SrcColor;
			colorAttachment.BlendingState.BlendOp = BlendOperation::Add;

			ColorAttachment depthColorAttachment;
			depthColorAttachment.InitialLayout = ImageLayoutType::RenderTarget;
			depthColorAttachment.FinalLayout = ImageLayoutType::RenderTarget;
			depthColorAttachment.Image = RenderManager::GetDummyImageR16Cube();
			depthColorAttachment.ClearOperation = ClearOperation::Load;
			depthColorAttachment.bBlendEnabled = true;
			depthColorAttachment.BlendingState.BlendSrc = BlendFactor::One;
			depthColorAttachment.BlendingState.BlendDst = BlendFactor::One;
			depthColorAttachment.BlendingState.BlendOp = BlendOperation::Max;

			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
			depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
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

			if (m_TranslucentLitTPLPipeline)
				m_TranslucentLitTPLPipeline->SetState(state);
			else
				m_TranslucentLitTPLPipeline = PipelineGraphics::Create(state);
		}

		// Spot light
		{
			ColorAttachment colorAttachment;
			colorAttachment.InitialLayout = ImageLayoutType::RenderTarget;
			colorAttachment.FinalLayout = ImageLayoutType::RenderTarget;
			colorAttachment.Image = RenderManager::GetDummyImage();
			colorAttachment.ClearOperation = ClearOperation::Load;
			colorAttachment.bBlendEnabled = true;
			colorAttachment.BlendingState.BlendSrc = BlendFactor::Zero;
			colorAttachment.BlendingState.BlendDst = BlendFactor::SrcColor;
			colorAttachment.BlendingState.BlendOp = BlendOperation::Add;

			ColorAttachment depthColorAttachment;
			depthColorAttachment.InitialLayout = ImageLayoutType::RenderTarget;
			depthColorAttachment.FinalLayout = ImageLayoutType::RenderTarget;
			depthColorAttachment.Image = RenderManager::GetDummyImageR16();
			depthColorAttachment.ClearOperation = ClearOperation::Load;
			depthColorAttachment.bBlendEnabled = true;
			depthColorAttachment.BlendingState.BlendSrc = BlendFactor::One;
			depthColorAttachment.BlendingState.BlendDst = BlendFactor::One;
			depthColorAttachment.BlendingState.BlendOp = BlendOperation::Max;

			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
			depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
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
			depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
			depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
			depthAttachment.Image = RenderManager::GetDummyDepthImage();
			depthAttachment.ClearOperation = ClearOperation::Load;
			depthAttachment.DepthCompareOp = CompareOperation::Greater;

			PipelineGraphicsState state;
			state.VertexShader = Shader::Create("shadow_maps/shadow_map_texts_unlit.vert", ShaderType::Vertex);
			state.FragmentShader = fragShader;
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::None;

			m_UnlitTDLPipeline = PipelineGraphics::Create(state);
		}

		// Point light
		{
			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
			depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
			depthAttachment.Image = RenderManager::GetDummyDepthCubeImage();
			depthAttachment.ClearOperation = ClearOperation::Load;
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
		}

		// Spot light
		{
			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
			depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
			depthAttachment.Image = RenderManager::GetDummyDepthImage();
			depthAttachment.ClearOperation = ClearOperation::Load;
			depthAttachment.DepthCompareOp = CompareOperation::Greater;

			ShaderDefines slDefines;
			slDefines["EG_SPOT_LIGHT_PASS"] = "";

			PipelineGraphicsState state;
			state.VertexShader = Shader::Create("shadow_maps/shadow_map_texts_unlit.vert", ShaderType::Vertex, slDefines);
			state.FragmentShader = fragShader;
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::None;

			m_UnlitTSLPipeline = PipelineGraphics::Create(state);
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

	void ShadowPassTask::InitColoredDirectionalLightFramebuffers(std::vector<Ref<Framebuffer>>& framebuffers, const Ref<PipelineGraphics>& pipeline)
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
			attachments.push_back(nonTraslucentShadowMaps[i]);

			framebuffers[i] = Framebuffer::Create(attachments, glm::uvec2(csmSizes[i]), renderPassHandle);
		}
	}
	
	void ShadowPassTask::FreeColoredDirectionalLightShadowMaps()
	{
		std::fill(m_DLCShadowMaps.begin(), m_DLCShadowMaps.end(), RenderManager::GetDummyImage());
		std::fill(m_DLCDShadowMaps.begin(), m_DLCDShadowMaps.end(), RenderManager::GetDummyImageR16());
		m_DLCFramebuffers.clear();
	}
	
	void ShadowPassTask::HandleColoredPointLightShadowMaps()
	{
		m_PLCShadowMaps.clear();
		m_PLCDShadowMaps.clear();
		m_PLCFramebuffers.clear();
	}

	void ShadowPassTask::HandleColoredSpotLightShadowMaps()
	{
		m_SLCShadowMaps.clear();
		m_SLCDShadowMaps.clear();
		m_SLCFramebuffers.clear();
	}
}
