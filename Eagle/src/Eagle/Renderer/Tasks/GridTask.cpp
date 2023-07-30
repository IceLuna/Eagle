#include "egpch.h"
#include "GridTask.h"
#include "Eagle/Renderer/SceneRenderer.h"

#include "Eagle/Renderer/VidWrappers/RenderCommandManager.h"

#include "Eagle/Debug/CPUTimings.h"
#include "Eagle/Debug/GPUTimings.h"

#include "glm/gtc/matrix_transform.hpp"

namespace Eagle
{
	GridTask::GridTask(SceneRenderer& renderer, const Ref<Image>& output)
		: RendererTask(renderer)
		, m_Output(output)
	{
		bJitter = m_Renderer.GetOptions().InternalState.bJitter;
		InitPipeline();
	}

	void GridTask::RecordCommandBuffer(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "Editor Grid");
		EG_CPU_TIMING_SCOPED("Editor Grid");

		struct PushData
		{
			float GridSize = 0.025f;
			float GridScale;
		} pushData;
		static_assert(sizeof(PushData) <= 128);

		const float scale = m_Renderer.GetOptions_RT().GridScale;
		pushData.GridScale = scale * 2.00f + pushData.GridSize;

		const glm::mat4 transform = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)) * glm::scale(glm::mat4(1.0f), glm::vec3(scale));
		const glm::mat4 mvp = m_Renderer.GetViewProjection() * transform;

		if (bJitter)
			m_Pipeline->SetBuffer(m_Renderer.GetJitter(), 0, 0);

		cmd->BeginGraphics(m_Pipeline);
		cmd->SetGraphicsRootConstants(&mvp, &pushData);
		cmd->Draw(6, 0);
		cmd->EndGraphics();
	}
	
	void GridTask::InitPipeline()
	{
		ColorAttachment attachment;
		attachment.Image = m_Output;
		attachment.ClearOperation = ClearOperation::Load;
		attachment.InitialLayout = ImageReadAccess::PixelShaderRead;
		attachment.FinalLayout = ImageReadAccess::PixelShaderRead;

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
		depthAttachment.DepthCompareOp = CompareOperation::LessEqual;
		depthAttachment.DepthBias = -200.f;

		ShaderDefines defines;
		if (bJitter)
			defines["EG_JITTER"] = "";

		PipelineGraphicsState state;
		state.ColorAttachments.push_back(attachment);
		state.DepthStencilAttachment = depthAttachment;
		state.VertexShader = Shader::Create("assets/shaders/grid_quad.vert", ShaderType::Vertex, defines);
		state.FragmentShader = Shader::Create("assets/shaders/grid.frag", ShaderType::Fragment);

		if (m_Pipeline)
			m_Pipeline->SetState(state);
		else
			m_Pipeline = PipelineGraphics::Create(state);
	}
}
