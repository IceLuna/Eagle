#include "egpch.h"
#include "GridTask.h"
#include "Eagle/Renderer/SceneRenderer.h"

#include "Eagle/Renderer/VidWrappers/RenderCommandManager.h"

#include "Eagle/Debug/CPUTimings.h"
#include "Eagle/Debug/GPUTimings.h"

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

		const auto& matrices = m_Renderer.GetCameraMatrices();

		const glm::mat4& viewProj = matrices.ViewProjUnjittered;
		struct VertexPushData
		{
			glm::mat4 InvViewProj;
		} vertexData;

		struct FragmentPushData
		{
			// Columns 2 and 3 of `inverse(jittered VP) - inverse(unjittered VP)`. The depth buffer the grid is tested
			// against was rendered with TAA jitter, so the grid's depth is computed along the jittered ray.
			// All zeros when TAA is off
			glm::vec4 JitterDeltaZ;
			glm::vec4 JitterDeltaW;
			float CellSize;
		} fragmentData;
		static_assert(sizeof(VertexPushData) + sizeof(FragmentPushData) <= 128);

		vertexData.InvViewProj = glm::inverse(viewProj);

		// Double precision: the difference is tiny compared to the matrices' values
		const glm::dmat4 jitterDelta = glm::inverse(glm::dmat4(matrices.ViewProj)) - glm::inverse(glm::dmat4(viewProj));
		fragmentData.JitterDeltaZ = glm::vec4(jitterDelta[2]);
		fragmentData.JitterDeltaW = glm::vec4(jitterDelta[3]);
		fragmentData.CellSize = glm::max(m_Renderer.GetOptions_RT().GridCellSize, 0.001f);

		cmd->BeginGraphics(m_Pipeline);
		cmd->SetGraphicsRootConstants(&vertexData, &fragmentData);
		cmd->Draw(3, 0);
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
		depthAttachment.bWriteDepth = false;

		PipelineGraphicsState state;
		state.ColorAttachments.push_back(attachment);
		state.DepthStencilAttachment = depthAttachment;
		state.VertexShader = Shader::Create("grid_quad.vert", ShaderType::Vertex);
		state.FragmentShader = Shader::Create("grid.frag", ShaderType::Fragment);
		state.CullMode = CullMode::None;

		if (m_Pipeline)
			m_Pipeline->SetState(state);
		else
			m_Pipeline = PipelineGraphics::Create(state);
	}
}
