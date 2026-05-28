#include "egpch.h"
#include "RenderTextLitTask.h"

#include "Eagle/Renderer/RenderManager.h"
#include "Eagle/Renderer/TextureSystem.h"
#include "Eagle/Renderer/MaterialSystem.h"
#include "Eagle/Renderer/VidWrappers/Buffer.h"
#include "Eagle/Renderer/VidWrappers/RenderCommandManager.h"

#include "Eagle/Components/Components.h"

#include "Eagle/Debug/CPUTimings.h"
#include "Eagle/Debug/GPUTimings.h"

namespace Eagle
{
	struct PushData
	{
		glm::mat4 ViewProj;
		glm::mat4 PrevViewProj;
	};

	RenderTextLitTask::RenderTextLitTask(SceneRenderer& renderer)
		: RendererTask(renderer)
	{
		const auto& settings = m_Renderer.GetOptions();
		bMotionRequired = settings.InternalState.bMotionBuffer;
		bJitter = settings.InternalState.bJitter;
		bGeometricSpecularAA = settings.bGeometricSpecularAA;

		InitPipeline();
	}

	void RenderTextLitTask::Draw(const Ref<CommandBuffer>& cmd, const Ref<PipelineGraphics>& pipeline, const QuadsRenderData<LitTextGeometryData>::BlendModeGeomType& data, const void* vertexPushData, RenderStats& stats)
	{
		if (data.IsEmpty())
			return;

		cmd->BeginGraphics(pipeline);
		cmd->SetGraphicsRootConstants(vertexPushData, nullptr);

		uint32_t quadsCount = (uint32_t)(data.ShadowCastingQuads.QuadVertices.size() / 4);
		if (quadsCount > 0)
		{
			cmd->DrawIndexed(data.ShadowCastingQuads.VertexBuffer, data.ShadowCastingQuads.IndexBuffer, quadsCount * 6, 0, 0);
			++stats.DrawCalls;
		}
		quadsCount = (uint32_t)(data.NonShadowQuads.QuadVertices.size() / 4);
		if (quadsCount > 0)
		{
			cmd->DrawIndexed(data.NonShadowQuads.VertexBuffer, data.NonShadowQuads.IndexBuffer, quadsCount * 6, 0, 0);
			++stats.DrawCalls;
		}
		
		cmd->EndGraphics();
	}

	void RenderTextLitTask::Draw(const Ref<CommandBuffer>& cmd, const Ref<PipelineGraphics>& pipeline, const LitTextGeometryData& data, const void* vertexPushData, RenderStats& stats, const Ref<Framebuffer>& fb)
	{
		const uint32_t quadsCount = (uint32_t)(data.QuadVertices.size() / 4);
		if (quadsCount == 0)
			return;

		if (fb)
			cmd->BeginGraphics(pipeline, fb);
		else
			cmd->BeginGraphics(pipeline);

		cmd->SetGraphicsRootConstants(vertexPushData, nullptr);
		cmd->DrawIndexed(data.VertexBuffer, data.IndexBuffer, quadsCount * 6, 0, 0);
		cmd->EndGraphics();
		++stats.DrawCalls;
	}

	void RenderTextLitTask::RecordCommandBuffer(const Ref<CommandBuffer>& cmd)
	{
		RenderOpaque(cmd);
		RenderMasked(cmd);
	}

	void RenderTextLitTask::RenderOpaque(const Ref<CommandBuffer>& cmd)
	{
		const auto& singleSided = m_Renderer.GetSingleSidedTextsRenderData();
		const auto& doubleSided = m_Renderer.GetDoubleSidedTextsRenderData();

		if (singleSided.Opaque.IsEmpty() && doubleSided.Opaque.IsEmpty())
			return;

		EG_CPU_TIMING_SCOPED("Render Opaque Text3D Lit");
		EG_GPU_TIMING_SCOPED(cmd, "Render Opaque Text3D Lit");

		PushData pushData;
		pushData.ViewProj = m_Renderer.GetViewProjection();

		const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
		const bool bTexturesDirty = texturesChangedFrame >= m_OpaqueTexturesUpdatedFrames[RenderManager::GetCurrentFrameIndex()];
		if (bTexturesDirty)
		{
			m_OpaquePipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
			m_OpaqueTexturesUpdatedFrames[RenderManager::GetCurrentFrameIndex()] = texturesChangedFrame + 1;
		}
		m_OpaquePipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
		m_OpaquePipeline->SetBuffer(MaterialSystem::GetMaterialsRawBuffer(), EG_PERSISTENT_SET, EG_BINDING_RAW_MATERIALS);

		m_OpaquePipeline->SetBuffer(m_Renderer.GetTextsTransformsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MAX);
		if (bMotionRequired)
		{
			pushData.PrevViewProj = m_Renderer.GetPrevViewProjection();
			m_OpaquePipeline->SetBuffer(m_Renderer.GetTextsPrevTransformBuffer(), EG_PERSISTENT_SET, EG_BINDING_MAX + 1);
		}
		m_OpaquePipeline->SetTextureArray(m_Renderer.GetAtlases(), 1, 0);

		auto& stats = m_Renderer.GetStats();

		cmd->SetGraphicsCullMode(CullMode::Back);
		Draw(cmd, m_OpaquePipeline, singleSided.Opaque, &pushData, stats);

		cmd->SetGraphicsCullMode(CullMode::None);
		Draw(cmd, m_OpaquePipeline, doubleSided.Opaque, &pushData, stats);
	}

