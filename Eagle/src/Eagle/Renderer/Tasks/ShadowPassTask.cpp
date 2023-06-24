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

#include "Eagle/Debug/CPUTimings.h"
#include "Eagle/Debug/GPUTimings.h"

#include "../../Eagle-Editor/assets/shaders/common_structures.h"

// Temp
#include "Eagle/Core/Application.h"

namespace Eagle
{
	// TODO: Expose to editor
	static constexpr uint32_t s_CSMSizes[EG_CASCADES_COUNT] =
	{
		RendererConfig::DirLightShadowMapSize * 2,
		RendererConfig::DirLightShadowMapSize,
		RendererConfig::DirLightShadowMapSize,
		RendererConfig::DirLightShadowMapSize
	};

	static glm::uvec2 GetPointLightSMSize(float distanceToCamera, float maxShadowDistance)
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
		
		glm::uvec2 size = RendererConfig::PointLightSMSize / scaler;
		size = glm::max(minRes, size);

		return size;
	}

	static glm::uvec2 GetSpotLightSMSize(float distanceToCamera, float maxShadowDistance)
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
		
		glm::uvec2 size = RendererConfig::SpotLightSMSize / scaler;
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

		InitMeshPipelines();
		InitSpritesPipelines();

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

		ShadowPassMeshes(cmd);
		ShadowPassSprites(cmd);
	}

	void ShadowPassTask::ShadowPassMeshes(const Ref<CommandBuffer>& cmd)
	{
		auto& meshes = m_Renderer.GetMeshes();

		if (meshes.empty())
			return;

		EG_GPU_TIMING_SCOPED(cmd, "Meshes shadow pass");
		EG_CPU_TIMING_SCOPED("Meshes shadow pass");

		const auto& vb = m_Renderer.GetMeshVertexBuffer();
		const auto& ivb = m_Renderer.GetMeshInstanceVertexBuffer();
		const auto& ib = m_Renderer.GetMeshIndexBuffer();
		const auto& transformsBuffer = m_Renderer.GetMeshTransformsBuffer();
		const auto& dirLight = m_Renderer.GetDirectionalLight();
		const glm::vec3 cameraPos = m_Renderer.GetViewPosition();
		const float shadowMaxDistance = m_Renderer.GetShadowMaxDistance();

		// For directional light
		if (m_Renderer.HasDirectionalLight() && dirLight.bCastsShadows)
		{
			EG_GPU_TIMING_SCOPED(cmd, "Meshes: CSM Shadow pass");
			EG_CPU_TIMING_SCOPED("Meshes: CSM Shadow pass");

			CreateIfNeededDirectionalLightShadowMaps();

			m_MDLPipeline->SetBuffer(transformsBuffer, 0, 0);
			for (uint32_t i = 0; i < m_DLFramebuffers.size(); ++i)
			{
				const auto& viewProj = dirLight.ViewProj[i];

				cmd->BeginGraphics(m_MDLPipeline, m_DLFramebuffers[i]);
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
						cmd->DrawIndexedInstanced(vb, ib, indicesCount, firstIndex, vertexOffset, instanceCount, firstInstance, ivb);

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
				auto& pipeline = m_MPLPipeline;
				pipeline->SetBuffer(transformsBuffer, 0, 0);
				pipeline->SetBuffer(vpsBuffer, 0, 1);
				{
					EG_GPU_TIMING_SCOPED(cmd, "Meshes: Point Lights Shadow pass");
					EG_CPU_TIMING_SCOPED("Meshes: Point Lights Shadow pass");

					for (auto& pointLight : pointLights)
					{
						if (!pointLight.DoesCastShadows())
							continue;

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
								cmd->DrawIndexedInstanced(vb, ib, indicesCount, firstIndex, vertexOffset, instanceCount, firstInstance, ivb);

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
				auto& pipeline = m_MSLPipeline;
				pipeline->SetBuffer(transformsBuffer, 0, 0);
				{
					EG_GPU_TIMING_SCOPED(cmd, "Meshes: Spot Lights Shadow pass");
					EG_CPU_TIMING_SCOPED("Meshes: Spot Lights Shadow pass");

					for (auto& spotLight : spotLights)
					{
						if (spotLight.bCastsShadows == 0)
							continue;
						
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
								cmd->DrawIndexedInstanced(vb, ib, indicesCount, firstIndex, vertexOffset, instanceCount, firstInstance, ivb);

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
	
	void ShadowPassTask::ShadowPassSprites(const Ref<CommandBuffer>& cmd)
	{
		const auto& vertices = m_Renderer.GetSpritesVertices();

		const uint32_t quadsCount = (uint32_t)(vertices.size() / 4);
		if (quadsCount == 0)
			return;

		EG_GPU_TIMING_SCOPED(cmd, "Sprites shadow pass");
		EG_CPU_TIMING_SCOPED("Sprites shadow pass");

		const auto& vb = m_Renderer.GetSpritesVertexBuffer();
		const auto& ib = m_Renderer.GetSpritesIndexBuffer();
		const auto& transformsBuffer = m_Renderer.GetSpritesTransformsBuffer();

		const glm::vec3 cameraPos = m_Renderer.GetViewPosition();
		const float shadowMaxDistance = m_Renderer.GetShadowMaxDistance();

		const bool bHasMeshes = !m_Renderer.GetMeshes().empty();

		// For directional light
		const auto& dirLight = m_Renderer.GetDirectionalLight();
		if (m_Renderer.HasDirectionalLight() && dirLight.bCastsShadows)
		{
			EG_GPU_TIMING_SCOPED(cmd, "Sprites: CSM Shadow pass");
			EG_CPU_TIMING_SCOPED("Sprites: CSM Shadow pass");

			CreateIfNeededDirectionalLightShadowMaps();

			auto& pipeline = bHasMeshes ? m_SDLPipeline : m_SDLPipelineClearing;
			pipeline->SetBuffer(transformsBuffer, 0, 0);
			for (uint32_t i = 0; i < m_DLFramebuffers.size(); ++i)
			{
				cmd->BeginGraphics(pipeline, m_DLFramebuffers[i]);
				cmd->SetGraphicsRootConstants(&dirLight.ViewProj[i], nullptr);
				cmd->DrawIndexed(vb, ib, quadsCount * 6, 0, 0);
				cmd->EndGraphics();
			}
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
				EG_GPU_TIMING_SCOPED(cmd, "Sprites: Point Lights Shadow pass");
				EG_CPU_TIMING_SCOPED("Sprites: Point Lights Shadow pass");

				auto& vpsBuffer = m_PLVPsBuffer;
				auto& pipeline = bHasMeshes ? m_SPLPipeline : m_SPLPipelineClearing;
				pipeline->SetBuffer(transformsBuffer, 0, 0);
				pipeline->SetBuffer(vpsBuffer, 0, 1);

				for (auto& pointLight : pointLights)
				{
					if (!pointLight.DoesCastShadows())
						continue;

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
				}

				// Release unused framebuffers
				framebuffers.resize(pointLightsCount);
			}
			// Release unused shadow-maps & framebuffers
			for (size_t i = pointLightsCount; i < framebuffers.size(); ++i)
				shadowMaps[i] = RenderManager::GetDummyDepthImage();
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
				EG_GPU_TIMING_SCOPED(cmd, "Sprites: Spot Lights Shadow pass");
				EG_CPU_TIMING_SCOPED("Sprites: Spot Lights Shadow pass");

				auto& pipeline = bHasMeshes ? m_SSLPipeline : m_SSLPipelineClearing;
				pipeline->SetBuffer(transformsBuffer, 0, 0);

				for (auto& spotLight : spotLights)
				{
					if (spotLight.bCastsShadows == 0)
						continue;

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
				}
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

			m_MDLPipeline = PipelineGraphics::Create(state);
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

			m_MPLPipeline = PipelineGraphics::Create(state);
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

			m_MSLPipeline = PipelineGraphics::Create(state);
		}
	}

	void ShadowPassTask::InitSpritesPipelines()
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

			m_SDLPipeline = PipelineGraphics::Create(state);

			state.DepthStencilAttachment.ClearOperation = ClearOperation::Clear;
			m_SDLPipelineClearing = PipelineGraphics::Create(state);
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

			m_SPLPipeline = PipelineGraphics::Create(state);

			state.DepthStencilAttachment.ClearOperation = ClearOperation::Clear;
			m_SPLPipelineClearing = PipelineGraphics::Create(state);
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

			m_SSLPipeline = PipelineGraphics::Create(state);

			state.DepthStencilAttachment.ClearOperation = ClearOperation::Clear;
			m_SSLPipelineClearing = PipelineGraphics::Create(state);
		}
	}
	
	void ShadowPassTask::CreateIfNeededDirectionalLightShadowMaps()
	{
		if (m_DLShadowMaps[0] != RenderManager::GetDummyDepthImage())
			return;

		m_DLShadowMaps.resize(EG_CASCADES_COUNT);
		m_DLFramebuffers.resize(EG_CASCADES_COUNT);

		for (uint32_t i = 0; i < EG_CASCADES_COUNT; ++i)
		{
			const glm::uvec3 size = glm::uvec3(s_CSMSizes[i], s_CSMSizes[i], 1);
			m_DLShadowMaps[i] = CreateDepthImage(size, std::string("CSMShadowMap") + std::to_string(i), ImageLayoutType::Unknown, false);
		}

		const void* renderPassHandle = m_MDLPipeline->GetRenderPassHandle();
		for (uint32_t i = 0; i < EG_CASCADES_COUNT; ++i)
			m_DLFramebuffers[i] = Framebuffer::Create({ m_DLShadowMaps[i] }, glm::uvec2(s_CSMSizes[i]), renderPassHandle);
	}
	
	void ShadowPassTask::FreeDirectionalLightShadowMaps()
	{
		std::fill(m_DLShadowMaps.begin(), m_DLShadowMaps.end(), RenderManager::GetDummyDepthImage());
		m_DLFramebuffers.clear();
	}
}
