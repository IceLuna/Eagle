#include "egpch.h"
#include "RenderMeshesTask.h"

#include "Eagle/Renderer/RenderManager.h"
#include "Eagle/Renderer/SceneRenderer.h"
#include "Eagle/Renderer/MaterialSystem.h"
#include "Eagle/Renderer/VidWrappers/RenderCommandManager.h"
#include "Eagle/Renderer/TextureSystem.h"

#include "Eagle/Debug/CPUTimings.h"
#include "Eagle/Debug/GPUTimings.h"

namespace Eagle
{
	struct PushData
	{
		glm::mat4 ViewProj;
		glm::mat4 PrevViewProj;
	};

	RenderMeshesTask::RenderMeshesTask(SceneRenderer& renderer)
		: RendererTask(renderer)
	{
		bMotionRequired = renderer.GetOptions_RT().InternalState.bMotionBuffer;
		bJitter = renderer.GetOptions_RT().InternalState.bJitter;
		InitPipeline();
	}

	void RenderMeshesTask::RecordCommandBuffer(const Ref<CommandBuffer>& cmd)
	{
		const auto& drawData = m_Renderer.GetStaticMeshesDrawData();
		if (drawData.Opaque.empty())
		{
			// Just to clear images & transition layouts
			cmd->BeginGraphics(m_OpaquePipeline);
			cmd->EndGraphics();
		}
		else
			RenderOpaque(cmd);
		
		if (!drawData.Masked.empty())
			RenderMasked(cmd);
	}

	void RenderMeshesTask::InitPipeline()
	{
		const auto& gbuffer = m_Renderer.GetGBuffer();

		ColorAttachment colorAttachment;
		colorAttachment.Image = gbuffer.Albedo;
		colorAttachment.InitialLayout = ImageLayoutType::Unknown;
		colorAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		colorAttachment.ClearOperation = ClearOperation::Clear;

		ColorAttachment geometry_shading_NormalsAttachment;
		geometry_shading_NormalsAttachment.Image = gbuffer.Geometry_Shading_Normals;
		geometry_shading_NormalsAttachment.InitialLayout = ImageLayoutType::Unknown;
		geometry_shading_NormalsAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		geometry_shading_NormalsAttachment.ClearOperation = ClearOperation::Clear;

		ColorAttachment emissiveAttachment;
		emissiveAttachment.Image = gbuffer.Emissive;
		emissiveAttachment.InitialLayout = ImageLayoutType::Unknown;
		emissiveAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		emissiveAttachment.ClearOperation = ClearOperation::Clear;

		ColorAttachment materialAttachment;
		materialAttachment.Image = gbuffer.MaterialData;
		materialAttachment.InitialLayout = ImageLayoutType::Unknown;
		materialAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		materialAttachment.ClearOperation = ClearOperation::Clear;

		ColorAttachment flagsAttachment;
		flagsAttachment.Image = gbuffer.Flags;
		flagsAttachment.InitialLayout = ImageLayoutType::Unknown;
		flagsAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		flagsAttachment.ClearOperation = ClearOperation::Clear;

		constexpr int objectIDClearColorUint = -1;
		const float objectIDClearColor = *(float*)(&objectIDClearColorUint);
		ColorAttachment objectIDAttachment;
		objectIDAttachment.Image = gbuffer.ObjectID;
		objectIDAttachment.InitialLayout = ImageLayoutType::Unknown;
		objectIDAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		objectIDAttachment.ClearOperation = ClearOperation::Clear;
		objectIDAttachment.ClearColor = glm::vec4{ objectIDClearColor };

		DepthStencilAttachment depthAttachment;
		depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.Image = gbuffer.Depth;
		depthAttachment.bWriteDepth = true;
		depthAttachment.ClearOperation = ClearOperation::Load;
		depthAttachment.DepthCompareOp = CompareOperation::GreaterEqual;

		ShaderDefines vertexDefines;
		ShaderDefines fragmentDefines;
		if (bMotionRequired)
		{
			vertexDefines["EG_MOTION"] = "";
			fragmentDefines["EG_MOTION"] = "";
		}
		if (bJitter)
			vertexDefines["EG_JITTER"] = "";

		PipelineGraphicsState state;
		state.VertexShader = Shader::Create("mesh.vert", ShaderType::Vertex, vertexDefines);
		state.FragmentShader = Shader::Create("mesh.frag", ShaderType::Fragment, fragmentDefines);

		state.ColorAttachments.push_back(colorAttachment);
		state.ColorAttachments.push_back(geometry_shading_NormalsAttachment);
		state.ColorAttachments.push_back(emissiveAttachment);
		state.ColorAttachments.push_back(materialAttachment);
		state.ColorAttachments.push_back(flagsAttachment);
		state.ColorAttachments.push_back(objectIDAttachment);
		if (bMotionRequired)
		{
			ColorAttachment velocityAttachment;
			velocityAttachment.Image = gbuffer.Motion;
			velocityAttachment.InitialLayout = ImageLayoutType::Unknown;
			velocityAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
			velocityAttachment.ClearOperation = ClearOperation::Clear;
			state.ColorAttachments.push_back(velocityAttachment);
		}

		state.PerInstanceAttribs = PerInstanceAttribs;
		state.DepthStencilAttachment = depthAttachment;
		state.CullMode = CullMode::Back;

		if (m_OpaquePipeline)
			m_OpaquePipeline->SetState(state);
		else
			m_OpaquePipeline = PipelineGraphics::Create(state);

		// Attachments of masked pipeline must be loaded
		for (auto& attachment : state.ColorAttachments)
		{
			attachment.ClearOperation = ClearOperation::Load;
			attachment.InitialLayout = ImageReadAccess::PixelShaderRead;
		}

		fragmentDefines["EG_MASKED"] = "";
		state.FragmentShader = Shader::Create("mesh.frag", ShaderType::Fragment, fragmentDefines);

		if (m_MaskedPipeline)
			m_MaskedPipeline->SetState(state);
		else
			m_MaskedPipeline = PipelineGraphics::Create(state);
	}
	
