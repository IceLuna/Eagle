#include "egpch.h"
#include "RenderSpritesTask.h"

#include "Eagle/Renderer/RenderManager.h"
#include "Eagle/Renderer/SceneRenderer.h"
#include "Eagle/Renderer/TextureSystem.h"
#include "Eagle/Renderer/MaterialSystem.h"

#include "Eagle/Renderer/VidWrappers/RenderCommandManager.h"

#include "Eagle/Debug/CPUTimings.h"
#include "Eagle/Debug/GPUTimings.h"

namespace Eagle
{
	struct PushData
	{
		glm::mat4 ViewProj;
		glm::mat4 PrevViewProj;
	};

	RenderSpritesTask::RenderSpritesTask(SceneRenderer& renderer)
		: RendererTask(renderer)
	{
		bMotionRequired = m_Renderer.GetOptions_RT().InternalState.bMotionBuffer;
		bJitter = m_Renderer.GetOptions_RT().InternalState.bJitter;
		InitPipeline();
	}

	void RenderSpritesTask::Draw(const Ref<CommandBuffer>& cmd, const Ref<PipelineGraphics>& pipeline, const QuadsRenderData<SpriteGeometryData>::BlendModeGeomType& spritesData, const void* vertexPushData, RenderStats& stats)
	{
		if (spritesData.IsEmpty())
			return;

		cmd->BeginGraphics(pipeline);
		cmd->SetGraphicsRootConstants(vertexPushData, nullptr);

		uint32_t quadsCount = (uint32_t)(spritesData.ShadowCastingQuads.QuadVertices.size() / 4);
		if (quadsCount > 0)
		{
			cmd->DrawIndexed(spritesData.ShadowCastingQuads.VertexBuffer, spritesData.ShadowCastingQuads.IndexBuffer, quadsCount * 6, 0, 0);
			++stats.DrawCalls;
		}
		quadsCount = (uint32_t)(spritesData.NonShadowQuads.QuadVertices.size() / 4);
		if (quadsCount > 0)
		{
			cmd->DrawIndexed(spritesData.NonShadowQuads.VertexBuffer, spritesData.NonShadowQuads.IndexBuffer, quadsCount * 6, 0, 0);
			++stats.DrawCalls;
		}

		cmd->EndGraphics();
	}

	void RenderSpritesTask::Draw(const Ref<CommandBuffer>& cmd, const Ref<PipelineGraphics>& pipeline, const SpriteGeometryData& spritesData, const void* vertexPushData, RenderStats& stats, const Ref<Framebuffer>& fb)
	{
		const uint32_t quadsCount = (uint32_t)(spritesData.QuadVertices.size() / 4);
		if (quadsCount == 0)
			return;

		if (fb)
			cmd->BeginGraphics(pipeline, fb);
		else
			cmd->BeginGraphics(pipeline);
		cmd->SetGraphicsRootConstants(vertexPushData, nullptr);
		cmd->DrawIndexed(spritesData.VertexBuffer, spritesData.IndexBuffer, quadsCount * 6, 0, 0);
		cmd->EndGraphics();
		++stats.DrawCalls;
	}

	void RenderSpritesTask::RecordCommandBuffer(const Ref<CommandBuffer>& cmd)
	{
		RenderOpaque(cmd);
		RenderMasked(cmd);
	}

	void RenderSpritesTask::RenderOpaque(const Ref<CommandBuffer>& cmd)
	{
		const auto& singleSided = m_Renderer.GetSingleSidedSpritesRenderData();
		const auto& doubleSided = m_Renderer.GetDoubleSidedSpritesRenderData();

		if (singleSided.Opaque.IsEmpty() && doubleSided.Opaque.IsEmpty())
			return;

		EG_CPU_TIMING_SCOPED("Render Opaque Sprites");
		EG_GPU_TIMING_SCOPED(cmd, "Render Opaque Sprites");

		const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
		const bool bTexturesDirty = texturesChangedFrame >= m_OpaqueTexturesUpdatedFrames[RenderManager::GetCurrentFrameIndex()];
		if (bTexturesDirty)
		{
			m_OpaquePipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
			m_OpaqueTexturesUpdatedFrames[RenderManager::GetCurrentFrameIndex()] = texturesChangedFrame + 1;
		}
		m_OpaquePipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
		m_OpaquePipeline->SetBuffer(MaterialSystem::GetMaterialsRawBuffer(), EG_PERSISTENT_SET, EG_BINDING_RAW_MATERIALS);
		m_OpaquePipeline->SetBuffer(m_Renderer.GetSpritesTransformsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MAX);

		PushData pushData;
		pushData.ViewProj = m_Renderer.GetViewProjection();
		if (bMotionRequired)
		{
			pushData.PrevViewProj = m_Renderer.GetPrevViewProjection();
			m_OpaquePipeline->SetBuffer(m_Renderer.GetSpritesPrevTransformBuffer(), EG_PERSISTENT_SET, EG_BINDING_MAX + 1);
		}
		if (bJitter)
			m_OpaquePipeline->SetBuffer(m_Renderer.GetJitter(), 1, 0);

		cmd->SetGraphicsCullMode(CullMode::Back);
		Draw(cmd, m_OpaquePipeline, singleSided.Opaque, &pushData, m_Renderer.GetStats());

		cmd->SetGraphicsCullMode(CullMode::None);
		Draw(cmd, m_OpaquePipeline, doubleSided.Opaque, &pushData, m_Renderer.GetStats());
	}

