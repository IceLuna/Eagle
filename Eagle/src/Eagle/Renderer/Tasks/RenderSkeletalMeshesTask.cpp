#include "egpch.h"
#include "RenderSkeletalMeshesTask.h"

#include "Eagle/Components/Components.h"
#include "Eagle/Renderer/RenderManager.h"
#include "Eagle/Renderer/SceneRenderer.h"
#include "Eagle/Renderer/MaterialSystem.h"
#include "Eagle/Renderer/VidWrappers/RenderCommandManager.h"
#include "Eagle/Renderer/TextureSystem.h"
#include "Eagle/Animation/AnimationSystem.h"

#include "Eagle/Debug/CPUTimings.h"
#include "Eagle/Debug/GPUTimings.h"

namespace Eagle
{
	// We're manually fetching VB & IVB data, so set the to null for the draw calls
	static Ref<Buffer> s_NullBuffer = nullptr;

	RenderSkeletalMeshesTask::RenderSkeletalMeshesTask(SceneRenderer& renderer)
		: RendererTask(renderer)
	{
		bMotionRequired = renderer.GetOptions_RT().InternalState.bMotionBuffer;
		bJitter = renderer.GetOptions_RT().InternalState.bJitter;
		InitPipeline();
	}

	void RenderSkeletalMeshesTask::RecordCommandBuffer(const Ref<CommandBuffer>& cmd)
	{
		RenderOpaque(cmd);
		RenderMasked(cmd);
	}

	void RenderSkeletalMeshesTask::InitPipeline()
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

		ColorAttachment objectIDAttachment;
		objectIDAttachment.Image = gbuffer.ObjectID;
		objectIDAttachment.InitialLayout = ImageLayoutType::RenderTarget;
		objectIDAttachment.FinalLayout = ImageLayoutType::RenderTarget;
		objectIDAttachment.ClearOperation = ClearOperation::Load;

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
		state.VertexShader = Shader::Create("mesh_skeletal.vert", ShaderType::Vertex, vertexDefines);
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

	void RenderSkeletalMeshesTask::DrawCulled(const Ref<CommandBuffer>& cmd, const Ref<PipelineGraphics>& pipeline, const SkeletalMeshGeometryData& buffers, const FrustumCulledMeshes& meshes,
		MaterialBlendMode blendMode, RenderStats& stats, const void* vertexPushData)
	{
		const auto& singleSided = meshes.SingleSided.BlendModes[uint32_t(blendMode)];
		const auto& doubleSided = meshes.DoubleSided.BlendModes[uint32_t(blendMode)];

		if (singleSided.GetNumMeshes() == 0 && doubleSided.GetNumMeshes() == 0)
			return;

		cmd->BeginGraphics(pipeline);
		if (vertexPushData)
			cmd->SetGraphicsRootConstants(vertexPushData, nullptr);

		if (singleSided.GetNumMeshes() > 0)
		{
			cmd->SetGraphicsCullMode(CullMode::Back);
			cmd->DrawIndexedInstancedIndirectCount(s_NullBuffer, buffers.IndexBuffer, singleSided.Result.IndirectArgsBuffer, singleSided.Result.DrawCountBuffer, s_NullBuffer, singleSided.Result.MaxDrawCalls);
			++stats.DrawCalls;
		}

		if (doubleSided.GetNumMeshes())
		{
			cmd->SetGraphicsCullMode(CullMode::None);
			cmd->DrawIndexedInstancedIndirectCount(s_NullBuffer, buffers.IndexBuffer, doubleSided.Result.IndirectArgsBuffer, doubleSided.Result.DrawCountBuffer, s_NullBuffer, doubleSided.Result.MaxDrawCalls);
			++stats.DrawCalls;
		}

		cmd->EndGraphics();
	}

	void RenderSkeletalMeshesTask::DrawUnculledShadowCasters(const Ref<CommandBuffer>& cmd, const Ref<PipelineGraphics>& pipeline, const SkeletalMeshGeometryData& buffers, const FrustumCulledMeshes& meshes,
		MaterialBlendMode blendMode, RenderStats& stats, const void* vertexPushData, const Ref<Framebuffer>& framebuffer, CullMode singleSidedCullMode)
	{
		const auto& singleSided = meshes.SingleSided.BlendModes[uint32_t(blendMode)];
		const auto& doubleSided = meshes.DoubleSided.BlendModes[uint32_t(blendMode)];

		if (framebuffer)
			cmd->BeginGraphics(pipeline, framebuffer);
		else
			cmd->BeginGraphics(pipeline);

		if (vertexPushData)
			cmd->SetGraphicsRootConstants(vertexPushData, nullptr);

		if (singleSided.GetNumMeshes() > 0)
		{
			cmd->SetGraphicsCullMode(singleSidedCullMode);
			cmd->DrawIndexedInstancedIndirectCount(s_NullBuffer, buffers.IndexBuffer, singleSided.Result.UnculledShadowCastersIndirectArgsBuffer, singleSided.Result.UnculledShadowCastersDrawCountBuffer, s_NullBuffer, singleSided.Result.MaxDrawCalls);
			++stats.DrawCalls;
		}

		if (doubleSided.GetNumMeshes())
		{
			cmd->SetGraphicsCullMode(CullMode::None);
			cmd->DrawIndexedInstancedIndirectCount(s_NullBuffer, buffers.IndexBuffer, doubleSided.Result.UnculledShadowCastersIndirectArgsBuffer, doubleSided.Result.UnculledShadowCastersDrawCountBuffer, s_NullBuffer, doubleSided.Result.MaxDrawCalls);
			++stats.DrawCalls;
		}

		cmd->EndGraphics();
	}

