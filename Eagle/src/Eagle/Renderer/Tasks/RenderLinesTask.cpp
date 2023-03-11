#include "egpch.h"
#include "RenderLinesTask.h"

#include "Eagle/Renderer/SceneRenderer.h"
#include "Eagle/Renderer/VidWrappers/RenderCommandManager.h"
#include "Eagle/Renderer/VidWrappers/Buffer.h"

namespace Eagle
{
	RenderLinesTask::RenderLinesTask(SceneRenderer& renderer)
		: RendererTask(renderer)
	{
		InitPipeline();

		BufferSpecifications linesVertexSpecs;
		linesVertexSpecs.Size = s_BaseLinesVertexBufferSize;
		linesVertexSpecs.Usage = BufferUsage::VertexBuffer | BufferUsage::TransferDst;

		m_VertexBuffer = Buffer::Create(linesVertexSpecs, "LinesVertexBuffer");
		m_Vertices.reserve(s_DefaultLinesVerticesCount);
	}

	void RenderLinesTask::RecordCommandBuffer(const Ref<CommandBuffer>& cmd)
	{
		if (m_Vertices.empty())
			return;

		UploadVertexBuffer(cmd);
		Render(cmd);
	}

	void RenderLinesTask::SetDebugLines(const std::vector<RendererLine>& lines)
	{
		m_Vertices.clear();

		for (auto& line : lines)
		{
			m_Vertices.push_back({ line.Color, line.Start });
			m_Vertices.push_back({ line.Color, line.End });
		}
	}

	void RenderLinesTask::Render(const Ref<CommandBuffer>& cmd)
	{
		const uint32_t linesCount = (uint32_t)(m_Vertices.size());

		cmd->BeginGraphics(m_Pipeline);
		cmd->SetGraphicsRootConstants(&m_Renderer.GetViewProjection()[0][0], nullptr);
		cmd->Draw(m_VertexBuffer, linesCount, 0);
		cmd->EndGraphics();
	}

	void RenderLinesTask::UploadVertexBuffer(const Ref<CommandBuffer>& cmd)
	{
		const size_t currentVertexSize = m_Vertices.size() * sizeof(LineVertex);
		auto& vb = m_VertexBuffer;
		if (currentVertexSize > vb->GetSize())
		{
			size_t newSize = glm::max(currentVertexSize, vb->GetSize() * 3 / 2);
			constexpr size_t alignment = 4 * sizeof(LineVertex);
			newSize += alignment - (newSize % alignment);
			vb->Resize(newSize);
		}

		cmd->Write(vb, m_Vertices.data(), m_Vertices.size() * sizeof(LineVertex), 0, BufferLayoutType::Unknown, BufferReadAccess::Vertex);
		cmd->TransitionLayout(vb, BufferReadAccess::Vertex, BufferReadAccess::Vertex);
	}

	void RenderLinesTask::InitPipeline()
	{
		ColorAttachment colorAttachment;
		colorAttachment.bClearEnabled = false;
		colorAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
		colorAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		colorAttachment.Image = m_Renderer.GetOutput();

		DepthStencilAttachment depthAttachment;
		depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.Image = m_Renderer.GetGBuffer().Depth;
		depthAttachment.bClearEnabled = false;
		depthAttachment.bWriteDepth = true;
		depthAttachment.DepthCompareOp = CompareOperation::Less;

		PipelineGraphicsState state;
		state.VertexShader = Shader::Create("assets/shaders/line.vert", ShaderType::Vertex);
		state.FragmentShader = Shader::Create("assets/shaders/line.frag", ShaderType::Fragment);
		state.ColorAttachments.push_back(colorAttachment);
		state.DepthStencilAttachment = depthAttachment;
		state.Topology = Topology::Lines;
		state.LineWidth = 5.f;

		m_Pipeline = PipelineGraphics::Create(state);
	}
}
