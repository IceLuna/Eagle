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
	static constexpr uint32_t s_CSMSizes[EG_CASCADES_COUNT] =
	{
		RendererConfig::DirLightShadowMapSize * 2,
		RendererConfig::DirLightShadowMapSize * 2,
		RendererConfig::DirLightShadowMapSize,
		RendererConfig::DirLightShadowMapSize
	};

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
		, m_DLFramebuffers(EG_CASCADES_COUNT)
	{
		std::fill(m_PLShadowMaps.begin(), m_PLShadowMaps.end(), RenderManager::GetDummyDepthCubeImage());
		std::fill(m_SLShadowMaps.begin(), m_SLShadowMaps.end(), RenderManager::GetDummyDepthImage());

		InitMeshPipelines();
		InitSpritesPipelines();

		BufferSpecifications pointLightsVPBufferSpecs;
		pointLightsVPBufferSpecs.Size = sizeof(glm::mat4) * 6;
		pointLightsVPBufferSpecs.Usage = BufferUsage::StorageBuffer | BufferUsage::TransferDst;
		m_PLVPsBuffer = Buffer::Create(pointLightsVPBufferSpecs, "PointLightsVPs");
	}

	void ShadowPassTask::RecordCommandBuffer(const Ref<CommandBuffer>& cmd)
	{
		ShadowPassMeshes(cmd);
		ShadowPassSprites(cmd);
	}

	void ShadowPassTask::ShadowPassMeshes(const Ref<CommandBuffer>& cmd)
	{
		auto& meshes = m_Renderer.GetMeshes();

		if (meshes.empty())
			return;

		struct VertexPushData
		{
			glm::mat4 ViewProj;
			uint32_t TransformIndex;
		} pushData;

		const auto& vb = m_Renderer.GetMeshVertexBuffer();
		const auto& ib = m_Renderer.GetMeshIndexBuffer();
		const auto& transformsBuffer = m_Renderer.GetMeshTransformsBuffer();

		// For directional light
		if (m_Renderer.HasDirectionalLight())
		{
			EG_GPU_TIMING_SCOPED(cmd, "Meshes: CSM Shadow pass");
			EG_CPU_TIMING_SCOPED("Meshes: CSM Shadow pass");

			const auto& dirLight = m_Renderer.GetDirectionalLight();

			m_MDLPipeline->SetBuffer(transformsBuffer, 0, 0);
			for (uint32_t i = 0; i < m_DLFramebuffers.size(); ++i)
			{
				uint32_t firstIndex = 0;
				uint32_t vertexOffset = 0;
				pushData.ViewProj = dirLight.ViewProj[i];

				cmd->BeginGraphics(m_MDLPipeline, m_DLFramebuffers[i]);
				const size_t meshesCount = meshes.size();
				for (size_t i = 0; i < meshesCount; ++i)
				{
					const auto& mesh = meshes[i];
					const auto& vertices = mesh.Mesh->GetVertices();
					const auto& indices = mesh.Mesh->GetIndeces();
					size_t vertexSize = vertices.size() * sizeof(Vertex);
					size_t indexSize = indices.size() * sizeof(Index);

					pushData.TransformIndex = uint32_t(i);
					cmd->SetGraphicsRootConstants(&pushData, nullptr);
					cmd->DrawIndexed(vb, ib, (uint32_t)indices.size(), firstIndex, vertexOffset);
					firstIndex += (uint32_t)indices.size();
					vertexOffset += (uint32_t)vertices.size();
				}
				cmd->EndGraphics();
			}
		}

		// For point lights
		const auto& pointLights = m_Renderer.GetPointLights();
		if (pointLights.size())
		{
			auto& vpsBuffer = m_PLVPsBuffer;
			auto& pipeline = m_MPLPipeline;
			auto& framebuffers = m_PLFramebuffers;
			pipeline->SetBuffer(transformsBuffer, 0, 0);
			pipeline->SetBuffer(vpsBuffer, 0, 1);
			{
				EG_GPU_TIMING_SCOPED(cmd, "Meshes: Point Lights Shadow pass");
				EG_CPU_TIMING_SCOPED("Meshes: Point Lights Shadow pass");

				uint32_t i = 0;
				for (auto& pointLight : pointLights)
				{
					if (i >= framebuffers.size())
					{
						// Create SM & framebuffer
						m_PLShadowMaps[i] = CreateDepthImage(RendererConfig::PointLightSMSize, "PointLight_SM" + std::to_string(i), ImageLayoutType::Unknown, true);
						framebuffers.push_back(Framebuffer::Create({ m_PLShadowMaps[i] }, glm::uvec2(RendererConfig::PointLightSMSize), pipeline->GetRenderPassHandle()));
					}

					cmd->StorageBufferBarrier(vpsBuffer);
					cmd->Write(vpsBuffer, &pointLight.ViewProj[0][0], vpsBuffer->GetSize(), 0, BufferLayoutType::Unknown, BufferLayoutType::StorageBuffer);
					cmd->StorageBufferBarrier(vpsBuffer);

					uint32_t firstIndex = 0;
					uint32_t vertexOffset = 0;

					cmd->BeginGraphics(pipeline, framebuffers[i]);
					const size_t meshesCount = meshes.size();
					for (size_t i = 0; i < meshesCount; ++i)
					{
						const auto& mesh = meshes[i];
						const auto& vertices = mesh.Mesh->GetVertices();
						const auto& indices = mesh.Mesh->GetIndeces();
						size_t vertexSize = vertices.size() * sizeof(Vertex);
						size_t indexSize = indices.size() * sizeof(Index);

						pushData.TransformIndex = uint32_t(i);
						cmd->SetGraphicsRootConstants(&pushData, nullptr);
						cmd->DrawIndexed(vb, ib, (uint32_t)indices.size(), firstIndex, vertexOffset);
						firstIndex += (uint32_t)indices.size();
						vertexOffset += (uint32_t)vertices.size();
					}
					cmd->EndGraphics();
					++i;
				}

				// Release unused shadow-maps & framebuffers
				for (size_t i = pointLights.size(); i < framebuffers.size(); ++i)
				{
					m_PLShadowMaps[i] = RenderManager::GetDummyDepthCubeImage();
				}
				framebuffers.resize(pointLights.size());
			}
		}

		// For spot lights
		const auto& spotLights = m_Renderer.GetSpotLights();
		if (spotLights.size())
		{
			auto& pipeline = m_MSLPipeline;
			auto& framebuffers = m_SLFramebuffers;
			pipeline->SetBuffer(transformsBuffer, 0, 0);
			{
				EG_GPU_TIMING_SCOPED(cmd, "Meshes: Spot Lights Shadow pass");
				EG_CPU_TIMING_SCOPED("Meshes: Spot Lights Shadow pass");

				uint32_t i = 0;
				for (auto& spotLight : spotLights)
				{
					if (i >= framebuffers.size())
					{
						// Create SM & framebuffer
						m_SLShadowMaps[i] = CreateDepthImage(RendererConfig::SpotLightSMSize, "SpotLight_SM" + std::to_string(i), ImageLayoutType::Unknown, false);
						framebuffers.push_back(Framebuffer::Create({ m_SLShadowMaps[i] }, glm::uvec2(RendererConfig::SpotLightSMSize), pipeline->GetRenderPassHandle()));
					}

					uint32_t firstIndex = 0;
					uint32_t vertexOffset = 0;
					pushData.ViewProj = spotLight.ViewProj;

					cmd->BeginGraphics(pipeline, framebuffers[i]);
					const size_t meshesCount = meshes.size();
					for (size_t i = 0; i < meshesCount; ++i)
					{
						const auto& mesh = meshes[i];
						const auto& vertices = mesh.Mesh->GetVertices();
						const auto& indices = mesh.Mesh->GetIndeces();
						size_t vertexSize = vertices.size() * sizeof(Vertex);
						size_t indexSize = indices.size() * sizeof(Index);

						pushData.TransformIndex = uint32_t(i);
						cmd->SetGraphicsRootConstants(&pushData, nullptr);
						cmd->DrawIndexed(vb, ib, (uint32_t)indices.size(), firstIndex, vertexOffset);
						firstIndex += (uint32_t)indices.size();
						vertexOffset += (uint32_t)vertices.size();
					}
					cmd->EndGraphics();
					++i;
				}

				// Release unused shadow-maps & framebuffers
				for (size_t i = spotLights.size(); i < framebuffers.size(); ++i)
				{
					m_SLShadowMaps[i] = RenderManager::GetDummyDepthImage();
				}
				framebuffers.resize(spotLights.size());
			}
		}
	}
	
	void ShadowPassTask::ShadowPassSprites(const Ref<CommandBuffer>& cmd)
	{
		const auto& vertices = m_Renderer.GetSpritesVertices();

		const uint32_t quadsCount = (uint32_t)(vertices.size() / 4);
		if (quadsCount == 0)
			return;

		const auto& vb = m_Renderer.GetSpritesVertexBuffer();
		const auto& ib = m_Renderer.GetSpritesIndexBuffer();

		// For directional light
		if (m_Renderer.HasDirectionalLight())
		{
			EG_GPU_TIMING_SCOPED(cmd, "Sprites: CSM Shadow pass");
			EG_CPU_TIMING_SCOPED("Sprites: CSM Shadow pass");

			const auto& dirLight = m_Renderer.GetDirectionalLight();
			for (uint32_t i = 0; i < m_DLFramebuffers.size(); ++i)
			{
				cmd->BeginGraphics(m_SDLPipeline, m_DLFramebuffers[i]);
				cmd->SetGraphicsRootConstants(&dirLight.ViewProj[i], nullptr);
				cmd->DrawIndexed(vb, ib, quadsCount * 6, 0, 0);
				cmd->EndGraphics();
			}
		}

		// Point lights
		auto& pointLights = m_Renderer.GetPointLights();
		if (pointLights.size())
		{
			EG_GPU_TIMING_SCOPED(cmd, "Sprites: Point Lights Shadow pass");
			EG_CPU_TIMING_SCOPED("Sprites: Point Lights Shadow pass");

			auto& shadowMaps = m_PLShadowMaps;
			auto& vpsBuffer = m_PLVPsBuffer;
			auto& framebuffers = m_PLFramebuffers;
			auto& pipeline = m_SPLPipeline;
			pipeline->SetBuffer(vpsBuffer, 0, 0);

			uint32_t i = 0;
			for (auto& pointLight : pointLights)
			{
				if (i >= framebuffers.size())
				{
					m_PLShadowMaps[i] = CreateDepthImage(RendererConfig::PointLightSMSize, "PointLight_SM" + std::to_string(i), ImageLayoutType::Unknown, true);
					framebuffers.push_back(Framebuffer::Create({ shadowMaps[i] }, RendererConfig::PointLightSMSize, pipeline->GetRenderPassHandle()));
				}

				cmd->StorageBufferBarrier(vpsBuffer);
				cmd->Write(vpsBuffer, &pointLight.ViewProj[0][0], vpsBuffer->GetSize(), 0, BufferLayoutType::Unknown, BufferLayoutType::StorageBuffer);
				cmd->StorageBufferBarrier(vpsBuffer);

				cmd->BeginGraphics(pipeline, framebuffers[i]);
				cmd->DrawIndexed(vb, ib, quadsCount * 6, 0, 0);
				cmd->EndGraphics();
				++i;
			}

			// Release unused framebuffers
			framebuffers.resize(pointLights.size());
		}

		// Spot lights
		auto& spotLights = m_Renderer.GetSpotLights();
		if (spotLights.size())
		{
			EG_GPU_TIMING_SCOPED(cmd, "Sprites: Spot Lights Shadow pass");
			EG_CPU_TIMING_SCOPED("Sprites: Spot Lights Shadow pass");

			auto& shadowMaps = m_SLShadowMaps;
			auto& framebuffers = m_SLFramebuffers;
			auto& pipeline = m_SSLPipeline;

			uint32_t i = 0;
			for (auto& spotLight : spotLights)
			{
				if (i >= framebuffers.size())
				{
					m_SLShadowMaps[i] = CreateDepthImage(RendererConfig::SpotLightSMSize, "SpotLight_SM" + std::to_string(i), ImageLayoutType::Unknown, false);
					framebuffers.push_back(Framebuffer::Create({ shadowMaps[i] }, RendererConfig::SpotLightSMSize, pipeline->GetRenderPassHandle()));
				}

				cmd->BeginGraphics(pipeline, framebuffers[i]);
				cmd->SetGraphicsRootConstants(&spotLight.ViewProj, nullptr);
				cmd->DrawIndexed(vb, ib, quadsCount * 6, 0, 0);
				cmd->EndGraphics();
				++i;
			}

			// Release unused framebuffers
			framebuffers.resize(spotLights.size());
		}
	}

	void ShadowPassTask::InitMeshPipelines()
	{
		Ref<Sampler> shadowMapSampler = Sampler::Create(FilterMode::Point, AddressMode::Clamp, CompareOperation::Never, 0.f, 0.f, 1.f);

		// For directional light
		{
			for (uint32_t i = 0; i < m_DLShadowMaps.size(); ++i)
			{
				const glm::uvec3 size = glm::uvec3(s_CSMSizes[i], s_CSMSizes[i], 1);
				m_DLShadowMaps[i] = CreateDepthImage(size, std::string("CSMShadowMap") + std::to_string(i), ImageReadAccess::PixelShaderRead, false);
				m_DLShadowMapSamplers[i] = shadowMapSampler;
			}

			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageLayoutType::Unknown;
			depthAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.Image = m_DLShadowMaps[0];
			depthAttachment.bWriteDepth = true;
			depthAttachment.ClearOperation = ClearOperation::Clear;
			depthAttachment.DepthClearValue = 1.f;
			depthAttachment.DepthCompareOp = CompareOperation::Less;

			PipelineGraphicsState state;
			state.VertexShader = Shader::Create("assets/shaders/shadow_map.vert", ShaderType::Vertex);
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::Back;

			m_MDLPipeline = PipelineGraphics::Create(state);

			const void* renderPassHandle = m_MDLPipeline->GetRenderPassHandle();
			for (uint32_t i = 0; i < m_DLFramebuffers.size(); ++i)
				m_DLFramebuffers[i] = Framebuffer::Create({ m_DLShadowMaps[i] }, glm::uvec2(s_CSMSizes[i]), renderPassHandle);
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
			state.VertexShader = Shader::Create("assets/shaders/shadow_map.vert", ShaderType::Vertex, defines);
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::Back;
			state.bEnableMultiViewRendering = true;
			state.MultiViewPasses = 6;

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
			state.VertexShader = Shader::Create("assets/shaders/shadow_map.vert", ShaderType::Vertex, defines);
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::Back;

			m_MSLPipeline = PipelineGraphics::Create(state);
		}
	}

	void ShadowPassTask::InitSpritesPipelines()
	{
		ShaderDefines defines;
		defines["EG_SPRITES"] = "";

		// Directional light
		{
			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.Image = m_DLFramebuffers[0]->GetImages()[0];
			depthAttachment.ClearOperation = ClearOperation::Load;

			PipelineGraphicsState state;
			state.VertexShader = Shader::Create("assets/shaders/shadow_map.vert", ShaderType::Vertex, defines);
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::Back;
			state.FrontFace = FrontFaceMode::Clockwise;

			m_SDLPipeline = PipelineGraphics::Create(state);
		}

		// Point light
		{
			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.Image = RenderManager::GetDummyDepthCubeImage();
			depthAttachment.ClearOperation = ClearOperation::Load;

			ShaderDefines plDefines = defines;
			plDefines["EG_POINT_LIGHT_PASS"] = "";

			PipelineGraphicsState state;
			state.VertexShader = Shader::Create("assets/shaders/shadow_map.vert", ShaderType::Vertex, plDefines);
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::Back;
			state.FrontFace = FrontFaceMode::Clockwise;
			state.bEnableMultiViewRendering = true;
			state.MultiViewPasses = 6;

			m_SPLPipeline = PipelineGraphics::Create(state);
		}

		// Spot light
		{
			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.Image = RenderManager::GetDummyDepthImage();
			depthAttachment.ClearOperation = ClearOperation::Load;

			ShaderDefines slDefines = defines;
			slDefines["EG_SPOT_LIGHT_PASS"] = "";

			PipelineGraphicsState state;
			state.VertexShader = Shader::Create("assets/shaders/shadow_map.vert", ShaderType::Vertex, slDefines);
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::Back;
			state.FrontFace = FrontFaceMode::Clockwise;

			m_SSLPipeline = PipelineGraphics::Create(state);
		}
	}
}