	void RenderSkeletalMeshesTask::RenderOpaque(const Ref<CommandBuffer>& cmd)
	{
		const auto& culledMeshes = m_Renderer.GetCulledSkeletalMeshes();
		const auto& ivb = culledMeshes.InstanceBuffer;
		const auto& singleSided = culledMeshes.SingleSided.BlendModes[uint32_t(MaterialBlendMode::Opaque)];
		const auto& doubleSided = culledMeshes.DoubleSided.BlendModes[uint32_t(MaterialBlendMode::Opaque)];
		if (singleSided.GetNumMeshes() == 0 && doubleSided.GetNumMeshes() == 0)
			return;

		EG_GPU_TIMING_SCOPED(cmd, "Render Opaque Skeletal Meshes");
		EG_CPU_TIMING_SCOPED("Render Opaque Skeletal Meshes");

		const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
		const bool bTexturesDirty = texturesChangedFrame >= m_OpaqueTexturesUpdatedFrames[RenderManager::GetCurrentFrameIndex()];
		if (bTexturesDirty)
		{
			m_OpaquePipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
			m_OpaqueTexturesUpdatedFrames[RenderManager::GetCurrentFrameIndex()] = texturesChangedFrame + 1;
		}

		m_OpaquePipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
		m_OpaquePipeline->SetBuffer(MaterialSystem::GetMaterialsRawBuffer(), EG_PERSISTENT_SET, EG_BINDING_RAW_MATERIALS);
		m_OpaquePipeline->SetBuffer(m_Renderer.GetSkinnedVertices(), EG_PERSISTENT_SET, EG_BINDING_MAX);
		m_OpaquePipeline->SetBuffer(ivb, EG_PERSISTENT_SET, EG_BINDING_MAX + 1);
		m_OpaquePipeline->SetBuffer(m_Renderer.GetCameraMatricesBuffer(), EG_PERSISTENT_SET, EG_BINDING_MAX + 2);
		m_OpaquePipeline->SetBuffer(m_Renderer.GetSkeletalMeshTransformsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MAX + 3);

		if (bMotionRequired)
		{
			m_OpaquePipeline->SetBuffer(m_Renderer.GetPrevSkinnedVerticesPositions(), EG_PERSISTENT_SET, EG_BINDING_MAX + 4);
		}
		if (bJitter)
			m_OpaquePipeline->SetBuffer(m_Renderer.GetJitter(), 1, 0);

		const auto& buffers = m_Renderer.GetSkeletalMeshesBuffers();
		auto& stats = m_Renderer.GetStats();

		DrawCulled(cmd, m_OpaquePipeline, buffers, culledMeshes, MaterialBlendMode::Opaque, stats);
	}

	void RenderSkeletalMeshesTask::RenderMasked(const Ref<CommandBuffer>& cmd)
	{
		const auto& culledMeshes = m_Renderer.GetCulledSkeletalMeshes();
		const auto& ivb = culledMeshes.InstanceBuffer;
		const auto& singleSided = culledMeshes.SingleSided.BlendModes[uint32_t(MaterialBlendMode::Masked)];
		const auto& doubleSided = culledMeshes.DoubleSided.BlendModes[uint32_t(MaterialBlendMode::Masked)];
		if (singleSided.GetNumMeshes() == 0 && doubleSided.GetNumMeshes() == 0)
			return;

		EG_GPU_TIMING_SCOPED(cmd, "Render Masked Skeletal Meshes");
		EG_CPU_TIMING_SCOPED("Render Masked Skeletal Meshes");

		const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
		const bool bTexturesDirty = texturesChangedFrame >= m_MaskedTexturesUpdatedFrames[RenderManager::GetCurrentFrameIndex()];
		if (bTexturesDirty)
		{
			m_MaskedPipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
			m_MaskedTexturesUpdatedFrames[RenderManager::GetCurrentFrameIndex()] = texturesChangedFrame + 1;
		}

		m_MaskedPipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
		m_MaskedPipeline->SetBuffer(MaterialSystem::GetMaterialsRawBuffer(), EG_PERSISTENT_SET, EG_BINDING_RAW_MATERIALS);
		m_MaskedPipeline->SetBuffer(m_Renderer.GetSkinnedVertices(), EG_PERSISTENT_SET, EG_BINDING_MAX);
		m_MaskedPipeline->SetBuffer(ivb, EG_PERSISTENT_SET, EG_BINDING_MAX + 1);
		m_MaskedPipeline->SetBuffer(m_Renderer.GetCameraMatricesBuffer(), EG_PERSISTENT_SET, EG_BINDING_MAX + 2);
		m_MaskedPipeline->SetBuffer(m_Renderer.GetSkeletalMeshTransformsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MAX + 3);

		if (bMotionRequired)
		{
			m_MaskedPipeline->SetBuffer(m_Renderer.GetPrevSkinnedVerticesPositions(), EG_PERSISTENT_SET, EG_BINDING_MAX + 4);
		}
		if (bJitter)
			m_MaskedPipeline->SetBuffer(m_Renderer.GetJitter(), 1, 0);

		const auto& buffers = m_Renderer.GetSkeletalMeshesBuffers();
		auto& stats = m_Renderer.GetStats();
		DrawCulled(cmd, m_MaskedPipeline, buffers, culledMeshes, MaterialBlendMode::Masked, stats);
	}
}
