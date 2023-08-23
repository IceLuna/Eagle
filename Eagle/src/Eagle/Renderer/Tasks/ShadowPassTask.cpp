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

#include "Eagle/Debug/CPUTimings.h"
#include "Eagle/Debug/GPUTimings.h"

#include "../../Eagle-Editor/assets/shaders/common_structures.h"

// Temp
#include "Eagle/Core/Application.h"

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

	static Ref<Image> CreateDepthImage(glm::uvec3 size, const std::string& debugName, ImageLayout layout, bool bCube)
	{
		ImageSpecifications depthSpecs;
		depthSpecs.Format = Application::Get().GetRenderContext()->GetDepthFormat();
		depthSpecs.Usage = ImageUsage::DepthStencilAttachment | ImageUsage::Sampled;
		depthSpecs.bIsCube = bCube;
		depthSpecs.Size = size;
		depthSpecs.Layout = layout;
		return Image::Create(depthSpecs, debugName);
	}

	ShadowPassTask::ShadowPassTask(SceneRenderer& renderer)
		: RendererTask(renderer)
		, m_PLShadowMaps(EG_MAX_LIGHT_SHADOW_MAPS)
		, m_PLShadowMapSamplers(EG_MAX_LIGHT_SHADOW_MAPS)
		, m_SLShadowMaps(EG_MAX_LIGHT_SHADOW_MAPS)
		, m_SLShadowMapSamplers(m_PLShadowMapSamplers)
		, m_DLShadowMaps(EG_CASCADES_COUNT)
		, m_DLShadowMapSamplers(EG_CASCADES_COUNT)
	{
		std::fill(m_PLShadowMaps.begin(), m_PLShadowMaps.end(), RenderManager::GetDummyDepthCubeImage());
		std::fill(m_SLShadowMaps.begin(), m_SLShadowMaps.end(), RenderManager::GetDummyDepthImage());
		std::fill(m_DLShadowMaps.begin(), m_DLShadowMaps.end(), RenderManager::GetDummyDepthImage());

		m_Settings = m_Renderer.GetOptions().ShadowsSettings;

		m_TextFragShader = Shader::Create("assets/shaders/shadow_map_texts.frag", ShaderType::Fragment);
		m_MaskedTextFragShader = Shader::Create("assets/shaders/shadow_map_texts.frag", ShaderType::Fragment, { {"EG_MASKED", ""} });
		InitMeshPipelines();
		InitOpacitySpritesPipelines();
		InitMaskedSpritesPipelines();
		InitOpaqueLitTextsPipelines();
		InitMaskedLitTextsPipelines();
		InitUnlitTextsPipelines();

		BufferSpecifications pointLightsVPBufferSpecs;
		pointLightsVPBufferSpecs.Size = sizeof(glm::mat4) * 6;
		pointLightsVPBufferSpecs.Layout = BufferReadAccess::Uniform;
		pointLightsVPBufferSpecs.Usage = BufferUsage::UniformBuffer | BufferUsage::TransferDst;
		m_PLVPsBuffer = Buffer::Create(pointLightsVPBufferSpecs, "PointLightsVPs");
	}

	void ShadowPassTask::RecordCommandBuffer(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "Shadow pass");
		EG_CPU_TIMING_SCOPED("Shadow pass");

		bDidDrawDL = false;
		bDidDrawPL = false;
		bDidDrawSL = false;

		ShadowPassOpacityMeshes(cmd);
		ShadowPassMaskedMeshes(cmd);
		ShadowPassOpacitySprites(cmd);
		ShadowPassMaskedSprites(cmd);
		ShadowPassOpaqueLitTexts(cmd);
		ShadowPassMaskedLitTexts(cmd);
		ShadowPassUnlitTexts(cmd);
	}

	void ShadowPassTask::InitWithOptions(const SceneRendererSettings& settings)
	{
		if (settings.ShadowsSettings == m_Settings)
			return;

		const bool bDirLightChanged = !m_Settings.DirLightsEqual(settings.ShadowsSettings);
		const bool bPointLightChanged = m_Settings.PointLightShadowMapSize != settings.ShadowsSettings.PointLightShadowMapSize;
		const bool bSpotLightChanged = m_Settings.SpotLightShadowMapSize != settings.ShadowsSettings.SpotLightShadowMapSize;
		m_Settings = settings.ShadowsSettings;

		if (bDirLightChanged)
			InitDirectionalLightShadowMaps();

		if (bPointLightChanged)
		{
			std::fill(m_PLShadowMaps.begin(), m_PLShadowMaps.end(), RenderManager::GetDummyDepthCubeImage());
			m_PLFramebuffers.clear();
		}
		if (bSpotLightChanged)
		{
			std::fill(m_SLShadowMaps.begin(), m_SLShadowMaps.end(), RenderManager::GetDummyDepthImage());
			m_SLFramebuffers.clear();
		}
	}

	void ShadowPassTask::ShadowPassOpacityMeshes(const Ref<CommandBuffer>& cmd)
	{
		auto& meshes = m_Renderer.GetOpaqueMeshes();
		if (meshes.empty())
			return;

		EG_GPU_TIMING_SCOPED(cmd, "Opacity Meshes shadow pass");
		EG_CPU_TIMING_SCOPED("Opacity Meshes shadow pass");

		const auto& meshesData = m_Renderer.GetOpaqueMeshesData();
		const auto& vb = meshesData.VertexBuffer;
		const auto& ivb = meshesData.InstanceBuffer;
		const auto& ib = meshesData.IndexBuffer;
		const auto& transformsBuffer = m_Renderer.GetMeshTransformsBuffer();
		const auto& dirLight = m_Renderer.GetDirectionalLight();
		const glm::vec3 cameraPos = m_Renderer.GetViewPosition();
		const float shadowMaxDistance = m_Renderer.GetShadowMaxDistance();
		auto& stats = m_Renderer.GetStats();

		// For directional light
		if (m_Renderer.HasDirectionalLight() && dirLight.bCastsShadows)
		{
			EG_GPU_TIMING_SCOPED(cmd, "Opacity Meshes: CSM Shadow pass");
			EG_CPU_TIMING_SCOPED("Opacity Meshes: CSM Shadow pass");
			bDidDrawDL = true;

			CreateIfNeededDirectionalLightShadowMaps();
			m_OpacityMDLPipeline->SetBuffer(transformsBuffer, 0, 0);
			for (uint32_t i = 0; i < m_DLFramebuffers.size(); ++i)
			{
				const auto& viewProj = dirLight.ViewProj[i];

				cmd->BeginGraphics(m_OpacityMDLPipeline, m_DLFramebuffers[i]);
				cmd->SetGraphicsRootConstants(&viewProj, nullptr);

				uint32_t firstIndex = 0;
				uint32_t firstInstance = 0;
				uint32_t vertexOffset = 0;
				for (auto& [meshKey, datas] : meshes)
				{
					const uint32_t verticesCount = (uint32_t)meshKey.Mesh->GetVertices().size();
					const uint32_t indicesCount = (uint32_t)meshKey.Mesh->GetIndeces().size();
					const uint32_t instanceCount = (uint32_t)datas.size();

					if (meshKey.bCastsShadows)
					{
						stats.Indeces += indicesCount;
						stats.Vertices += verticesCount;
						++stats.DrawCalls;
						cmd->DrawIndexedInstanced(vb, ib, indicesCount, firstIndex, vertexOffset, instanceCount, firstInstance, ivb);
					}

					firstIndex += indicesCount;
					vertexOffset += verticesCount;
					firstInstance += instanceCount;
				}
				cmd->EndGraphics();
			}
		}
		else
		{
			FreeDirectionalLightShadowMaps();
		}

		// For point lights
		{
			const auto& pointLights = m_Renderer.GetPointLights();
			uint32_t pointLightsCount = 0;
			auto& framebuffers = m_PLFramebuffers;
			auto& shadowMaps = m_PLShadowMaps;

			{
				auto& vpsBuffer = m_PLVPsBuffer;
				auto& pipeline = m_OpacityMPLPipeline;
				pipeline->SetBuffer(transformsBuffer, 0, 0);
				pipeline->SetBuffer(vpsBuffer, 0, 1);
				{
					EG_GPU_TIMING_SCOPED(cmd, "Opacity Meshes: Point Lights Shadow pass");
					EG_CPU_TIMING_SCOPED("Opacity Meshes: Point Lights Shadow pass");

					for (auto& pointLight : pointLights)
					{
						if (!pointLight.DoesCastShadows())
							continue;

						bDidDrawPL = true;
						const float distanceToCamera = glm::length(cameraPos - pointLight.Position);
						const uint32_t& i = pointLightsCount;
						
						const glm::uvec2 smSize = GetPointLightSMSize(distanceToCamera, shadowMaxDistance);
						if (i >= framebuffers.size())
						{
							// Create SM & framebuffer
							shadowMaps[i] = CreateDepthImage(glm::uvec3(smSize, 1u), "PointLight_SM" + std::to_string(i), ImageLayoutType::Unknown, true);
							framebuffers.push_back(Framebuffer::Create({ shadowMaps[i] }, smSize, pipeline->GetRenderPassHandle()));
						}
						else if (smSize != framebuffers[i]->GetSize())
						{
							// Create SM & framebuffer with the new size
							shadowMaps[i] = CreateDepthImage(glm::uvec3(smSize, 1u), "PointLight_SM" + std::to_string(i), ImageLayoutType::Unknown, true);
							framebuffers[i] = Framebuffer::Create({shadowMaps[i]}, smSize, pipeline->GetRenderPassHandle());
						}

						cmd->TransitionLayout(vpsBuffer, BufferReadAccess::Uniform, BufferReadAccess::Uniform);
						cmd->Write(vpsBuffer, &pointLight.ViewProj[0][0], vpsBuffer->GetSize(), 0, BufferLayoutType::Unknown, BufferReadAccess::Uniform);
						cmd->TransitionLayout(vpsBuffer, BufferReadAccess::Uniform, BufferReadAccess::Uniform);

						cmd->BeginGraphics(pipeline, framebuffers[i]);

						uint32_t firstIndex = 0;
						uint32_t firstInstance = 0;
						uint32_t vertexOffset = 0;
						for (auto& [meshKey, datas] : meshes)
						{
							const uint32_t verticesCount = (uint32_t)meshKey.Mesh->GetVertices().size();
							const uint32_t indicesCount = (uint32_t)meshKey.Mesh->GetIndeces().size();
							const uint32_t instanceCount = (uint32_t)datas.size();

							if (meshKey.bCastsShadows)
							{
								stats.Indeces += indicesCount;
								stats.Vertices += verticesCount;
								++stats.DrawCalls;
								cmd->DrawIndexedInstanced(vb, ib, indicesCount, firstIndex, vertexOffset, instanceCount, firstInstance, ivb);
							}

							firstIndex += indicesCount;
							vertexOffset += verticesCount;
							firstInstance += instanceCount;
						}
						cmd->EndGraphics();
						++pointLightsCount;
					}

					// Release unused shadow-maps & framebuffers
					for (size_t i = pointLightsCount; i < framebuffers.size(); ++i)
						shadowMaps[i] = RenderManager::GetDummyDepthCubeImage();
					framebuffers.resize(pointLightsCount);
				}
			}
		}

		// For spot lights
		{
			const auto& spotLights = m_Renderer.GetSpotLights();
			uint32_t spotLightsCount = 0;
			auto& framebuffers = m_SLFramebuffers;
			auto& shadowMaps = m_SLShadowMaps;
			{
				auto& pipeline = m_OpacityMSLPipeline;
				pipeline->SetBuffer(transformsBuffer, 0, 0);
				{
					EG_GPU_TIMING_SCOPED(cmd, "Opacity Meshes: Spot Lights Shadow pass");
					EG_CPU_TIMING_SCOPED("Opacity Meshes: Spot Lights Shadow pass");

					for (auto& spotLight : spotLights)
					{
						if (spotLight.bCastsShadows == 0)
							continue;
						
						bDidDrawSL = true;
						const float distanceToCamera = glm::length(cameraPos - spotLight.Position);
						const uint32_t& i = spotLightsCount;

						const glm::uvec2 smSize = GetSpotLightSMSize(distanceToCamera, shadowMaxDistance);
						if (i >= framebuffers.size())
						{
							// Create SM & framebuffer
							shadowMaps[i] = CreateDepthImage(glm::uvec3(smSize, 1u), "SpotLight_SM" + std::to_string(i), ImageLayoutType::Unknown, false);
							framebuffers.push_back(Framebuffer::Create({ shadowMaps[i] }, smSize, pipeline->GetRenderPassHandle()));
						}
						else if (smSize != framebuffers[i]->GetSize())
						{
							// Create SM & framebuffer with the new size
							shadowMaps[i] = CreateDepthImage(glm::uvec3(smSize, 1u), "SpotLight_SM" + std::to_string(i), ImageLayoutType::Unknown, false);
							framebuffers[i] = Framebuffer::Create({ shadowMaps[i] }, smSize, pipeline->GetRenderPassHandle());
						}

						const auto& viewProj = spotLight.ViewProj;

						cmd->BeginGraphics(pipeline, framebuffers[i]);
						cmd->SetGraphicsRootConstants(&viewProj, nullptr);

						uint32_t firstIndex = 0;
						uint32_t firstInstance = 0;
						uint32_t vertexOffset = 0;
						for (auto& [meshKey, datas] : meshes)
						{
							const uint32_t verticesCount = (uint32_t)meshKey.Mesh->GetVertices().size();
							const uint32_t indicesCount = (uint32_t)meshKey.Mesh->GetIndeces().size();
							const uint32_t instanceCount = (uint32_t)datas.size();

							if (meshKey.bCastsShadows)
							{
								stats.Indeces += indicesCount;
								stats.Vertices += verticesCount;
								++stats.DrawCalls;
								cmd->DrawIndexedInstanced(vb, ib, indicesCount, firstIndex, vertexOffset, instanceCount, firstInstance, ivb);
							}

							firstIndex += indicesCount;
							vertexOffset += verticesCount;
							firstInstance += instanceCount;
						}
						cmd->EndGraphics();
						++spotLightsCount;
					}
				}
			}
		
			// Release unused shadow-maps & framebuffers
			for (size_t i = spotLightsCount; i < framebuffers.size(); ++i)
				shadowMaps[i] = RenderManager::GetDummyDepthImage();
			framebuffers.resize(spotLightsCount);
		}
	}
	
	void ShadowPassTask::ShadowPassMaskedMeshes(const Ref<CommandBuffer>& cmd)
	{
		auto& meshes = m_Renderer.GetMaskedMeshes();
		if (meshes.empty())
			return;

		EG_GPU_TIMING_SCOPED(cmd, "Masked Meshes shadow pass");
		EG_CPU_TIMING_SCOPED("Masked Meshes shadow pass");

		const auto& meshesData = m_Renderer.GetMaskedMeshesData();
		const auto& vb = meshesData.VertexBuffer;
		const auto& ivb = meshesData.InstanceBuffer;
		const auto& ib = meshesData.IndexBuffer;
		const auto& transformsBuffer = m_Renderer.GetMeshTransformsBuffer();
		const auto& dirLight = m_Renderer.GetDirectionalLight();
		const glm::vec3 cameraPos = m_Renderer.GetViewPosition();
		const float shadowMaxDistance = m_Renderer.GetShadowMaxDistance();
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
				m_MaskedMDLPipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_PERSISTENT_SET, EG_BINDING_TEXTURES);
				m_MaskedMDLPipelineClearing->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_PERSISTENT_SET, EG_BINDING_TEXTURES);
				m_MaskedMeshesDLTexturesUpdatedFrames[currentFrameIndex] = texturesChangedFrame + 1;
			}
			pipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);

			for (uint32_t i = 0; i < m_DLFramebuffers.size(); ++i)
			{
				const auto& viewProj = dirLight.ViewProj[i];

				cmd->BeginGraphics(pipeline, m_DLFramebuffers[i]);
				cmd->SetGraphicsRootConstants(&viewProj, nullptr);

				uint32_t firstIndex = 0;
				uint32_t firstInstance = 0;
				uint32_t vertexOffset = 0;
				for (auto& [meshKey, datas] : meshes)
				{
					const uint32_t verticesCount = (uint32_t)meshKey.Mesh->GetVertices().size();
					const uint32_t indicesCount = (uint32_t)meshKey.Mesh->GetIndeces().size();
					const uint32_t instanceCount = (uint32_t)datas.size();

					if (meshKey.bCastsShadows)
					{
						stats.Indeces += indicesCount;
						stats.Vertices += verticesCount;
						++stats.DrawCalls;
						cmd->DrawIndexedInstanced(vb, ib, indicesCount, firstIndex, vertexOffset, instanceCount, firstInstance, ivb);
					}

					firstIndex += indicesCount;
					vertexOffset += verticesCount;
					firstInstance += instanceCount;
				}
				cmd->EndGraphics();
			}
			bDidDrawDL = true;
		}
		else
		{
			FreeDirectionalLightShadowMaps();
		}

		// For point lights
		{
			const auto& pointLights = m_Renderer.GetPointLights();
			uint32_t pointLightsCount = 0;
			auto& framebuffers = m_PLFramebuffers;
			auto& shadowMaps = m_PLShadowMaps;

			{
				auto& vpsBuffer = m_PLVPsBuffer;
				auto& pipeline = bDidDrawPL ? m_MaskedMPLPipeline : m_MaskedMPLPipelineClearing;
				pipeline->SetBuffer(transformsBuffer, 1, 0);
				pipeline->SetBuffer(vpsBuffer, 1, 1);

				const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
				const bool bTexturesDirty = texturesChangedFrame >= m_MaskedMeshesPLTexturesUpdatedFrames[currentFrameIndex];
				if (bTexturesDirty)
				{
					m_MaskedMPLPipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_PERSISTENT_SET, EG_BINDING_TEXTURES);
					m_MaskedMPLPipelineClearing->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_PERSISTENT_SET, EG_BINDING_TEXTURES);
					m_MaskedMeshesPLTexturesUpdatedFrames[currentFrameIndex] = texturesChangedFrame + 1;
				}
				pipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);

				{
					EG_GPU_TIMING_SCOPED(cmd, "Masked Meshes: Point Lights Shadow pass");
					EG_CPU_TIMING_SCOPED("Masked Meshes: Point Lights Shadow pass");

					for (auto& pointLight : pointLights)
					{
						if (!pointLight.DoesCastShadows())
							continue;

						bDidDrawPL = true;
						const float distanceToCamera = glm::length(cameraPos - pointLight.Position);
						const uint32_t& i = pointLightsCount;
						
						const glm::uvec2 smSize = GetPointLightSMSize(distanceToCamera, shadowMaxDistance);
						if (i >= framebuffers.size())
						{
							// Create SM & framebuffer
							shadowMaps[i] = CreateDepthImage(glm::uvec3(smSize, 1u), "PointLight_SM" + std::to_string(i), ImageLayoutType::Unknown, true);
							framebuffers.push_back(Framebuffer::Create({ shadowMaps[i] }, smSize, pipeline->GetRenderPassHandle()));
						}
						else if (smSize != framebuffers[i]->GetSize())
						{
							// Create SM & framebuffer with the new size
							shadowMaps[i] = CreateDepthImage(glm::uvec3(smSize, 1u), "PointLight_SM" + std::to_string(i), ImageLayoutType::Unknown, true);
							framebuffers[i] = Framebuffer::Create({shadowMaps[i]}, smSize, pipeline->GetRenderPassHandle());
						}

						cmd->TransitionLayout(vpsBuffer, BufferReadAccess::Uniform, BufferReadAccess::Uniform);
						cmd->Write(vpsBuffer, &pointLight.ViewProj[0][0], vpsBuffer->GetSize(), 0, BufferLayoutType::Unknown, BufferReadAccess::Uniform);
						cmd->TransitionLayout(vpsBuffer, BufferReadAccess::Uniform, BufferReadAccess::Uniform);

						cmd->BeginGraphics(pipeline, framebuffers[i]);

						uint32_t firstIndex = 0;
						uint32_t firstInstance = 0;
						uint32_t vertexOffset = 0;
						for (auto& [meshKey, datas] : meshes)
						{
							const uint32_t verticesCount = (uint32_t)meshKey.Mesh->GetVertices().size();
							const uint32_t indicesCount = (uint32_t)meshKey.Mesh->GetIndeces().size();
							const uint32_t instanceCount = (uint32_t)datas.size();

							if (meshKey.bCastsShadows)
							{
								stats.Indeces += indicesCount;
								stats.Vertices += verticesCount;
								++stats.DrawCalls;
								cmd->DrawIndexedInstanced(vb, ib, indicesCount, firstIndex, vertexOffset, instanceCount, firstInstance, ivb);
							}

							firstIndex += indicesCount;
							vertexOffset += verticesCount;
							firstInstance += instanceCount;
						}
						cmd->EndGraphics();
						++pointLightsCount;
					}

					// Release unused shadow-maps & framebuffers
					for (size_t i = pointLightsCount; i < framebuffers.size(); ++i)
						shadowMaps[i] = RenderManager::GetDummyDepthCubeImage();
					framebuffers.resize(pointLightsCount);
				}
			}
		}

		// For spot lights
		{
			const auto& spotLights = m_Renderer.GetSpotLights();
			uint32_t spotLightsCount = 0;
			auto& framebuffers = m_SLFramebuffers;
			auto& shadowMaps = m_SLShadowMaps;
			{
				auto& pipeline = bDidDrawSL ? m_MaskedMSLPipeline : m_MaskedMSLPipelineClearing;
				pipeline->SetBuffer(transformsBuffer, 1, 0);

				const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
				const bool bTexturesDirty = texturesChangedFrame >= m_MaskedMeshesSLTexturesUpdatedFrames[currentFrameIndex];
				if (bTexturesDirty)
				{
					m_MaskedMSLPipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_PERSISTENT_SET, EG_BINDING_TEXTURES);
					m_MaskedMSLPipelineClearing->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_PERSISTENT_SET, EG_BINDING_TEXTURES);
					m_MaskedMeshesSLTexturesUpdatedFrames[currentFrameIndex] = texturesChangedFrame + 1;
				}
				pipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);

				{
					EG_GPU_TIMING_SCOPED(cmd, "Masked Meshes: Spot Lights Shadow pass");
					EG_CPU_TIMING_SCOPED("Masked Meshes: Spot Lights Shadow pass");

					for (auto& spotLight : spotLights)
					{
						if (spotLight.bCastsShadows == 0)
							continue;
						
						bDidDrawSL = true;
						const float distanceToCamera = glm::length(cameraPos - spotLight.Position);
						const uint32_t& i = spotLightsCount;

						const glm::uvec2 smSize = GetSpotLightSMSize(distanceToCamera, shadowMaxDistance);
						if (i >= framebuffers.size())
						{
							// Create SM & framebuffer
							shadowMaps[i] = CreateDepthImage(glm::uvec3(smSize, 1u), "SpotLight_SM" + std::to_string(i), ImageLayoutType::Unknown, false);
							framebuffers.push_back(Framebuffer::Create({ shadowMaps[i] }, smSize, pipeline->GetRenderPassHandle()));
						}
						else if (smSize != framebuffers[i]->GetSize())
						{
							// Create SM & framebuffer with the new size
							shadowMaps[i] = CreateDepthImage(glm::uvec3(smSize, 1u), "SpotLight_SM" + std::to_string(i), ImageLayoutType::Unknown, false);
							framebuffers[i] = Framebuffer::Create({ shadowMaps[i] }, smSize, pipeline->GetRenderPassHandle());
						}

						const auto& viewProj = spotLight.ViewProj;

						cmd->BeginGraphics(pipeline, framebuffers[i]);
						cmd->SetGraphicsRootConstants(&viewProj, nullptr);

						uint32_t firstIndex = 0;
						uint32_t firstInstance = 0;
						uint32_t vertexOffset = 0;
						for (auto& [meshKey, datas] : meshes)
						{
							const uint32_t verticesCount = (uint32_t)meshKey.Mesh->GetVertices().size();
							const uint32_t indicesCount = (uint32_t)meshKey.Mesh->GetIndeces().size();
							const uint32_t instanceCount = (uint32_t)datas.size();

							if (meshKey.bCastsShadows)
							{
								stats.Indeces += indicesCount;
								stats.Vertices += verticesCount;
								++stats.DrawCalls;
								cmd->DrawIndexedInstanced(vb, ib, indicesCount, firstIndex, vertexOffset, instanceCount, firstInstance, ivb);
							}

							firstIndex += indicesCount;
							vertexOffset += verticesCount;
							firstInstance += instanceCount;
						}
						cmd->EndGraphics();
						++spotLightsCount;
					}
				}
			}
		
			// Release unused shadow-maps & framebuffers
			for (size_t i = spotLightsCount; i < framebuffers.size(); ++i)
				shadowMaps[i] = RenderManager::GetDummyDepthImage();
			framebuffers.resize(spotLightsCount);
		}
	}
	
	void ShadowPassTask::ShadowPassOpacitySprites(const Ref<CommandBuffer>& cmd)
	{
		const auto& spritesData = m_Renderer.GetOpaqueSpritesData();
		const auto& vertices = spritesData.QuadVertices;

		const uint32_t quadsCount = (uint32_t)(vertices.size() / 4);
		if (quadsCount == 0)
			return;

		EG_GPU_TIMING_SCOPED(cmd, "Opacity Sprites shadow pass");
		EG_CPU_TIMING_SCOPED("Opacity Sprites shadow pass");

		const auto& vb = spritesData.VertexBuffer;
		const auto& ib = spritesData.IndexBuffer;
		const auto& transformsBuffer = m_Renderer.GetSpritesTransformsBuffer();

		const glm::vec3 cameraPos = m_Renderer.GetViewPosition();
		const float shadowMaxDistance = m_Renderer.GetShadowMaxDistance();
		auto& stats = m_Renderer.GetStats2D();

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
				++stats.DrawCalls;
				stats.QuadCount += quadsCount;
				cmd->BeginGraphics(pipeline, m_DLFramebuffers[i]);
				cmd->SetGraphicsRootConstants(&dirLight.ViewProj[i], nullptr);
				cmd->DrawIndexed(vb, ib, quadsCount * 6, 0, 0);
				cmd->EndGraphics();
			}
			bDidDrawDL = true;
		}
		else
		{
			FreeDirectionalLightShadowMaps();
		}

		// Point lights
		{
			auto& pointLights = m_Renderer.GetPointLights();
			uint32_t pointLightsCount = 0;
			auto& shadowMaps = m_PLShadowMaps;
			auto& framebuffers = m_PLFramebuffers;

			if (pointLights.size())
			{
				EG_GPU_TIMING_SCOPED(cmd, "Opacity Sprites: Point Lights Shadow pass");
				EG_CPU_TIMING_SCOPED("Opacity Sprites: Point Lights Shadow pass");

				auto& vpsBuffer = m_PLVPsBuffer;
				auto& pipeline = bDidDrawPL ? m_OpacitySPLPipeline : m_OpacitySPLPipelineClearing;
				pipeline->SetBuffer(transformsBuffer, 0, 0);
				pipeline->SetBuffer(vpsBuffer, 0, 1);
				bool bDidDraw = false;

				for (auto& pointLight : pointLights)
				{
					if (!pointLight.DoesCastShadows())
						continue;

					bDidDraw = true;
					const float distanceToCamera = glm::length(cameraPos - pointLight.Position);
					const uint32_t& i = pointLightsCount;
					const glm::uvec2 smSize = GetPointLightSMSize(distanceToCamera, shadowMaxDistance);
					if (i >= framebuffers.size())
					{
						// Create SM & framebuffer
						shadowMaps[i] = CreateDepthImage(glm::uvec3(smSize, 1u), "PointLight_SM" + std::to_string(i), ImageLayoutType::Unknown, true);
						framebuffers.push_back(Framebuffer::Create({ shadowMaps[i] }, smSize, pipeline->GetRenderPassHandle()));
					}
					else if (smSize != framebuffers[i]->GetSize())
					{
						// Create SM & framebuffer with the new size
						shadowMaps[i] = CreateDepthImage(glm::uvec3(smSize, 1u), "PointLight_SM" + std::to_string(i), ImageLayoutType::Unknown, true);
						framebuffers[i] = Framebuffer::Create({ shadowMaps[i] }, smSize, pipeline->GetRenderPassHandle());
					}

					cmd->TransitionLayout(vpsBuffer, BufferReadAccess::Uniform, BufferReadAccess::Uniform);
					cmd->Write(vpsBuffer, &pointLight.ViewProj[0][0], vpsBuffer->GetSize(), 0, BufferLayoutType::Unknown, BufferReadAccess::Uniform);
					cmd->TransitionLayout(vpsBuffer, BufferReadAccess::Uniform, BufferReadAccess::Uniform);

					cmd->BeginGraphics(pipeline, framebuffers[i]);
					cmd->DrawIndexed(vb, ib, quadsCount * 6, 0, 0);
					cmd->EndGraphics();
					++pointLightsCount;
					++stats.DrawCalls;
					stats.QuadCount += quadsCount;
				}

				bDidDrawPL |= bDidDraw;
				// Release unused framebuffers
				framebuffers.resize(pointLightsCount);
			}
			// Release unused shadow-maps & framebuffers
			for (size_t i = pointLightsCount; i < framebuffers.size(); ++i)
				shadowMaps[i] = RenderManager::GetDummyDepthCubeImage();
			framebuffers.resize(pointLightsCount);
		}

		// Spot lights
		{
			auto& spotLights = m_Renderer.GetSpotLights();
			uint32_t spotLightsCount = 0;
			auto& shadowMaps = m_SLShadowMaps;
			auto& framebuffers = m_SLFramebuffers;

			if (spotLights.size())
			{
				EG_GPU_TIMING_SCOPED(cmd, "Opacity Sprites: Spot Lights Shadow pass");
				EG_CPU_TIMING_SCOPED("Opacity Sprites: Spot Lights Shadow pass");

				auto& pipeline = bDidDrawSL ? m_OpacitySSLPipeline : m_OpacitySSLPipelineClearing;
				pipeline->SetBuffer(transformsBuffer, 0, 0);
				bool bDidDraw = false;

				for (auto& spotLight : spotLights)
				{
					if (spotLight.bCastsShadows == 0)
						continue;

					bDidDraw = true;
					const float distanceToCamera = glm::length(cameraPos - spotLight.Position);
					const uint32_t& i = spotLightsCount;

					const glm::uvec2 smSize = GetSpotLightSMSize(distanceToCamera, shadowMaxDistance);
					if (i >= framebuffers.size())
					{
						// Create SM & framebuffer
						shadowMaps[i] = CreateDepthImage(glm::uvec3(smSize, 1u), "SpotLight_SM" + std::to_string(i), ImageLayoutType::Unknown, false);
						framebuffers.push_back(Framebuffer::Create({ shadowMaps[i] }, smSize, pipeline->GetRenderPassHandle()));
					}
					else if (smSize != framebuffers[i]->GetSize())
					{
						// Create SM & framebuffer with the new size
						shadowMaps[i] = CreateDepthImage(glm::uvec3(smSize, 1u), "SpotLight_SM" + std::to_string(i), ImageLayoutType::Unknown, false);
						framebuffers[i] = Framebuffer::Create({ shadowMaps[i] }, smSize, pipeline->GetRenderPassHandle());
					}

					cmd->BeginGraphics(pipeline, framebuffers[i]);
					cmd->SetGraphicsRootConstants(&spotLight.ViewProj, nullptr);
					cmd->DrawIndexed(vb, ib, quadsCount * 6, 0, 0);
					cmd->EndGraphics();
					++spotLightsCount;
					++stats.DrawCalls;
					stats.QuadCount += quadsCount;
				}
				
				bDidDrawSL |= bDidDraw;
			}
			// Release unused shadow-maps & framebuffers
			for (size_t i = spotLightsCount; i < framebuffers.size(); ++i)
				shadowMaps[i] = RenderManager::GetDummyDepthImage();
			framebuffers.resize(spotLightsCount);
		}
	}
	
	void ShadowPassTask::ShadowPassMaskedSprites(const Ref<CommandBuffer>& cmd)
	{
		const auto& spritesData = m_Renderer.GetMaskedSpritesData();
		const auto& vertices = spritesData.QuadVertices;

		const uint32_t quadsCount = (uint32_t)(vertices.size() / 4);
		if (quadsCount == 0)
			return;

		EG_GPU_TIMING_SCOPED(cmd, "Masked Sprites shadow pass");
		EG_CPU_TIMING_SCOPED("Masked Sprites shadow pass");

		const auto& vb = spritesData.VertexBuffer;
		const auto& ib = spritesData.IndexBuffer;
		const auto& transformsBuffer = m_Renderer.GetSpritesTransformsBuffer();

		const glm::vec3 cameraPos = m_Renderer.GetViewPosition();
		const float shadowMaxDistance = m_Renderer.GetShadowMaxDistance();
		auto& stats = m_Renderer.GetStats2D();

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
				m_MaskedSDLPipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_PERSISTENT_SET, EG_BINDING_TEXTURES);
				m_MaskedSDLPipelineClearing->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_PERSISTENT_SET, EG_BINDING_TEXTURES);
				m_MaskedSpritesDLTexturesUpdatedFrames[currentFrameIndex] = texturesChangedFrame + 1;
			}
			pipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);

			pipeline->SetBuffer(transformsBuffer, 1, 0);
			for (uint32_t i = 0; i < m_DLFramebuffers.size(); ++i)
			{
				++stats.DrawCalls;
				stats.QuadCount += quadsCount;
				cmd->BeginGraphics(pipeline, m_DLFramebuffers[i]);
				cmd->SetGraphicsRootConstants(&dirLight.ViewProj[i], nullptr);
				cmd->DrawIndexed(vb, ib, quadsCount * 6, 0, 0);
				cmd->EndGraphics();
			}
			bDidDrawDL = true;
		}
		else
		{
			FreeDirectionalLightShadowMaps();
		}

		// Point lights
		{
			auto& pointLights = m_Renderer.GetPointLights();
			uint32_t pointLightsCount = 0;
			auto& shadowMaps = m_PLShadowMaps;
			auto& framebuffers = m_PLFramebuffers;

			if (pointLights.size())
			{
				EG_GPU_TIMING_SCOPED(cmd, "Masked Sprites: Point Lights Shadow pass");
				EG_CPU_TIMING_SCOPED("Masked Sprites: Point Lights Shadow pass");

				auto& vpsBuffer = m_PLVPsBuffer;
				auto& pipeline = bDidDrawPL ? m_MaskedSPLPipeline : m_MaskedSPLPipelineClearing;

				const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
				const bool bTexturesDirty = texturesChangedFrame >= m_MaskedSpritesPLTexturesUpdatedFrames[currentFrameIndex];
				if (bTexturesDirty)
				{
					m_MaskedSPLPipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_PERSISTENT_SET, EG_BINDING_TEXTURES);
					m_MaskedSPLPipelineClearing->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_PERSISTENT_SET, EG_BINDING_TEXTURES);
					m_MaskedSpritesPLTexturesUpdatedFrames[currentFrameIndex] = texturesChangedFrame + 1;
				}
				pipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);

				pipeline->SetBuffer(transformsBuffer, 1, 0);
				pipeline->SetBuffer(vpsBuffer, 1, 1);
				bool bDidDraw = false;

				for (auto& pointLight : pointLights)
				{
					if (!pointLight.DoesCastShadows())
						continue;

					bDidDraw = true;
					const float distanceToCamera = glm::length(cameraPos - pointLight.Position);
					const uint32_t& i = pointLightsCount;
					const glm::uvec2 smSize = GetPointLightSMSize(distanceToCamera, shadowMaxDistance);
					if (i >= framebuffers.size())
					{
						// Create SM & framebuffer
						shadowMaps[i] = CreateDepthImage(glm::uvec3(smSize, 1u), "PointLight_SM" + std::to_string(i), ImageLayoutType::Unknown, true);
						framebuffers.push_back(Framebuffer::Create({ shadowMaps[i] }, smSize, pipeline->GetRenderPassHandle()));
					}
					else if (smSize != framebuffers[i]->GetSize())
					{
						// Create SM & framebuffer with the new size
						shadowMaps[i] = CreateDepthImage(glm::uvec3(smSize, 1u), "PointLight_SM" + std::to_string(i), ImageLayoutType::Unknown, true);
						framebuffers[i] = Framebuffer::Create({ shadowMaps[i] }, smSize, pipeline->GetRenderPassHandle());
					}

					cmd->TransitionLayout(vpsBuffer, BufferReadAccess::Uniform, BufferReadAccess::Uniform);
					cmd->Write(vpsBuffer, &pointLight.ViewProj[0][0], vpsBuffer->GetSize(), 0, BufferLayoutType::Unknown, BufferReadAccess::Uniform);
					cmd->TransitionLayout(vpsBuffer, BufferReadAccess::Uniform, BufferReadAccess::Uniform);

					cmd->BeginGraphics(pipeline, framebuffers[i]);
					cmd->DrawIndexed(vb, ib, quadsCount * 6, 0, 0);
					cmd->EndGraphics();
					++pointLightsCount;
					++stats.DrawCalls;
					stats.QuadCount += quadsCount;
				}

				bDidDrawPL |= bDidDraw;
				// Release unused framebuffers
				framebuffers.resize(pointLightsCount);
			}
			// Release unused shadow-maps & framebuffers
			for (size_t i = pointLightsCount; i < framebuffers.size(); ++i)
				shadowMaps[i] = RenderManager::GetDummyDepthCubeImage();
			framebuffers.resize(pointLightsCount);
		}

		// Spot lights
		{
			auto& spotLights = m_Renderer.GetSpotLights();
			uint32_t spotLightsCount = 0;
			auto& shadowMaps = m_SLShadowMaps;
			auto& framebuffers = m_SLFramebuffers;

			if (spotLights.size())
			{
				EG_GPU_TIMING_SCOPED(cmd, "Masked Sprites: Spot Lights Shadow pass");
				EG_CPU_TIMING_SCOPED("Masked Sprites: Spot Lights Shadow pass");

				auto& pipeline = bDidDrawSL ? m_MaskedSSLPipeline : m_MaskedSSLPipelineClearing;

				const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
				const bool bTexturesDirty = texturesChangedFrame >= m_MaskedSpritesSLTexturesUpdatedFrames[currentFrameIndex];
				if (bTexturesDirty)
				{
					m_MaskedSSLPipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_PERSISTENT_SET, EG_BINDING_TEXTURES);
					m_MaskedSSLPipelineClearing->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_PERSISTENT_SET, EG_BINDING_TEXTURES);
					m_MaskedSpritesSLTexturesUpdatedFrames[currentFrameIndex] = texturesChangedFrame + 1;
				}
				pipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);

				pipeline->SetBuffer(transformsBuffer, 1, 0);
				bool bDidDraw = false;

				for (auto& spotLight : spotLights)
				{
					if (spotLight.bCastsShadows == 0)
						continue;

					bDidDraw = true;
					const float distanceToCamera = glm::length(cameraPos - spotLight.Position);
					const uint32_t& i = spotLightsCount;

					const glm::uvec2 smSize = GetSpotLightSMSize(distanceToCamera, shadowMaxDistance);
					if (i >= framebuffers.size())
					{
						// Create SM & framebuffer
						shadowMaps[i] = CreateDepthImage(glm::uvec3(smSize, 1u), "SpotLight_SM" + std::to_string(i), ImageLayoutType::Unknown, false);
						framebuffers.push_back(Framebuffer::Create({ shadowMaps[i] }, smSize, pipeline->GetRenderPassHandle()));
					}
					else if (smSize != framebuffers[i]->GetSize())
					{
						// Create SM & framebuffer with the new size
						shadowMaps[i] = CreateDepthImage(glm::uvec3(smSize, 1u), "SpotLight_SM" + std::to_string(i), ImageLayoutType::Unknown, false);
						framebuffers[i] = Framebuffer::Create({ shadowMaps[i] }, smSize, pipeline->GetRenderPassHandle());
					}

					cmd->BeginGraphics(pipeline, framebuffers[i]);
					cmd->SetGraphicsRootConstants(&spotLight.ViewProj, nullptr);
					cmd->DrawIndexed(vb, ib, quadsCount * 6, 0, 0);
					cmd->EndGraphics();
					++spotLightsCount;
					++stats.DrawCalls;
					stats.QuadCount += quadsCount;
				}
				
				bDidDrawSL |= bDidDraw;
			}
			// Release unused shadow-maps & framebuffers
			for (size_t i = spotLightsCount; i < framebuffers.size(); ++i)
				shadowMaps[i] = RenderManager::GetDummyDepthImage();
			framebuffers.resize(spotLightsCount);
		}
	}

	void ShadowPassTask::ShadowPassOpaqueLitTexts(const Ref<CommandBuffer>& cmd)
	{
		const auto& textData = m_Renderer.GetOpaqueLitTextData();
		const auto& vertices = textData.QuadVertices;

		const uint32_t quadsCount = (uint32_t)(vertices.size() / 4);
		if (quadsCount == 0)
			return;

		EG_GPU_TIMING_SCOPED(cmd, "Opaque Lit Texts shadow pass");
		EG_CPU_TIMING_SCOPED("Opaque Lit Texts shadow pass");

		const auto& vb = textData.VertexBuffer;
		const auto& ib = textData.IndexBuffer;
		const auto& transformsBuffer = m_Renderer.GetTextsTransformsBuffer();

		const glm::vec3 cameraPos = m_Renderer.GetViewPosition();
		const float shadowMaxDistance = m_Renderer.GetShadowMaxDistance();
		auto& stats = m_Renderer.GetStats2D();

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
				++stats.DrawCalls;
				stats.QuadCount += quadsCount;
				cmd->BeginGraphics(pipeline, m_DLFramebuffers[i]);
				cmd->SetGraphicsRootConstants(&dirLight.ViewProj[i], nullptr);
				cmd->DrawIndexed(vb, ib, quadsCount * 6, 0, 0);
				cmd->EndGraphics();
			}
			bDidDrawDL = true;
		}
		else
		{
			FreeDirectionalLightShadowMaps();
		}

		// Point lights
		{
			auto& pointLights = m_Renderer.GetPointLights();
			uint32_t pointLightsCount = 0;
			auto& shadowMaps = m_PLShadowMaps;
			auto& framebuffers = m_PLFramebuffers;

			if (pointLights.size())
			{
				EG_GPU_TIMING_SCOPED(cmd, "Opaque Lit Texts: Point Lights Shadow pass");
				EG_CPU_TIMING_SCOPED("Opaque Lit Texts: Point Lights Shadow pass");

				auto& vpsBuffer = m_PLVPsBuffer;
				auto& pipeline = bDidDrawPL ? m_OpaqueLitTPLPipeline : m_OpaqueLitTPLPipelineClearing;
				pipeline->SetBuffer(transformsBuffer, 0, 0);
				pipeline->SetBuffer(vpsBuffer, 0, 1);
				pipeline->SetTextureArray(m_Renderer.GetAtlases(), 1, 0);
				bool bDidDraw = false;

				for (auto& pointLight : pointLights)
				{
					if (!pointLight.DoesCastShadows())
						continue;

					bDidDraw = true;
					const float distanceToCamera = glm::length(cameraPos - pointLight.Position);
					const uint32_t& i = pointLightsCount;
					const glm::uvec2 smSize = GetPointLightSMSize(distanceToCamera, shadowMaxDistance);
					if (i >= framebuffers.size())
					{
						// Create SM & framebuffer
						shadowMaps[i] = CreateDepthImage(glm::uvec3(smSize, 1u), "PointLight_SM" + std::to_string(i), ImageLayoutType::Unknown, true);
						framebuffers.push_back(Framebuffer::Create({ shadowMaps[i] }, smSize, pipeline->GetRenderPassHandle()));
					}
					else if (smSize != framebuffers[i]->GetSize())
					{
						// Create SM & framebuffer with the new size
						shadowMaps[i] = CreateDepthImage(glm::uvec3(smSize, 1u), "PointLight_SM" + std::to_string(i), ImageLayoutType::Unknown, true);
						framebuffers[i] = Framebuffer::Create({ shadowMaps[i] }, smSize, pipeline->GetRenderPassHandle());
					}

					cmd->TransitionLayout(vpsBuffer, BufferReadAccess::Uniform, BufferReadAccess::Uniform);
					cmd->Write(vpsBuffer, &pointLight.ViewProj[0][0], vpsBuffer->GetSize(), 0, BufferLayoutType::Unknown, BufferReadAccess::Uniform);
					cmd->TransitionLayout(vpsBuffer, BufferReadAccess::Uniform, BufferReadAccess::Uniform);

					cmd->BeginGraphics(pipeline, framebuffers[i]);
					cmd->DrawIndexed(vb, ib, quadsCount * 6, 0, 0);
					cmd->EndGraphics();
					++pointLightsCount;
					++stats.DrawCalls;
					stats.QuadCount += quadsCount;
				}
				bDidDrawPL |= bDidDraw;

				// Release unused framebuffers
				framebuffers.resize(pointLightsCount);
			}
			// Release unused shadow-maps & framebuffers
			for (size_t i = pointLightsCount; i < framebuffers.size(); ++i)
				shadowMaps[i] = RenderManager::GetDummyDepthCubeImage();
			framebuffers.resize(pointLightsCount);
		}

		// Spot lights
		{
			auto& spotLights = m_Renderer.GetSpotLights();
			uint32_t spotLightsCount = 0;
			auto& shadowMaps = m_SLShadowMaps;
			auto& framebuffers = m_SLFramebuffers;

			if (spotLights.size())
			{
				EG_GPU_TIMING_SCOPED(cmd, "Opaque Lit Texts: Spot Lights Shadow pass");
				EG_CPU_TIMING_SCOPED("Opaque Lit Texts: Spot Lights Shadow pass");

				auto& pipeline = bDidDrawSL ? m_OpaqueLitTSLPipeline : m_OpaqueLitTSLPipelineClearing;
				pipeline->SetBuffer(transformsBuffer, 0, 0);
				pipeline->SetTextureArray(m_Renderer.GetAtlases(), 1, 0);
				bool bDidDraw = false;

				for (auto& spotLight : spotLights)
				{
					if (spotLight.bCastsShadows == 0)
						continue;

					bDidDraw = true;
					const float distanceToCamera = glm::length(cameraPos - spotLight.Position);
					const uint32_t& i = spotLightsCount;

					const glm::uvec2 smSize = GetSpotLightSMSize(distanceToCamera, shadowMaxDistance);
					if (i >= framebuffers.size())
					{
						// Create SM & framebuffer
						shadowMaps[i] = CreateDepthImage(glm::uvec3(smSize, 1u), "SpotLight_SM" + std::to_string(i), ImageLayoutType::Unknown, false);
						framebuffers.push_back(Framebuffer::Create({ shadowMaps[i] }, smSize, pipeline->GetRenderPassHandle()));
					}
					else if (smSize != framebuffers[i]->GetSize())
					{
						// Create SM & framebuffer with the new size
						shadowMaps[i] = CreateDepthImage(glm::uvec3(smSize, 1u), "SpotLight_SM" + std::to_string(i), ImageLayoutType::Unknown, false);
						framebuffers[i] = Framebuffer::Create({ shadowMaps[i] }, smSize, pipeline->GetRenderPassHandle());
					}

					cmd->BeginGraphics(pipeline, framebuffers[i]);
					cmd->SetGraphicsRootConstants(&spotLight.ViewProj, nullptr);
					cmd->DrawIndexed(vb, ib, quadsCount * 6, 0, 0);
					cmd->EndGraphics();
					++spotLightsCount;
					++stats.DrawCalls;
					stats.QuadCount += quadsCount;
				}
				bDidDrawSL |= bDidDraw;
			}
			// Release unused shadow-maps & framebuffers
			for (size_t i = spotLightsCount; i < framebuffers.size(); ++i)
				shadowMaps[i] = RenderManager::GetDummyDepthImage();
			framebuffers.resize(spotLightsCount);
		}
	}
	
	void ShadowPassTask::ShadowPassMaskedLitTexts(const Ref<CommandBuffer>& cmd)
	{
		const auto& textData = m_Renderer.GetMaskedLitTextData();
		const auto& vertices = textData.QuadVertices;

		const uint32_t quadsCount = (uint32_t)(vertices.size() / 4);
		if (quadsCount == 0)
			return;

		EG_GPU_TIMING_SCOPED(cmd, "Masked Lit Texts shadow pass");
		EG_CPU_TIMING_SCOPED("Masked Lit Texts shadow pass");

		const auto& vb = textData.VertexBuffer;
		const auto& ib = textData.IndexBuffer;
		const auto& transformsBuffer = m_Renderer.GetTextsTransformsBuffer();

		const glm::vec3 cameraPos = m_Renderer.GetViewPosition();
		const float shadowMaxDistance = m_Renderer.GetShadowMaxDistance();
		auto& stats = m_Renderer.GetStats2D();

		// For directional light
		const auto& dirLight = m_Renderer.GetDirectionalLight();
		if (m_Renderer.HasDirectionalLight() && dirLight.bCastsShadows)
		{
			EG_GPU_TIMING_SCOPED(cmd, "Masked Lit Texts: CSM Shadow pass");
			EG_CPU_TIMING_SCOPED("Masked Lit Texts: CSM Shadow pass");

			CreateIfNeededDirectionalLightShadowMaps();

			auto& pipeline = bDidDrawDL ? m_MaskedLitTDLPipeline : m_MaskedLitTDLPipelineClearing;
			pipeline->SetBuffer(transformsBuffer, 0, 0);
			pipeline->SetTextureArray(m_Renderer.GetAtlases(), 1, 0);
			for (uint32_t i = 0; i < m_DLFramebuffers.size(); ++i)
			{
				++stats.DrawCalls;
				stats.QuadCount += quadsCount;
				cmd->BeginGraphics(pipeline, m_DLFramebuffers[i]);
				cmd->SetGraphicsRootConstants(&dirLight.ViewProj[i], nullptr);
				cmd->DrawIndexed(vb, ib, quadsCount * 6, 0, 0);
				cmd->EndGraphics();
			}
			bDidDrawDL = true;
		}
		else
		{
			FreeDirectionalLightShadowMaps();
		}

		// Point lights
		{
			auto& pointLights = m_Renderer.GetPointLights();
			uint32_t pointLightsCount = 0;
			auto& shadowMaps = m_PLShadowMaps;
			auto& framebuffers = m_PLFramebuffers;

			if (pointLights.size())
			{
				EG_GPU_TIMING_SCOPED(cmd, "Masked Lit Texts: Point Lights Shadow pass");
				EG_CPU_TIMING_SCOPED("Masked Lit Texts: Point Lights Shadow pass");

				auto& vpsBuffer = m_PLVPsBuffer;
				auto& pipeline = bDidDrawPL ? m_MaskedLitTPLPipeline : m_MaskedLitTPLPipelineClearing;
				pipeline->SetBuffer(transformsBuffer, 0, 0);
				pipeline->SetBuffer(vpsBuffer, 0, 1);
				pipeline->SetTextureArray(m_Renderer.GetAtlases(), 1, 0);
				bool bDidDraw = false;

				for (auto& pointLight : pointLights)
				{
					if (!pointLight.DoesCastShadows())
						continue;

					bDidDraw = true;
					const float distanceToCamera = glm::length(cameraPos - pointLight.Position);
					const uint32_t& i = pointLightsCount;
					const glm::uvec2 smSize = GetPointLightSMSize(distanceToCamera, shadowMaxDistance);
					if (i >= framebuffers.size())
					{
						// Create SM & framebuffer
						shadowMaps[i] = CreateDepthImage(glm::uvec3(smSize, 1u), "PointLight_SM" + std::to_string(i), ImageLayoutType::Unknown, true);
						framebuffers.push_back(Framebuffer::Create({ shadowMaps[i] }, smSize, pipeline->GetRenderPassHandle()));
					}
					else if (smSize != framebuffers[i]->GetSize())
					{
						// Create SM & framebuffer with the new size
						shadowMaps[i] = CreateDepthImage(glm::uvec3(smSize, 1u), "PointLight_SM" + std::to_string(i), ImageLayoutType::Unknown, true);
						framebuffers[i] = Framebuffer::Create({ shadowMaps[i] }, smSize, pipeline->GetRenderPassHandle());
					}

					cmd->TransitionLayout(vpsBuffer, BufferReadAccess::Uniform, BufferReadAccess::Uniform);
					cmd->Write(vpsBuffer, &pointLight.ViewProj[0][0], vpsBuffer->GetSize(), 0, BufferLayoutType::Unknown, BufferReadAccess::Uniform);
					cmd->TransitionLayout(vpsBuffer, BufferReadAccess::Uniform, BufferReadAccess::Uniform);

					cmd->BeginGraphics(pipeline, framebuffers[i]);
					cmd->DrawIndexed(vb, ib, quadsCount * 6, 0, 0);
					cmd->EndGraphics();
					++pointLightsCount;
					++stats.DrawCalls;
					stats.QuadCount += quadsCount;
				}
				bDidDrawPL |= bDidDraw;

				// Release unused framebuffers
				framebuffers.resize(pointLightsCount);
			}
			// Release unused shadow-maps & framebuffers
			for (size_t i = pointLightsCount; i < framebuffers.size(); ++i)
				shadowMaps[i] = RenderManager::GetDummyDepthCubeImage();
			framebuffers.resize(pointLightsCount);
		}

		// Spot lights
		{
			auto& spotLights = m_Renderer.GetSpotLights();
			uint32_t spotLightsCount = 0;
			auto& shadowMaps = m_SLShadowMaps;
			auto& framebuffers = m_SLFramebuffers;

			if (spotLights.size())
			{
				EG_GPU_TIMING_SCOPED(cmd, "Masked Lit Texts: Spot Lights Shadow pass");
				EG_CPU_TIMING_SCOPED("Masked Lit Texts: Spot Lights Shadow pass");

				auto& pipeline = bDidDrawSL ? m_MaskedLitTSLPipeline : m_MaskedLitTSLPipelineClearing;
				pipeline->SetBuffer(transformsBuffer, 0, 0);
				pipeline->SetTextureArray(m_Renderer.GetAtlases(), 1, 0);
				bool bDidDraw = false;

				for (auto& spotLight : spotLights)
				{
					if (spotLight.bCastsShadows == 0)
						continue;

					bDidDraw = true;
					const float distanceToCamera = glm::length(cameraPos - spotLight.Position);
					const uint32_t& i = spotLightsCount;

					const glm::uvec2 smSize = GetSpotLightSMSize(distanceToCamera, shadowMaxDistance);
					if (i >= framebuffers.size())
					{
						// Create SM & framebuffer
						shadowMaps[i] = CreateDepthImage(glm::uvec3(smSize, 1u), "SpotLight_SM" + std::to_string(i), ImageLayoutType::Unknown, false);
						framebuffers.push_back(Framebuffer::Create({ shadowMaps[i] }, smSize, pipeline->GetRenderPassHandle()));
					}
					else if (smSize != framebuffers[i]->GetSize())
					{
						// Create SM & framebuffer with the new size
						shadowMaps[i] = CreateDepthImage(glm::uvec3(smSize, 1u), "SpotLight_SM" + std::to_string(i), ImageLayoutType::Unknown, false);
						framebuffers[i] = Framebuffer::Create({ shadowMaps[i] }, smSize, pipeline->GetRenderPassHandle());
					}

					cmd->BeginGraphics(pipeline, framebuffers[i]);
					cmd->SetGraphicsRootConstants(&spotLight.ViewProj, nullptr);
					cmd->DrawIndexed(vb, ib, quadsCount * 6, 0, 0);
					cmd->EndGraphics();
					++spotLightsCount;
					++stats.DrawCalls;
					stats.QuadCount += quadsCount;
				}
				bDidDrawSL |= bDidDraw;
			}
			// Release unused shadow-maps & framebuffers
			for (size_t i = spotLightsCount; i < framebuffers.size(); ++i)
				shadowMaps[i] = RenderManager::GetDummyDepthImage();
			framebuffers.resize(spotLightsCount);
		}
	}

	void ShadowPassTask::ShadowPassUnlitTexts(const Ref<CommandBuffer>& cmd)
	{
		const auto& textData = m_Renderer.GetUnlitTextData();
		const auto& vertices = textData.QuadVertices;

		const uint32_t quadsCount = (uint32_t)(vertices.size() / 4);
		if (quadsCount == 0)
			return;

		EG_GPU_TIMING_SCOPED(cmd, "Unlit Texts shadow pass");
		EG_CPU_TIMING_SCOPED("Unlit Texts shadow pass");

		const auto& vb = textData.VertexBuffer;
		const auto& ib = textData.IndexBuffer;
		const auto& transformsBuffer = m_Renderer.GetTextsTransformsBuffer();

		const glm::vec3 cameraPos = m_Renderer.GetViewPosition();
		const float shadowMaxDistance = m_Renderer.GetShadowMaxDistance();
		auto& stats = m_Renderer.GetStats2D();

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
				cmd->BeginGraphics(pipeline, m_DLFramebuffers[i]);
				cmd->SetGraphicsRootConstants(&dirLight.ViewProj[i], nullptr);
				cmd->DrawIndexed(vb, ib, quadsCount * 6, 0, 0);
				cmd->EndGraphics();
				++stats.DrawCalls;
				stats.QuadCount += quadsCount;
			}
			bDidDrawDL = true;
		}
		else
		{
			FreeDirectionalLightShadowMaps();
		}

		// Point lights
		{
			auto& pointLights = m_Renderer.GetPointLights();
			uint32_t pointLightsCount = 0;
			auto& shadowMaps = m_PLShadowMaps;
			auto& framebuffers = m_PLFramebuffers;

			if (pointLights.size())
			{
				EG_GPU_TIMING_SCOPED(cmd, "Unlit Texts: Point Lights Shadow pass");
				EG_CPU_TIMING_SCOPED("Unlit Texts: Point Lights Shadow pass");

				auto& vpsBuffer = m_PLVPsBuffer;
				auto& pipeline = bDidDrawPL ? m_UnlitTPLPipeline : m_UnlitTPLPipelineClearing;
				pipeline->SetBuffer(transformsBuffer, 0, 0);
				pipeline->SetBuffer(vpsBuffer, 0, 1);
				pipeline->SetTextureArray(m_Renderer.GetAtlases(), 1, 0);
				bool bDidDraw = false;

				for (auto& pointLight : pointLights)
				{
					if (!pointLight.DoesCastShadows())
						continue;

					bDidDraw = true;
					const float distanceToCamera = glm::length(cameraPos - pointLight.Position);
					const uint32_t& i = pointLightsCount;
					const glm::uvec2 smSize = GetPointLightSMSize(distanceToCamera, shadowMaxDistance);
					if (i >= framebuffers.size())
					{
						// Create SM & framebuffer
						shadowMaps[i] = CreateDepthImage(glm::uvec3(smSize, 1u), "PointLight_SM" + std::to_string(i), ImageLayoutType::Unknown, true);
						framebuffers.push_back(Framebuffer::Create({ shadowMaps[i] }, smSize, pipeline->GetRenderPassHandle()));
					}
					else if (smSize != framebuffers[i]->GetSize())
					{
						// Create SM & framebuffer with the new size
						shadowMaps[i] = CreateDepthImage(glm::uvec3(smSize, 1u), "PointLight_SM" + std::to_string(i), ImageLayoutType::Unknown, true);
						framebuffers[i] = Framebuffer::Create({ shadowMaps[i] }, smSize, pipeline->GetRenderPassHandle());
					}

					cmd->TransitionLayout(vpsBuffer, BufferReadAccess::Uniform, BufferReadAccess::Uniform);
					cmd->Write(vpsBuffer, &pointLight.ViewProj[0][0], vpsBuffer->GetSize(), 0, BufferLayoutType::Unknown, BufferReadAccess::Uniform);
					cmd->TransitionLayout(vpsBuffer, BufferReadAccess::Uniform, BufferReadAccess::Uniform);

					cmd->BeginGraphics(pipeline, framebuffers[i]);
					cmd->DrawIndexed(vb, ib, quadsCount * 6, 0, 0);
					cmd->EndGraphics();
					++pointLightsCount;
					++stats.DrawCalls;
					stats.QuadCount += quadsCount;
				}
				bDidDrawPL |= bDidDraw;

				// Release unused framebuffers
				framebuffers.resize(pointLightsCount);
			}
			// Release unused shadow-maps & framebuffers
			for (size_t i = pointLightsCount; i < framebuffers.size(); ++i)
				shadowMaps[i] = RenderManager::GetDummyDepthCubeImage();
			framebuffers.resize(pointLightsCount);
		}

		// Spot lights
		{
			auto& spotLights = m_Renderer.GetSpotLights();
			uint32_t spotLightsCount = 0;
			auto& shadowMaps = m_SLShadowMaps;
			auto& framebuffers = m_SLFramebuffers;

			if (spotLights.size())
			{
				EG_GPU_TIMING_SCOPED(cmd, "Unlit Texts: Spot Lights Shadow pass");
				EG_CPU_TIMING_SCOPED("Unlit Texts: Spot Lights Shadow pass");

				auto& pipeline = bDidDrawSL ? m_UnlitTSLPipeline : m_UnlitTSLPipelineClearing;
				pipeline->SetBuffer(transformsBuffer, 0, 0);
				pipeline->SetTextureArray(m_Renderer.GetAtlases(), 1, 0);
				bool bDidDraw = false;

				for (auto& spotLight : spotLights)
				{
					if (spotLight.bCastsShadows == 0)
						continue;

					bDidDraw = true;
					const float distanceToCamera = glm::length(cameraPos - spotLight.Position);
					const uint32_t& i = spotLightsCount;

					const glm::uvec2 smSize = GetSpotLightSMSize(distanceToCamera, shadowMaxDistance);
					if (i >= framebuffers.size())
					{
						// Create SM & framebuffer
						shadowMaps[i] = CreateDepthImage(glm::uvec3(smSize, 1u), "SpotLight_SM" + std::to_string(i), ImageLayoutType::Unknown, false);
						framebuffers.push_back(Framebuffer::Create({ shadowMaps[i] }, smSize, pipeline->GetRenderPassHandle()));
					}
					else if (smSize != framebuffers[i]->GetSize())
					{
						// Create SM & framebuffer with the new size
						shadowMaps[i] = CreateDepthImage(glm::uvec3(smSize, 1u), "SpotLight_SM" + std::to_string(i), ImageLayoutType::Unknown, false);
						framebuffers[i] = Framebuffer::Create({ shadowMaps[i] }, smSize, pipeline->GetRenderPassHandle());
					}

					cmd->BeginGraphics(pipeline, framebuffers[i]);
					cmd->SetGraphicsRootConstants(&spotLight.ViewProj, nullptr);
					cmd->DrawIndexed(vb, ib, quadsCount * 6, 0, 0);
					cmd->EndGraphics();
					++spotLightsCount;
					++stats.DrawCalls;
					stats.QuadCount += quadsCount;
				}
				bDidDrawSL |= bDidDraw;
			}
			// Release unused shadow-maps & framebuffers
			for (size_t i = spotLightsCount; i < framebuffers.size(); ++i)
				shadowMaps[i] = RenderManager::GetDummyDepthImage();
			framebuffers.resize(spotLightsCount);
		}
	}

	void ShadowPassTask::InitMeshPipelines()
	{
		Ref<Sampler> shadowMapSampler = Sampler::Create(FilterMode::Point, AddressMode::Clamp, CompareOperation::Never, 0.f, 0.f, 1.f);

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
			depthAttachment.DepthClearValue = 1.f;
			depthAttachment.DepthCompareOp = CompareOperation::Less;

			PipelineGraphicsState state;
			state.VertexShader = Shader::Create("assets/shaders/shadow_map_meshes.vert", ShaderType::Vertex);
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::Front;
			state.PerInstanceAttribs = RenderMeshesTask::PerInstanceAttribs;

			m_OpacityMDLPipeline = PipelineGraphics::Create(state);

			state.VertexShader = Shader::Create("assets/shaders/shadow_map_meshes.vert", ShaderType::Vertex, { {"EG_MASKED", ""} });
			state.FragmentShader = Shader::Create("assets/shaders/shadow_map_masked.frag", ShaderType::Fragment);
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

			ShaderDefines defines;
			defines["EG_POINT_LIGHT_PASS"] = "";

			PipelineGraphicsState state;
			state.VertexShader = Shader::Create("assets/shaders/shadow_map_meshes.vert", ShaderType::Vertex, defines);
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::Front;
			state.bEnableMultiViewRendering = true;
			state.MultiViewPasses = 6;
			state.PerInstanceAttribs = RenderMeshesTask::PerInstanceAttribs;

			m_OpacityMPLPipeline = PipelineGraphics::Create(state);

			defines["EG_MASKED"] = "";
			state.VertexShader = Shader::Create("assets/shaders/shadow_map_meshes.vert", ShaderType::Vertex, defines);
			state.FragmentShader = Shader::Create("assets/shaders/shadow_map_masked.frag", ShaderType::Fragment);
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

			ShaderDefines defines;
			defines["EG_SPOT_LIGHT_PASS"] = "";

			PipelineGraphicsState state;
			state.VertexShader = Shader::Create("assets/shaders/shadow_map_meshes.vert", ShaderType::Vertex, defines);
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::Back;
			state.PerInstanceAttribs = RenderMeshesTask::PerInstanceAttribs;

			m_OpacityMSLPipeline = PipelineGraphics::Create(state);

			defines["EG_MASKED"] = "";
			state.VertexShader = Shader::Create("assets/shaders/shadow_map_meshes.vert", ShaderType::Vertex, defines);
			state.FragmentShader = Shader::Create("assets/shaders/shadow_map_masked.frag", ShaderType::Fragment);
			m_MaskedMSLPipelineClearing = PipelineGraphics::Create(state);

			state.DepthStencilAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
			state.DepthStencilAttachment.ClearOperation = ClearOperation::Load;
			m_MaskedMSLPipeline = PipelineGraphics::Create(state);
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

			PipelineGraphicsState state;
			state.VertexShader = Shader::Create("assets/shaders/shadow_map_sprites.vert", ShaderType::Vertex);
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::Front;
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

			ShaderDefines plDefines;
			plDefines["EG_POINT_LIGHT_PASS"] = "";

			PipelineGraphicsState state;
			state.VertexShader = Shader::Create("assets/shaders/shadow_map_sprites.vert", ShaderType::Vertex, plDefines);
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::Front;
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

			ShaderDefines slDefines;
			slDefines["EG_SPOT_LIGHT_PASS"] = "";

			PipelineGraphicsState state;
			state.VertexShader = Shader::Create("assets/shaders/shadow_map_sprites.vert", ShaderType::Vertex, slDefines);
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::Back;
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

			PipelineGraphicsState state;
			state.VertexShader = Shader::Create("assets/shaders/shadow_map_sprites.vert", ShaderType::Vertex, { {"EG_MASKED", ""} });
			state.FragmentShader = Shader::Create("assets/shaders/shadow_map_masked.frag", ShaderType::Fragment);
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::Front;
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

			ShaderDefines plDefines;
			plDefines["EG_POINT_LIGHT_PASS"] = "";
			plDefines["EG_MASKED"] = "";

			PipelineGraphicsState state;
			state.VertexShader = Shader::Create("assets/shaders/shadow_map_sprites.vert", ShaderType::Vertex, plDefines);
			state.FragmentShader = Shader::Create("assets/shaders/shadow_map_masked.frag", ShaderType::Fragment);
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::Front;
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

			ShaderDefines slDefines;
			slDefines["EG_SPOT_LIGHT_PASS"] = "";
			slDefines["EG_MASKED"] = "";

			PipelineGraphicsState state;
			state.VertexShader = Shader::Create("assets/shaders/shadow_map_sprites.vert", ShaderType::Vertex, slDefines);
			state.FragmentShader = Shader::Create("assets/shaders/shadow_map_masked.frag", ShaderType::Fragment);
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::Back;
			state.FrontFace = FrontFaceMode::Clockwise;

			m_MaskedSSLPipeline = PipelineGraphics::Create(state);

			state.DepthStencilAttachment.InitialLayout = ImageLayoutType::Unknown;
			state.DepthStencilAttachment.ClearOperation = ClearOperation::Clear;
			m_MaskedSSLPipelineClearing = PipelineGraphics::Create(state);
		}
	}

	void ShadowPassTask::InitOpaqueLitTextsPipelines()
	{
		// Directional light
		{
			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.Image = RenderManager::GetDummyDepthImage();
			depthAttachment.ClearOperation = ClearOperation::Load;

			PipelineGraphicsState state;
			state.VertexShader = Shader::Create("assets/shaders/shadow_map_texts_lit.vert", ShaderType::Vertex);
			state.FragmentShader = m_TextFragShader;
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::Front;
			state.FrontFace = FrontFaceMode::Clockwise;

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

			ShaderDefines plDefines;
			plDefines["EG_POINT_LIGHT_PASS"] = "";

			PipelineGraphicsState state;
			state.VertexShader = Shader::Create("assets/shaders/shadow_map_texts_lit.vert", ShaderType::Vertex, plDefines);
			state.FragmentShader = m_TextFragShader;
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::Front;
			state.FrontFace = FrontFaceMode::Clockwise;
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

			ShaderDefines slDefines;
			slDefines["EG_SPOT_LIGHT_PASS"] = "";

			PipelineGraphicsState state;
			state.VertexShader = Shader::Create("assets/shaders/shadow_map_texts_lit.vert", ShaderType::Vertex, slDefines);
			state.FragmentShader = m_TextFragShader;
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::Back;
			state.FrontFace = FrontFaceMode::Clockwise;

			m_OpaqueLitTSLPipeline = PipelineGraphics::Create(state);

			state.DepthStencilAttachment.InitialLayout = ImageLayoutType::Unknown;
			state.DepthStencilAttachment.ClearOperation = ClearOperation::Clear;
			m_OpaqueLitTSLPipelineClearing = PipelineGraphics::Create(state);
		}
	}
	
	void ShadowPassTask::InitMaskedLitTextsPipelines()
	{
		// Directional light
		{
			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.Image = RenderManager::GetDummyDepthImage();
			depthAttachment.ClearOperation = ClearOperation::Load;

			PipelineGraphicsState state;
			state.VertexShader = Shader::Create("assets/shaders/shadow_map_texts_lit.vert", ShaderType::Vertex, { {"EG_MASKED", ""} });
			state.FragmentShader = m_MaskedTextFragShader;
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::Front;
			state.FrontFace = FrontFaceMode::Clockwise;

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

			ShaderDefines plDefines;
			plDefines["EG_POINT_LIGHT_PASS"] = "";
			plDefines["EG_MASKED"] = "";

			PipelineGraphicsState state;
			state.VertexShader = Shader::Create("assets/shaders/shadow_map_texts_lit.vert", ShaderType::Vertex, plDefines);
			state.FragmentShader = m_MaskedTextFragShader;
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::Front;
			state.FrontFace = FrontFaceMode::Clockwise;
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

			ShaderDefines slDefines;
			slDefines["EG_SPOT_LIGHT_PASS"] = "";
			slDefines["EG_MASKED"] = "";

			PipelineGraphicsState state;
			state.VertexShader = Shader::Create("assets/shaders/shadow_map_texts_lit.vert", ShaderType::Vertex, slDefines);
			state.FragmentShader = m_MaskedTextFragShader;
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::Back;
			state.FrontFace = FrontFaceMode::Clockwise;

			m_MaskedLitTSLPipeline = PipelineGraphics::Create(state);

			state.DepthStencilAttachment.InitialLayout = ImageLayoutType::Unknown;
			state.DepthStencilAttachment.ClearOperation = ClearOperation::Clear;
			m_MaskedLitTSLPipelineClearing = PipelineGraphics::Create(state);
		}
	}

	void ShadowPassTask::InitUnlitTextsPipelines()
	{
		// Directional light
		{
			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.Image = RenderManager::GetDummyDepthImage();
			depthAttachment.ClearOperation = ClearOperation::Load;

			PipelineGraphicsState state;
			state.VertexShader = Shader::Create("assets/shaders/shadow_map_texts_unlit.vert", ShaderType::Vertex);
			state.FragmentShader = m_TextFragShader;
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::None;
			state.FrontFace = FrontFaceMode::Clockwise;

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

			ShaderDefines plDefines;
			plDefines["EG_POINT_LIGHT_PASS"] = "";

			PipelineGraphicsState state;
			state.VertexShader = Shader::Create("assets/shaders/shadow_map_texts_unlit.vert", ShaderType::Vertex, plDefines);
			state.FragmentShader = m_TextFragShader;
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::None;
			state.FrontFace = FrontFaceMode::Clockwise;
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

			ShaderDefines slDefines;
			slDefines["EG_SPOT_LIGHT_PASS"] = "";

			PipelineGraphicsState state;
			state.VertexShader = Shader::Create("assets/shaders/shadow_map_texts_unlit.vert", ShaderType::Vertex, slDefines);
			state.FragmentShader = m_TextFragShader;
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::None;
			state.FrontFace = FrontFaceMode::Clockwise;

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
			m_DLShadowMaps[i] = CreateDepthImage(size, std::string("CSMShadowMap") + std::to_string(i), ImageLayoutType::Unknown, false);
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
}
