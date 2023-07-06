#include "egpch.h"
#include "RenderLinesTask.h"

#include "Eagle/Renderer/RenderManager.h"
#include "Eagle/Renderer/SceneRenderer.h"
#include "Eagle/Renderer/VidWrappers/RenderCommandManager.h"
#include "Eagle/Renderer/VidWrappers/Buffer.h"

#include "Eagle/Debug/CPUTimings.h"
#include "Eagle/Debug/GPUTimings.h"

namespace Eagle
{
	RenderLinesTask::RenderLinesTask(SceneRenderer& renderer)
		: RendererTask(renderer)
	{
		m_LineWidth = m_Renderer.GetOptions_RT().LineWidth;
		InitPipeline();

		BufferSpecifications linesVertexSpecs;
		linesVertexSpecs.Size = s_BaseLinesVertexBufferSize;
		linesVertexSpecs.Layout = BufferReadAccess::Vertex;
		linesVertexSpecs.Usage = BufferUsage::VertexBuffer | BufferUsage::TransferDst;

		m_VertexBuffer = Buffer::Create(linesVertexSpecs, "LinesVertexBuffer");
		m_Vertices.reserve(s_DefaultLinesVerticesCount);
	}

	void RenderLinesTask::RecordCommandBuffer(const Ref<CommandBuffer>& cmd)
	{
		if (m_Vertices.empty())
			return;

		EG_CPU_TIMING_SCOPED("Debug lines");
		EG_GPU_TIMING_SCOPED(cmd, "Debug lines");

		UploadVertexBuffer(cmd);
		RenderLines(cmd);
	}

	void RenderLinesTask::SetDebugLines(const std::vector<RendererLine>& lines)
	{
		std::vector<LineVertex> tempData;
		tempData.reserve(lines.size() * 2);

		for (auto& line : lines)
		{
			tempData.push_back({ line.Color, line.Start });
			tempData.push_back({ line.Color, line.End });
		}

		RenderManager::Submit([this, vertices = std::move(tempData)](Ref<CommandBuffer>& cmd) mutable
		{
			m_Vertices = std::move(vertices);
		});
	}

	void RenderLinesTask::RenderLines(const Ref<CommandBuffer>& cmd)
	{
		EG_CPU_TIMING_SCOPED("Render Debug lines");
		EG_GPU_TIMING_SCOPED(cmd, "Render Debug lines");

		const uint32_t linesCount = (uint32_t)(m_Vertices.size());

		cmd->BeginGraphics(m_Pipeline);
		cmd->SetGraphicsRootConstants(&m_Renderer.GetViewProjection()[0][0], nullptr);
		cmd->Draw(m_VertexBuffer, linesCount, 0);
		cmd->EndGraphics();
	}

	void RenderLinesTask::UploadVertexBuffer(const Ref<CommandBuffer>& cmd)
	{
		EG_CPU_TIMING_SCOPED("Upload Debug lines data");
		EG_GPU_TIMING_SCOPED(cmd, "Upload Debug lines data");

		const size_t currentVertexSize = m_Vertices.size() * sizeof(LineVertex);
		auto& vb = m_VertexBuffer;
		if (currentVertexSize > vb->GetSize())
		{
			size_t newSize = glm::max(currentVertexSize, vb->GetSize() * 3 / 2);
			constexpr size_t alignment = 4 * sizeof(LineVertex);
			newSize += alignment - (newSize % alignment);
			vb->Resize(newSize);
		}

		cmd->Write(vb, m_Vertices.data(), currentVertexSize, 0, BufferLayoutType::Unknown, BufferReadAccess::Vertex);
		cmd->TransitionLayout(vb, BufferReadAccess::Vertex, BufferReadAccess::Vertex);
	}

	void RenderLinesTask::InitPipeline()
	{
		ColorAttachment colorAttachment;
		colorAttachment.ClearOperation = ClearOperation::Load;
		colorAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
		colorAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		colorAttachment.Image = m_Renderer.GetOutput();

		DepthStencilAttachment depthAttachment;
		depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.Image = m_Renderer.GetGBuffer().Depth;
		depthAttachment.ClearOperation = ClearOperation::Load;
		depthAttachment.bWriteDepth = true;
		depthAttachment.DepthCompareOp = CompareOperation::Less;

		PipelineGraphicsState state;
		state.VertexShader = Shader::Create("assets/shaders/line.vert", ShaderType::Vertex);
		state.FragmentShader = Shader::Create("assets/shaders/line.frag", ShaderType::Fragment);
		state.ColorAttachments.push_back(colorAttachment);
		state.DepthStencilAttachment = depthAttachment;
		state.Topology = Topology::Lines;
		state.LineWidth = m_LineWidth;

		m_Pipeline = PipelineGraphics::Create(state);
	}
}
