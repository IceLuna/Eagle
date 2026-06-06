#include "egpch.h"
#include "RenderTextUnlitTask.h"

#include "Eagle/Renderer/RenderManager.h"
#include "Eagle/Renderer/VidWrappers/Buffer.h"
#include "Eagle/Renderer/VidWrappers/RenderCommandManager.h"

#include "Eagle/Components/Components.h"

#include "Eagle/Debug/CPUTimings.h"
#include "Eagle/Debug/GPUTimings.h"

#include "../../Eagle-Editor/assets/shaders/defines.h"

#include <codecvt>

namespace Eagle
{
	RenderTextUnlitTask::RenderTextUnlitTask(SceneRenderer& renderer)
		: RendererTask(renderer)
	{
		InitPipeline();
	}

	void RenderTextUnlitTask::Draw(const Ref<CommandBuffer>& cmd, const Ref<PipelineGraphics>& pipeline, const QuadsRenderData<UnlitTextGeometryData>::BlendModeGeomType& data, const void* pushData, RenderStats& stats)
	{
		if (data.IsEmpty())
			return;

		cmd->BeginGraphics(pipeline);
		cmd->SetGraphicsRootConstants(pushData, nullptr);

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

	void RenderTextUnlitTask::Draw(const Ref<CommandBuffer>& cmd, const Ref<PipelineGraphics>& pipeline, const UnlitTextGeometryData& data, const void* pushData, RenderStats& stats, const Ref<Framebuffer>& fb)
	{
		const uint32_t quadsCount = (uint32_t)(data.QuadVertices.size() / 4);
		if (quadsCount == 0)
			return;

		if (fb)
			cmd->BeginGraphics(pipeline, fb);
		else
			cmd->BeginGraphics(pipeline);
		cmd->SetGraphicsRootConstants(pushData, nullptr);
		cmd->DrawIndexed(data.VertexBuffer, data.IndexBuffer, quadsCount * 6, 0, 0);
		cmd->EndGraphics();
		++stats.DrawCalls;
	}

	void RenderTextUnlitTask::RecordCommandBuffer(const Ref<CommandBuffer>& cmd)
	{
		const auto& singleSided = m_Renderer.GetSingleSidedUnlitTextsRenderData();
		const auto& doubleSided = m_Renderer.GetDoubleSidedUnlitTextsRenderData();

		if (singleSided.Opaque.IsEmpty() && doubleSided.Opaque.IsEmpty())
			return;

		EG_CPU_TIMING_SCOPED("Render Text3D Unlit");
		EG_GPU_TIMING_SCOPED(cmd, "Render Text3D Unlit");

		m_Pipeline->SetBuffer(m_Renderer.GetTextsTransformsBuffer(), 0, 0);
		m_Pipeline->SetTextureArray(m_Renderer.GetAtlases(), 1, 0);

		const auto& vp = m_Renderer.GetViewProjection();
		auto& stats = m_Renderer.GetStats();
		cmd->SetGraphicsCullMode(CullMode::Back);
		Draw(cmd, m_Pipeline, singleSided.Opaque, glm::value_ptr(vp), stats);

		cmd->SetGraphicsCullMode(CullMode::None);
		Draw(cmd, m_Pipeline, doubleSided.Opaque, glm::value_ptr(vp), stats);
	}

	void RenderTextUnlitTask::InitPipeline()
	{
		ColorAttachment colorAttachment;
		colorAttachment.Image = m_Renderer.GetHDROutput();
		colorAttachment.InitialLayout = ImageLayoutType::RenderTarget;
		colorAttachment.FinalLayout = ImageLayoutType::RenderTarget;
		colorAttachment.ClearOperation = ClearOperation::Load;

		colorAttachment.bBlendEnabled = true;
		colorAttachment.BlendingState.BlendOp = BlendOperation::Add;
		colorAttachment.BlendingState.BlendSrc = BlendFactor::SrcAlpha;
		colorAttachment.BlendingState.BlendDst = BlendFactor::OneMinusSrcAlpha;

		colorAttachment.BlendingState.BlendOpAlpha = BlendOperation::Add;
		colorAttachment.BlendingState.BlendSrcAlpha = BlendFactor::SrcAlpha;
		colorAttachment.BlendingState.BlendDstAlpha = BlendFactor::OneMinusSrcAlpha;

		ColorAttachment objectIDAttachment;
		objectIDAttachment.Image = m_Renderer.GetGBuffer().ObjectID;
		objectIDAttachment.InitialLayout = ImageLayoutType::RenderTarget;
		objectIDAttachment.FinalLayout = ImageLayoutType::RenderTarget;
		objectIDAttachment.ClearOperation = ClearOperation::Load;

		DepthStencilAttachment depthAttachment;
		depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.Image = m_Renderer.GetGBuffer().Depth;
		depthAttachment.bWriteDepth = true;
		depthAttachment.DepthCompareOp = CompareOperation::GreaterEqual;
		depthAttachment.ClearOperation = ClearOperation::Load;

		PipelineGraphicsState state;
		state.VertexShader = Shader::Create("text/text.vert", ShaderType::Vertex);
		state.FragmentShader = Shader::Create("text/text.frag", ShaderType::Fragment);
		state.ColorAttachments.push_back(colorAttachment);
		state.ColorAttachments.push_back(objectIDAttachment);
		state.DepthStencilAttachment = depthAttachment;
		state.CullMode = CullMode::Dynamic;

		if (m_Pipeline)
			m_Pipeline->SetState(state);
		else
			m_Pipeline = PipelineGraphics::Create(state);
	}
}
