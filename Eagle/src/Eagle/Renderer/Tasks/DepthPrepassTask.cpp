#include "egpch.h"
#include "DepthPrepassTask.h"

#include "RenderSpritesTask.h"
#include "RenderMeshesTask.h"
#include "RenderSkeletalMeshesTask.h"

#include "Eagle/Renderer/RenderManager.h"
#include "Eagle/Renderer/SceneRenderer.h"
#include "Eagle/Renderer/VidWrappers/RenderCommandManager.h"

#include "Eagle/Debug/CPUTimings.h"
#include "Eagle/Debug/GPUTimings.h"

namespace Eagle
{
	DepthPrepassTask::DepthPrepassTask(SceneRenderer& renderer)
		: RendererTask(renderer)
	{
		const auto& options = renderer.GetOptions_RT();
		bJitter = options.InternalState.bJitter;
		InitPipelines();
	}

	void DepthPrepassTask::RecordCommandBuffer(const Ref<CommandBuffer>& cmd)
	{
		EG_CPU_TIMING_SCOPED("Depth Prepass");
		EG_GPU_TIMING_SCOPED(cmd, "Depth Prepass");

		RenderSprites(cmd);
		RenderStaticMeshes(cmd);
		RenderSkeletalMeshes(cmd);
	}

	void DepthPrepassTask::RenderSprites(const Ref<CommandBuffer>& cmd)
	{
		const auto& singleSided = m_Renderer.GetSingleSidedSpritesRenderData();
		if (singleSided.Opaque.IsEmpty())
		{
			return;
		}

		EG_CPU_TIMING_SCOPED("Depth Prepass. Sprites");
		EG_GPU_TIMING_SCOPED(cmd, "Depth Prepass. Sprites");

		m_SpritesPipeline->SetBuffer(m_Renderer.GetSpritesTransformsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MAX);

		const auto& vp = m_Renderer.GetViewProjection();
		if (bJitter)
			m_SpritesPipeline->SetBuffer(m_Renderer.GetJitter(), 1, 0);

		RenderSpritesTask::Draw(cmd, m_SpritesPipeline, singleSided.Opaque, glm::value_ptr(vp), m_Renderer.GetStats());
	}

	void DepthPrepassTask::RenderStaticMeshes(const Ref<CommandBuffer>& cmd)
	{
		const auto& culledMeshes = m_Renderer.GetCulledStaticMeshes();
		const auto& ivb = culledMeshes.InstanceBuffer;
		const auto& singleSided = culledMeshes.SingleSided.Opaque;
		const auto& doubleSided = culledMeshes.DoubleSided.Opaque;
		if (singleSided.GetNumMeshes() == 0 && doubleSided.GetNumMeshes() == 0)
			return;

		EG_GPU_TIMING_SCOPED(cmd, "Depth Prepass. Static Meshes");
		EG_CPU_TIMING_SCOPED("Depth Prepass. Static Meshes");

		m_StaticMeshesPipeline->SetBuffer(m_Renderer.GetMeshTransformsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MAX);

		const glm::mat4& viewProj = m_Renderer.GetViewProjection();
		if (bJitter)
			m_StaticMeshesPipeline->SetBuffer(m_Renderer.GetJitter(), 1, 0);

		auto& stats = m_Renderer.GetStats();
		const auto& buffers = m_Renderer.GetStaticMeshesBuffers();

		cmd->BeginGraphics(m_StaticMeshesPipeline);
		cmd->SetGraphicsRootConstants(glm::value_ptr(viewProj), nullptr);

		if (singleSided.GetNumMeshes() > 0)
		{
			cmd->SetGraphicsCullMode(CullMode::Back);
			cmd->DrawIndexedInstancedIndirectCount(buffers.VertexBuffer, buffers.IndexBuffer, singleSided.Result.IndirectArgsBuffer, singleSided.Result.DrawCountBuffer, ivb, singleSided.Result.MaxDrawCalls);
			++stats.DrawCalls;
		}
		if (doubleSided.GetNumMeshes())
		{
			cmd->SetGraphicsCullMode(CullMode::None);
			cmd->DrawIndexedInstancedIndirectCount(buffers.VertexBuffer, buffers.IndexBuffer, doubleSided.Result.IndirectArgsBuffer, doubleSided.Result.DrawCountBuffer, ivb, doubleSided.Result.MaxDrawCalls);
			++stats.DrawCalls;
		}

		cmd->EndGraphics();
	}