	void RenderTextLitTask::RenderMasked(const Ref<CommandBuffer>& cmd)
	{
		const auto& singleSided = m_Renderer.GetSingleSidedTextsRenderData();
		const auto& doubleSided = m_Renderer.GetDoubleSidedTextsRenderData();

		if (singleSided.Masked.IsEmpty() && doubleSided.Masked.IsEmpty())
			return;

		EG_CPU_TIMING_SCOPED("Render Masked Text3D Lit");
		EG_GPU_TIMING_SCOPED(cmd, "Render Masked Text3D Lit");

		PushData pushData;
		pushData.ViewProj = m_Renderer.GetViewProjection();

		const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
		const bool bTexturesDirty = texturesChangedFrame >= m_MaskedTexturesUpdatedFrames[RenderManager::GetCurrentFrameIndex()];
		if (bTexturesDirty)
		{
			m_MaskedPipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
			m_MaskedTexturesUpdatedFrames[RenderManager::GetCurrentFrameIndex()] = texturesChangedFrame + 1;
		}
		m_MaskedPipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
		m_MaskedPipeline->SetBuffer(MaterialSystem::GetMaterialsRawBuffer(), EG_PERSISTENT_SET, EG_BINDING_RAW_MATERIALS);

		m_MaskedPipeline->SetBuffer(m_Renderer.GetTextsTransformsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MAX);
		if (bMotionRequired)
		{
			pushData.PrevViewProj = m_Renderer.GetPrevViewProjection();
			m_MaskedPipeline->SetBuffer(m_Renderer.GetTextsPrevTransformBuffer(), EG_PERSISTENT_SET, EG_BINDING_MAX + 1);
		}
		m_MaskedPipeline->SetTextureArray(m_Renderer.GetAtlases(), 1, 0);

		auto& stats = m_Renderer.GetStats();
		cmd->SetGraphicsCullMode(CullMode::Back);
		Draw(cmd, m_MaskedPipeline, singleSided.Masked, &pushData, stats);

		cmd->SetGraphicsCullMode(CullMode::None);
		Draw(cmd, m_MaskedPipeline, doubleSided.Masked, &pushData, stats);
	}

	void RenderTextLitTask::InitPipeline()
	{
		const auto& gbuffer = m_Renderer.GetGBuffer();

		ColorAttachment colorAttachment;
		colorAttachment.ClearOperation = ClearOperation::Load;
		colorAttachment.InitialLayout = ImageLayoutType::RenderTarget;
		colorAttachment.FinalLayout = ImageLayoutType::RenderTarget;
		colorAttachment.Image = gbuffer.Albedo;

		ColorAttachment normalsAttachment;
		normalsAttachment.ClearOperation = ClearOperation::Load;
		normalsAttachment.InitialLayout = ImageLayoutType::RenderTarget;
		normalsAttachment.FinalLayout = ImageLayoutType::RenderTarget;
		normalsAttachment.Image = gbuffer.Normals;

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
		if (bGeometricSpecularAA)
			fragmentDefines["EG_GEOMETRIC_SPECULAR_AA"] = "";

		PipelineGraphicsState state;
		state.VertexShader = Shader::Create("text/text_lit.vert", ShaderType::Vertex, vertexDefines);
		state.FragmentShader = Shader::Create("text/text_lit.frag", ShaderType::Fragment, fragmentDefines);
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
		state.DepthStencilAttachment = depthAttachment;
		state.CullMode = CullMode::Dynamic;

		if (m_OpaquePipeline)
			m_OpaquePipeline->SetState(state);
		else
			m_OpaquePipeline = PipelineGraphics::Create(state);

		fragmentDefines["EG_MASKED"] = "";
		state.FragmentShader = Shader::Create("text/text_lit.frag", ShaderType::Fragment, fragmentDefines);
		if (m_MaskedPipeline)
			m_MaskedPipeline->SetState(state);
		else
			m_MaskedPipeline = PipelineGraphics::Create(state);
	}
}