	void RenderSpritesTask::RenderMasked(const Ref<CommandBuffer>& cmd)
	{
		const auto& singleSided = m_Renderer.GetSingleSidedSpritesRenderData();
		const auto& doubleSided = m_Renderer.GetDoubleSidedSpritesRenderData();

		if (singleSided.Masked.IsEmpty() && doubleSided.Masked.IsEmpty())
			return;

		EG_CPU_TIMING_SCOPED("Render Masked Sprites");
		EG_GPU_TIMING_SCOPED(cmd, "Render Masked Sprites");

		const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
		const bool bTexturesDirty = texturesChangedFrame >= m_MaskedTexturesUpdatedFrames[RenderManager::GetCurrentFrameIndex()];
		if (bTexturesDirty)
		{
			m_MaskedPipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
			m_MaskedTexturesUpdatedFrames[RenderManager::GetCurrentFrameIndex()] = texturesChangedFrame + 1;
		}
		m_MaskedPipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
		m_MaskedPipeline->SetBuffer(MaterialSystem::GetMaterialsRawBuffer(), EG_PERSISTENT_SET, EG_BINDING_RAW_MATERIALS);
		m_MaskedPipeline->SetBuffer(m_Renderer.GetSpritesTransformsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MAX);

		PushData pushData;
		pushData.ViewProj = m_Renderer.GetViewProjection();
		if (bMotionRequired)
		{
			pushData.PrevViewProj = m_Renderer.GetPrevViewProjection();
			m_MaskedPipeline->SetBuffer(m_Renderer.GetSpritesPrevTransformBuffer(), EG_PERSISTENT_SET, EG_BINDING_MAX + 1);
		}
		if (bJitter)
			m_MaskedPipeline->SetBuffer(m_Renderer.GetJitter(), 1, 0);

		cmd->SetGraphicsCullMode(CullMode::Back);
		Draw(cmd, m_MaskedPipeline, singleSided.Masked, &pushData, m_Renderer.GetStats());

		cmd->SetGraphicsCullMode(CullMode::None);
		Draw(cmd, m_MaskedPipeline, doubleSided.Masked, &pushData, m_Renderer.GetStats());
	}

	void RenderSpritesTask::InitPipeline()
	{
		const auto& gbuffer = m_Renderer.GetGBuffer();

		ColorAttachment colorAttachment;
		colorAttachment.ClearOperation = ClearOperation::Load;
		colorAttachment.InitialLayout = ImageLayoutType::RenderTarget;
		colorAttachment.FinalLayout = ImageLayoutType::RenderTarget;
		colorAttachment.Image = gbuffer.Albedo;

		ColorAttachment geometry_shading_NormalsAttachment;
		geometry_shading_NormalsAttachment.ClearOperation = ClearOperation::Load;
		geometry_shading_NormalsAttachment.InitialLayout = ImageLayoutType::RenderTarget;
		geometry_shading_NormalsAttachment.FinalLayout = ImageLayoutType::RenderTarget;
		geometry_shading_NormalsAttachment.Image = gbuffer.Normals;

		ColorAttachment emissiveAttachment;
		emissiveAttachment.ClearOperation = ClearOperation::Load;
		emissiveAttachment.InitialLayout = ImageLayoutType::RenderTarget;
		emissiveAttachment.FinalLayout = ImageLayoutType::RenderTarget;
		emissiveAttachment.Image = gbuffer.Emissive;

		ColorAttachment materialAttachment;
		materialAttachment.ClearOperation = ClearOperation::Load;
		materialAttachment.InitialLayout = ImageLayoutType::RenderTarget;
		materialAttachment.FinalLayout = ImageLayoutType::RenderTarget;
		materialAttachment.Image = gbuffer.MaterialData;

		ColorAttachment flagsAttachment;
		flagsAttachment.Image = gbuffer.Flags;
		flagsAttachment.InitialLayout = ImageLayoutType::RenderTarget;
		flagsAttachment.FinalLayout = ImageLayoutType::RenderTarget;
		flagsAttachment.ClearOperation = ClearOperation::Load;

		ColorAttachment objectIDAttachment;
		objectIDAttachment.ClearOperation = ClearOperation::Load;
		objectIDAttachment.InitialLayout = ImageLayoutType::RenderTarget;
		objectIDAttachment.FinalLayout = ImageLayoutType::RenderTarget;
		objectIDAttachment.Image = gbuffer.ObjectID;

		DepthStencilAttachment depthAttachment;
		depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.Image = gbuffer.Depth;
		depthAttachment.ClearOperation = ClearOperation::Load;
		depthAttachment.bWriteDepth = true;
		depthAttachment.DepthClearValue = 0.f;
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
		state.VertexShader = Shader::Create("sprite.vert", ShaderType::Vertex, vertexDefines);
		state.FragmentShader = Shader::Create("sprite.frag", ShaderType::Fragment, fragmentDefines);
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
			velocityAttachment.InitialLayout = ImageLayoutType::RenderTarget;
			velocityAttachment.FinalLayout = ImageLayoutType::RenderTarget;
			velocityAttachment.ClearOperation = ClearOperation::Load;
			state.ColorAttachments.push_back(velocityAttachment);
		}
		state.DepthStencilAttachment = depthAttachment;
		state.CullMode = CullMode::Dynamic;

		if (m_OpaquePipeline)
			m_OpaquePipeline->SetState(state);
		else
			m_OpaquePipeline = PipelineGraphics::Create(state);

		fragmentDefines["EG_MASKED"] = "";
		state.FragmentShader = Shader::Create("sprite.frag", ShaderType::Fragment, fragmentDefines);
		if (m_MaskedPipeline)
			m_MaskedPipeline->SetState(state);
		else
			m_MaskedPipeline = PipelineGraphics::Create(state);
	}
}
