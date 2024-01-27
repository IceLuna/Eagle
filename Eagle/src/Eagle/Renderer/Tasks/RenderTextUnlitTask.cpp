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
	RenderTextUnlitTask::RenderTextUnlitTask(SceneRenderer& renderer, const Ref<Image>& renderTo)
		: RendererTask(renderer)
		, m_ResultImage(renderTo)
	{
		bJitter = m_Renderer.GetOptions().InternalState.bJitter;
		InitPipeline();
	}

	static void Draw(const Ref<CommandBuffer>& cmd, Ref<PipelineGraphics>& pipeline, const UnlitTextGeometryData& data, const void* pushData, SceneRenderer::Statistics2D& stats)
	{
		if (data.QuadVertices.empty())
			return;

		const uint32_t quadsCount = (uint32_t)(data.QuadVertices.size() / 4);
		cmd->BeginGraphics(pipeline);
		cmd->SetGraphicsRootConstants(pushData, nullptr);
		cmd->DrawIndexed(data.VertexBuffer, data.IndexBuffer, quadsCount * 6, 0, 0);
		cmd->EndGraphics();
		++stats.DrawCalls;
		stats.QuadCount += quadsCount;
	}

	void RenderTextUnlitTask::RecordCommandBuffer(const Ref<CommandBuffer>& cmd)
	{
		const auto& data = m_Renderer.GetUnlitTextData();
		const auto& notCastingShadowsData = m_Renderer.GetUnlitNotCastingShadowTextData();

		if (data.QuadVertices.empty() && notCastingShadowsData.QuadVertices.empty())
			return;

		EG_CPU_TIMING_SCOPED("Render Text3D Unlit");
		EG_GPU_TIMING_SCOPED(cmd, "Render Text3D Unlit");

		m_Pipeline->SetBuffer(m_Renderer.GetTextsTransformsBuffer(), 0, 0);
		m_Pipeline->SetTextureArray(m_Renderer.GetAtlases(), 1, 0);
		if (bJitter)
			m_Pipeline->SetBuffer(m_Renderer.GetJitter(), 2, 0);

		Draw(cmd, m_Pipeline, data, &m_Renderer.GetViewProjection()[0][0], m_Renderer.GetStats2D());
		Draw(cmd, m_Pipeline, notCastingShadowsData, &m_Renderer.GetViewProjection()[0][0], m_Renderer.GetStats2D());
	}

	void RenderTextUnlitTask::InitPipeline()
	{
		ColorAttachment colorAttachment;
		colorAttachment.Image = m_ResultImage;
		colorAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
		colorAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
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
		objectIDAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
		objectIDAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		objectIDAttachment.ClearOperation = ClearOperation::Load;

		DepthStencilAttachment depthAttachment;
		depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.Image = m_Renderer.GetGBuffer().Depth;
		depthAttachment.bWriteDepth = true;
		depthAttachment.DepthCompareOp = CompareOperation::Less;
		depthAttachment.ClearOperation = ClearOperation::Load;

		ShaderDefines defines;
		if (bJitter)
			defines["EG_JITTER"] = "";

		PipelineGraphicsState state;
		state.VertexShader = Shader::Create("text.vert", ShaderType::Vertex, defines);
		state.FragmentShader = Shader::Create("text.frag", ShaderType::Fragment);
		state.ColorAttachments.push_back(colorAttachment);
		state.ColorAttachments.push_back(objectIDAttachment);
		state.DepthStencilAttachment = depthAttachment;
		state.CullMode = CullMode::None;

		if (m_Pipeline)
			m_Pipeline->SetState(state);
		else
			m_Pipeline = PipelineGraphics::Create(state);
	}
}