	void RenderMeshesTask::Draw(const Ref<CommandBuffer>& cmd, const Ref<PipelineGraphics>& pipeline, const std::vector<MeshDrawData>& meshes, const MeshGeometryData<Vertex>& buffers, RenderStats& stats,
		const void* vertexPushData, const Ref<Framebuffer>& framebuffer)
	{
		if (framebuffer)
			cmd->BeginGraphics(pipeline, framebuffer);
		else
			cmd->BeginGraphics(pipeline);
		cmd->SetGraphicsRootConstants(vertexPushData, nullptr);

		for (const auto& data : meshes)
		{
			const uint32_t verticesCount = data.VerticesCount;
			const uint32_t vertexOffset = data.VertexOffset;

			for (const auto& matRenderData : data.PerMaterialData)
			{
				const uint32_t indicesCount = matRenderData.IndexCount;
				const uint32_t firstIndex = matRenderData.FirstIndex;
				const uint32_t instanceCount = matRenderData.InstanceCount;
				const uint32_t firstInstance = matRenderData.FirstInstance;
				if (instanceCount > 0)
				{
					cmd->DrawIndexedInstanced(buffers.VertexBuffer, buffers.IndexBuffer, indicesCount, firstIndex, vertexOffset, instanceCount, firstInstance, buffers.InstanceBuffer);
					++stats.DrawCalls;
				}
			}
		}

		cmd->EndGraphics();
	}

	void RenderMeshesTask::RenderOpaque(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "Render Opaque Static Meshes");
		EG_CPU_TIMING_SCOPED("Render Opaque Static Meshes");

		const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
		const bool bTexturesDirty = texturesChangedFrame >= m_OpaqueTexturesUpdatedFrames[RenderManager::GetCurrentFrameIndex()];
		if (bTexturesDirty)
		{
			m_OpaquePipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
			m_OpaqueTexturesUpdatedFrames[RenderManager::GetCurrentFrameIndex()] = texturesChangedFrame + 1;
		}

		m_OpaquePipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
		m_OpaquePipeline->SetBuffer(MaterialSystem::GetMaterialsRawBuffer(), EG_PERSISTENT_SET, EG_BINDING_RAW_MATERIALS);
		m_OpaquePipeline->SetBuffer(m_Renderer.GetMeshTransformsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MAX);

		PushData pushData;
		pushData.ViewProj = m_Renderer.GetViewProjection();
		if (bMotionRequired)
		{
			pushData.PrevViewProj = m_Renderer.GetPrevViewProjection();
			m_OpaquePipeline->SetBuffer(m_Renderer.GetMeshPrevTransformsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MAX + 1);
		}
		if (bJitter)
			m_OpaquePipeline->SetBuffer(m_Renderer.GetJitter(), 1, 0);

		auto& stats = m_Renderer.GetStats();
		const auto& meshes = m_Renderer.GetStaticMeshesDrawData().Opaque;
		const auto& buffers = m_Renderer.GetStaticMeshesBuffers();
		Draw(cmd, m_OpaquePipeline, meshes, buffers, stats, &pushData);
	}

	void RenderMeshesTask::RenderMasked(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "Render Masked Static Meshes");
		EG_CPU_TIMING_SCOPED("Render Masked Static Meshes");

		const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
		const bool bTexturesDirty = texturesChangedFrame >= m_MaskedTexturesUpdatedFrames[RenderManager::GetCurrentFrameIndex()];
		if (bTexturesDirty)
		{
			m_MaskedPipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
			m_MaskedTexturesUpdatedFrames[RenderManager::GetCurrentFrameIndex()] = texturesChangedFrame + 1;
		}

		m_MaskedPipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
		m_MaskedPipeline->SetBuffer(MaterialSystem::GetMaterialsRawBuffer(), EG_PERSISTENT_SET, EG_BINDING_RAW_MATERIALS);
		m_MaskedPipeline->SetBuffer(m_Renderer.GetMeshTransformsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MAX);

		PushData pushData;
		pushData.ViewProj = m_Renderer.GetViewProjection();

		if (bMotionRequired)
		{
			pushData.PrevViewProj = m_Renderer.GetPrevViewProjection();
			m_MaskedPipeline->SetBuffer(m_Renderer.GetMeshPrevTransformsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MAX + 1);
		}
		if (bJitter)
			m_MaskedPipeline->SetBuffer(m_Renderer.GetJitter(), 1, 0);

		auto& stats = m_Renderer.GetStats();
		const auto& meshes = m_Renderer.GetStaticMeshesDrawData().Masked;
		const auto& buffers = m_Renderer.GetStaticMeshesBuffers();
		Draw(cmd, m_MaskedPipeline, meshes, buffers, stats, &pushData);
	}
}