	void DepthPrepassTask::RenderSkeletalMeshes(const Ref<CommandBuffer>& cmd)
	{
		// We're manually fetching VB & IVB data, so set the to null for the draw calls
		static Ref<Buffer> s_NullBuffer = nullptr;

		const auto& culledMeshes = m_Renderer.GetCulledSkeletalMeshes();
		const auto& ivb = culledMeshes.InstanceBuffer;
		const auto& singleSided = culledMeshes.SingleSided.Opaque;
		const auto& doubleSided = culledMeshes.DoubleSided.Opaque;
		if (singleSided.GetNumMeshes() == 0 && doubleSided.GetNumMeshes() == 0)
			return;

		EG_GPU_TIMING_SCOPED(cmd, "Depth Prepass. Skeletal Meshes");
		EG_CPU_TIMING_SCOPED("Depth Prepass. Skeletal Meshes");

		m_SkeletalMeshesPipeline->SetBuffer(m_Renderer.GetSkinnedVertices(), EG_PERSISTENT_SET, EG_BINDING_MAX);
		m_SkeletalMeshesPipeline->SetBuffer(ivb, EG_PERSISTENT_SET, EG_BINDING_MAX + 1);
		m_SkeletalMeshesPipeline->SetBuffer(m_Renderer.GetCameraMatricesBuffer(), EG_PERSISTENT_SET, EG_BINDING_MAX + 2);

		if (bJitter)
			m_SkeletalMeshesPipeline->SetBuffer(m_Renderer.GetJitter(), 1, 0);

		const auto& buffers = m_Renderer.GetSkeletalMeshesBuffers();
		auto& stats = m_Renderer.GetStats();

		cmd->BeginGraphics(m_SkeletalMeshesPipeline);
		if (singleSided.GetNumMeshes() > 0)
		{
			cmd->SetGraphicsCullMode(CullMode::Back);
			cmd->DrawIndexedInstancedIndirectCount(s_NullBuffer, buffers.IndexBuffer, singleSided.Result.IndirectArgsBuffer, singleSided.Result.DrawCountBuffer, s_NullBuffer, singleSided.Result.MaxDrawCalls);
			++stats.DrawCalls;
		}
		if (doubleSided.GetNumMeshes() > 0)
		{
			cmd->SetGraphicsCullMode(CullMode::None);
			cmd->DrawIndexedInstancedIndirectCount(s_NullBuffer, buffers.IndexBuffer, doubleSided.Result.IndirectArgsBuffer, doubleSided.Result.DrawCountBuffer, s_NullBuffer, doubleSided.Result.MaxDrawCalls);
			++stats.DrawCalls;
		}
		cmd->EndGraphics();
	}

	void DepthPrepassTask::InitSpritesPipeline()
	{
		const auto& gbuffer = m_Renderer.GetGBuffer();

		DepthStencilAttachment depthAttachment;
		depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.Image = gbuffer.Depth;
		depthAttachment.bWriteDepth = true;
		depthAttachment.ClearOperation = ClearOperation::Load;
		depthAttachment.DepthCompareOp = CompareOperation::Greater;

		ShaderDefines vertexDefines;
		vertexDefines["EG_DEPTH_ONLY"] = "";
		if (bJitter)
			vertexDefines["EG_JITTER"] = "";

		PipelineGraphicsState state;
		state.VertexShader = Shader::Create("sprite.vert", ShaderType::Vertex, vertexDefines);
		state.DepthStencilAttachment = depthAttachment;
		state.CullMode = CullMode::Back;

		if (m_SpritesPipeline)
			m_SpritesPipeline->SetState(state);
		else
			m_SpritesPipeline = PipelineGraphics::Create(state);
	}

	void DepthPrepassTask::InitStaticPipeline()
	{
		const auto& gbuffer = m_Renderer.GetGBuffer();

		DepthStencilAttachment depthAttachment;
		depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.Image = gbuffer.Depth;
		depthAttachment.bWriteDepth = true;
		depthAttachment.ClearOperation = ClearOperation::Load;
		depthAttachment.DepthCompareOp = CompareOperation::Greater;

		ShaderDefines vertexDefines;
		vertexDefines["EG_DEPTH_ONLY"] = "";
		if (bJitter)
			vertexDefines["EG_JITTER"] = "";

		PipelineGraphicsState state;
		state.VertexShader = Shader::Create("mesh.vert", ShaderType::Vertex, vertexDefines);

		state.PerInstanceAttribs = RenderMeshesTask::PerInstanceAttribs;
		state.DepthStencilAttachment = depthAttachment;
		state.CullMode = CullMode::Back;

		if (m_StaticMeshesPipeline)
			m_StaticMeshesPipeline->SetState(state);
		else
			m_StaticMeshesPipeline = PipelineGraphics::Create(state);
	}

	void DepthPrepassTask::InitSkeletalPipeline()
	{
		const auto& gbuffer = m_Renderer.GetGBuffer();

		DepthStencilAttachment depthAttachment;
		depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.Image = gbuffer.Depth;
		depthAttachment.bWriteDepth = true;
		depthAttachment.ClearOperation = ClearOperation::Load;
		depthAttachment.DepthCompareOp = CompareOperation::Greater;

		ShaderDefines vertexDefines;
		vertexDefines["EG_DEPTH_ONLY"] = "";
		if (bJitter)
			vertexDefines["EG_JITTER"] = "";

		PipelineGraphicsState state;
		state.VertexShader = Shader::Create("mesh_skeletal.vert", ShaderType::Vertex, vertexDefines);

		state.PerInstanceAttribs = RenderSkeletalMeshesTask::PerInstanceAttribs;
		state.DepthStencilAttachment = depthAttachment;
		state.CullMode = CullMode::Back;

		if (m_SkeletalMeshesPipeline)
			m_SkeletalMeshesPipeline->SetState(state);
		else
			m_SkeletalMeshesPipeline = PipelineGraphics::Create(state);
	}
}
