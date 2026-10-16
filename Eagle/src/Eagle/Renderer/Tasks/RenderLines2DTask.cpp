#include "egpch.h"
#include "RenderLines2DTask.h"

#include "Eagle/Renderer/RenderManager.h"
#include "Eagle/Renderer/SceneRenderer.h"
#include "Eagle/Renderer/VidWrappers/RenderCommandManager.h"
#include "Eagle/Renderer/VidWrappers/Buffer.h"

#include "Eagle/Debug/CPUTimings.h"
#include "Eagle/Debug/GPUTimings.h"

namespace Eagle
{
	RenderLines2DTask::RenderLines2DTask(SceneRenderer& renderer)
		: RendererTask(renderer)
	{
		m_LineWidth = m_Renderer.GetOptions().LineWidth;
		InitPipeline();

		BufferSpecifications vertexSpecs;
		vertexSpecs.Size = s_BaseVertexBufferSize;
		vertexSpecs.Layout = BufferReadAccess::Vertex;
		vertexSpecs.Usage = BufferUsage::VertexBuffer | BufferUsage::TransferDst;
		m_VertexBuffer = Buffer::Create(vertexSpecs, "DebugLines2D_VertexBuffer");
	}

	void RenderLines2DTask::SetLines(const std::vector<RendererLine2D>& lines)
	{
		std::vector<RendererDebugVertex2D> vertices;
		vertices.reserve(lines.size() * 2);
		for (const auto& line : lines)
		{
			vertices.push_back(line.Start);
			vertices.push_back(line.End);
		}

		RenderManager::Submit([task = shared_from_this(), vertices = std::move(vertices)](const Ref<CommandBuffer>&) mutable
		{
			auto thisRef = Cast<RenderLines2DTask>(task);
			thisRef->m_Vertices = std::move(vertices);
		});
	}

	void RenderLines2DTask::RecordCommandBuffer(const Ref<CommandBuffer>& cmd)
	{
		if (m_Vertices.empty())
			return;

		EG_CPU_TIMING_SCOPED("Debug lines 2D");
		EG_GPU_TIMING_SCOPED(cmd, "Debug lines 2D");

		// Upload
		const size_t verticesSize = m_Vertices.size() * sizeof(RendererDebugVertex2D);
		if (verticesSize > m_VertexBuffer->GetSize())
		{
			size_t newSize = glm::max(verticesSize, m_VertexBuffer->GetSize() * 3 / 2);
			constexpr size_t alignment = 2 * sizeof(RendererDebugVertex2D);
			newSize = (newSize + alignment - 1) / alignment * alignment;
			m_VertexBuffer->Resize(newSize);
		}
		cmd->Write(m_VertexBuffer, m_Vertices.data(), verticesSize, 0, m_VertexBuffer->GetLayout(), BufferReadAccess::Vertex);

		// Render
		struct PushData
		{
			glm::vec2 ViewportSize;
			float UIScale;
		} pushData;
		pushData.ViewportSize = glm::vec2(m_Renderer.GetOutput()->GetSize());
		pushData.UIScale = m_Renderer.GetUIScaleSettings_RT().GetScale(pushData.ViewportSize);

		if (m_Pipeline->GetState().ColorAttachments[0].Image != m_Renderer.GetOutput())
			InitPipeline();

		cmd->BeginGraphics(m_Pipeline);
		cmd->SetGraphicsRootConstants(&pushData, nullptr);
		cmd->Draw(m_VertexBuffer, (uint32_t)m_Vertices.size(), 0);
		cmd->EndGraphics();

		auto& stats = m_Renderer.GetStats();
		++stats.DrawCalls;
	}

	void RenderLines2DTask::InitPipeline()
	{
		ColorAttachment colorAttachment;
		colorAttachment.Image = m_Renderer.GetOutput();
		colorAttachment.InitialLayout = ImageLayoutType::RenderTarget;
		colorAttachment.FinalLayout = ImageLayoutType::RenderTarget;
		colorAttachment.ClearOperation = ClearOperation::Load;

		colorAttachment.bBlendEnabled = true;
		colorAttachment.BlendingState.BlendOp = BlendOperation::Add;
		colorAttachment.BlendingState.BlendSrc = BlendFactor::SrcAlpha;
		colorAttachment.BlendingState.BlendDst = BlendFactor::OneMinusSrcAlpha;

		colorAttachment.BlendingState.BlendOpAlpha = BlendOperation::Add;
		colorAttachment.BlendingState.BlendSrcAlpha = BlendFactor::Zero;
		colorAttachment.BlendingState.BlendDstAlpha = BlendFactor::One;

		PipelineGraphicsState state;
		state.VertexShader = Shader::Create("lines2D.vert", ShaderType::Vertex);
		state.FragmentShader = Shader::Create("lines2D.frag", ShaderType::Fragment);
		state.ColorAttachments.push_back(colorAttachment);
		state.Topology = Topology::Lines;
		state.LineWidth = m_LineWidth;
		state.CullMode = CullMode::None;

		if (m_Pipeline)
			m_Pipeline->SetState(state);
		else
			m_Pipeline = PipelineGraphics::Create(state);
	}
}
