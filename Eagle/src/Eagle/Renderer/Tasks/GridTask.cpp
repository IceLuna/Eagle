#include "egpch.h"
#include "GridTask.h"
#include "Eagle/Renderer/SceneRenderer.h"

#include "Eagle/Renderer/VidWrappers/RenderCommandManager.h"

#include "Eagle/Debug/CPUTimings.h"
#include "Eagle/Debug/GPUTimings.h"

#include "glm/gtc/matrix_transform.hpp"

namespace Eagle
{
	GridTask::GridTask(SceneRenderer& renderer)
		: RendererTask(renderer)
	{
		InitPipeline();
	}

	void GridTask::RecordCommandBuffer(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "Editor Grid");
		EG_CPU_TIMING_SCOPED("Editor Grid");

		if (m_Pipeline->GetState().ColorAttachments[0].Image != m_Renderer.GetOutput())
			InitPipeline();

		struct PushData
		{
			float GridSize = 0.025f;
			float GridScale;
		} pushData;
		static_assert(sizeof(PushData) <= 128);

		const float scale = m_Renderer.GetOptions_RT().GridScale;
		pushData.GridScale = scale * 2.00f + pushData.GridSize;

		const glm::mat4 transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.f, 0.005f, 0.f))
			* glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f))
			* glm::scale(glm::mat4(1.0f), glm::vec3(scale));
		const glm::mat4 mvp = m_Renderer.GetViewProjection() * transform;

		cmd->BeginGraphics(m_Pipeline);
		cmd->SetGraphicsRootConstants(&mvp, &pushData);
		cmd->Draw(6, 0);
		cmd->EndGraphics();

		auto& stats = m_Renderer.GetStats();
		++stats.DrawCalls;
	}
	
	void GridTask::InitPipeline()
	{
		ColorAttachment attachment;
		attachment.Image = m_Renderer.GetOutput();
		attachment.ClearOperation = ClearOperation::Load;
		attachment.InitialLayout = ImageLayoutType::RenderTarget;
		attachment.FinalLayout = ImageLayoutType::RenderTarget;

		attachment.bBlendEnabled = true;
		attachment.BlendingState.BlendOp = BlendOperation::Add;
		attachment.BlendingState.BlendSrc = BlendFactor::SrcAlpha;
		attachment.BlendingState.BlendDst = BlendFactor::OneMinusSrcAlpha;

		attachment.BlendingState.BlendOpAlpha = BlendOperation::Add;
		attachment.BlendingState.BlendSrcAlpha = BlendFactor::SrcAlpha;
		attachment.BlendingState.BlendDstAlpha = BlendFactor::OneMinusSrcAlpha;

		DepthStencilAttachment depthAttachment;
		depthAttachment.Image = m_Renderer.GetGBuffer().Depth;
		depthAttachment.ClearOperation = ClearOperation::Load;
		depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.DepthCompareOp = CompareOperation::GreaterEqual;

		PipelineGraphicsState state;
		state.ColorAttachments.push_back(attachment);
		state.DepthStencilAttachment = depthAttachment;
		state.VertexShader = Shader::Create("grid_quad.vert", ShaderType::Vertex);
		state.FragmentShader = Shader::Create("grid.frag", ShaderType::Fragment);

		if (m_Pipeline)
			m_Pipeline->SetState(state);
		else
			m_Pipeline = PipelineGraphics::Create(state);
	}
}
