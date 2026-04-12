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
		RenderOpaque(cmd);
		RenderMasked(cmd);
	}

	void RenderMeshesTask::InitPipeline()
	{
		const auto& gbuffer = m_Renderer.GetGBuffer();

		ColorAttachment colorAttachment;
		colorAttachment.Image = gbuffer.Albedo;
		colorAttachment.InitialLayout = ImageLayoutType::RenderTarget;
		colorAttachment.FinalLayout = ImageLayoutType::RenderTarget;
		colorAttachment.ClearOperation = ClearOperation::Load;

		ColorAttachment normalsAttachment;
		normalsAttachment.Image = gbuffer.Normals;
		normalsAttachment.InitialLayout = ImageLayoutType::RenderTarget;
		normalsAttachment.FinalLayout = ImageLayoutType::RenderTarget;
		normalsAttachment.ClearOperation = ClearOperation::Load;

		ColorAttachment emissiveAttachment;
		emissiveAttachment.Image = gbuffer.Emissive;
		emissiveAttachment.InitialLayout = ImageLayoutType::RenderTarget;
		emissiveAttachment.FinalLayout = ImageLayoutType::RenderTarget;
		emissiveAttachment.ClearOperation = ClearOperation::Load;

		ColorAttachment materialAttachment;
		materialAttachment.Image = gbuffer.MaterialData;
		materialAttachment.InitialLayout = ImageLayoutType::RenderTarget;
		materialAttachment.FinalLayout = ImageLayoutType::RenderTarget;
		materialAttachment.ClearOperation = ClearOperation::Load;

		ColorAttachment flagsAttachment;
		flagsAttachment.Image = gbuffer.Flags;
		flagsAttachment.InitialLayout = ImageLayoutType::RenderTarget;
		flagsAttachment.FinalLayout = ImageLayoutType::RenderTarget;
		flagsAttachment.ClearOperation = ClearOperation::Load;

		constexpr int objectIDClearColorUint = -1;
		const float objectIDClearColor = *(float*)(&objectIDClearColorUint);
		ColorAttachment objectIDAttachment;
		objectIDAttachment.Image = gbuffer.ObjectID;
		objectIDAttachment.InitialLayout = ImageLayoutType::RenderTarget;
		objectIDAttachment.FinalLayout = ImageLayoutType::RenderTarget;
		objectIDAttachment.ClearOperation = ClearOperation::Load;
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
		state.ColorAttachments.push_back(normalsAttachment);
		state.ColorAttachments.push_back(emissiveAttachment);
		state.ColorAttachments.push_back(materialAttachment);
		state.ColorAttachments.push_back(flagsAttachment);
		state.ColorAttachments.push_back(objectIDAttachment);
		if (bMotionRequired)
		{
			ColorAttachment velocityAttachment;
			velocityAttachment.Image = gbuffer.Motion;
			velocityAttachment.InitialLayout = ImageLayoutType::RenderTarget;
			velocityAttachment.FinalLayout = ImageLayoutType::RenderTarget;
			velocityAttachment.ClearOperation = ClearOperation::Load;
			state.ColorAttachments.push_back(velocityAttachment);
		}

		state.PerInstanceAttribs = PerInstanceAttribs;
		state.DepthStencilAttachment = depthAttachment;
		state.CullMode = CullMode::Dynamic;

		if (m_OpaquePipeline)
			m_OpaquePipeline->SetState(state);
		else
			m_OpaquePipeline = PipelineGraphics::Create(state);

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
		if (meshes.empty())
			return;

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
		const auto& drawData = m_Renderer.GetStaticMeshesDrawData();
		const auto& singleSidedMeshes = drawData.SingleSided.Opaque.DrawData;
		const auto& doubleSidedMeshes = drawData.DoubleSided.Opaque.DrawData;
		if (singleSidedMeshes.empty() && doubleSidedMeshes.empty())
			return;

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
		const auto& buffers = m_Renderer.GetStaticMeshesBuffers();

		if (!singleSidedMeshes.empty())
		{
			cmd->SetGraphicsCullMode(CullMode::Back);
			Draw(cmd, m_OpaquePipeline, singleSidedMeshes, buffers, stats, &pushData);
		}
		if (!doubleSidedMeshes.empty())
		{
			cmd->SetGraphicsCullMode(CullMode::None);
			Draw(cmd, m_OpaquePipeline, doubleSidedMeshes, buffers, stats, &pushData);
		}
	}

	void RenderMeshesTask::RenderMasked(const Ref<CommandBuffer>& cmd)
	{
		const auto& drawData = m_Renderer.GetStaticMeshesDrawData();
		const auto& singleSidedMeshes = drawData.SingleSided.Masked.DrawData;
		const auto& doubleSidedMeshes = drawData.DoubleSided.Masked.DrawData;
		if (singleSidedMeshes.empty() && doubleSidedMeshes.empty())
			return;

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
		const auto& buffers = m_Renderer.GetStaticMeshesBuffers();

		if (!singleSidedMeshes.empty())
		{
			cmd->SetGraphicsCullMode(CullMode::Back);
			Draw(cmd, m_MaskedPipeline, singleSidedMeshes, buffers, stats, &pushData);
		}
		if (!doubleSidedMeshes.empty())
		{
			cmd->SetGraphicsCullMode(CullMode::None);
			Draw(cmd, m_MaskedPipeline, doubleSidedMeshes, buffers, stats, &pushData);
		}
	}
}
