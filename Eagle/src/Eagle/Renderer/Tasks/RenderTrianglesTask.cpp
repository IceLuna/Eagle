#include "egpch.h"
#include "RenderTrianglesTask.h"

#include "Eagle/Renderer/RenderManager.h"
#include "Eagle/Renderer/SceneRenderer.h"
#include "Eagle/Renderer/VidWrappers/RenderCommandManager.h"
#include "Eagle/Renderer/VidWrappers/Buffer.h"

#include "Eagle/Debug/CPUTimings.h"
#include "Eagle/Debug/GPUTimings.h"

namespace Eagle
{
	RenderTrianglesTask::RenderTrianglesTask(SceneRenderer& renderer)
		: RendererTask(renderer)
	{
		InitPipeline();

		BufferSpecifications linesVertexSpecs;
		linesVertexSpecs.Size = s_BaseLinesVertexBufferSize;
		linesVertexSpecs.Layout = BufferReadAccess::Vertex;
		linesVertexSpecs.Usage = BufferUsage::VertexBuffer | BufferUsage::TransferDst;

		m_VertexBuffer = Buffer::Create(linesVertexSpecs, "DebugTrianglesVertexBuffer");
		m_Vertices.reserve(s_DefaultTrianglesVerticesCount);
	}

	void RenderTrianglesTask::RecordCommandBuffer(const Ref<CommandBuffer>& cmd)
	{
		if (m_Vertices.empty())
			return;

		EG_CPU_TIMING_SCOPED("Debug triangles");
		EG_GPU_TIMING_SCOPED(cmd, "Debug triangles");

		UploadVertexBuffer(cmd);
		RenderTriangles(cmd);
	}

	void RenderTrianglesTask::SetDebugTriangles(const std::vector<RendererTriangle>& triangles)
	{
		std::vector<RendererDebugVertex> tempData;
		tempData.reserve(triangles.size() * 3);

		for (auto& triangle : triangles)
		{
			for (const auto& vertex : triangle.Vertices)
				tempData.push_back(vertex);
		}

		RenderManager::Submit([task = shared_from_this(), vertices = std::move(tempData)](const Ref<CommandBuffer>& cmd) mutable
		{
			auto thisRef = Cast<RenderTrianglesTask>(task);
			thisRef->m_Vertices = std::move(vertices);
		});
	}

	void RenderTrianglesTask::RenderTriangles(const Ref<CommandBuffer>& cmd)
	{
		EG_CPU_TIMING_SCOPED("Render Debug triangles");
		EG_GPU_TIMING_SCOPED(cmd, "Render Debug triangles");

		const uint32_t trianglesCount = (uint32_t)(m_Vertices.size());

		cmd->BeginGraphics(m_Pipeline);
		cmd->SetGraphicsRootConstants(&m_Renderer.GetViewProjection()[0][0], nullptr);
		cmd->Draw(m_VertexBuffer, trianglesCount, 0);
		cmd->EndGraphics();

		auto& stats = m_Renderer.GetStats();
		++stats.DrawCalls;
	}

	void RenderTrianglesTask::UploadVertexBuffer(const Ref<CommandBuffer>& cmd)
	{
		EG_CPU_TIMING_SCOPED("Upload Debug triangles data");
		EG_GPU_TIMING_SCOPED(cmd, "Upload Debug triangles data");

		const size_t currentVertexSize = m_Vertices.size() * sizeof(RendererDebugVertex);
		auto& vb = m_VertexBuffer;
		if (currentVertexSize > vb->GetSize())
		{
			size_t newSize = glm::max(currentVertexSize, vb->GetSize() * 3 / 2);
			constexpr size_t alignment = 4 * sizeof(RendererDebugVertex);
			newSize += alignment - (newSize % alignment);
			vb->Resize(newSize);
		}

		cmd->Write(vb, m_Vertices.data(), currentVertexSize, 0, vb->GetLayout(), BufferReadAccess::Vertex);
	}

	void RenderTrianglesTask::InitPipeline()
	{
		ColorAttachment colorAttachment;
		colorAttachment.ClearOperation = ClearOperation::Load;
		colorAttachment.InitialLayout = ImageLayoutType::RenderTarget;
		colorAttachment.FinalLayout = ImageLayoutType::RenderTarget;
		colorAttachment.Image = m_Renderer.GetHDROutput();

		DepthStencilAttachment depthAttachment;
		depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.Image = m_Renderer.GetGBuffer().Depth;
		depthAttachment.ClearOperation = ClearOperation::Load;
		depthAttachment.bWriteDepth = true;
		depthAttachment.DepthCompareOp = CompareOperation::GreaterEqual;

		PipelineGraphicsState state;
		state.VertexShader = Shader::Create("simple_colored_geometry.vert", ShaderType::Vertex);
		state.FragmentShader = Shader::Create("simple_colored_geometry.frag", ShaderType::Fragment);
		state.ColorAttachments.push_back(colorAttachment);
		if (bEnableDebugLinesDepthTest)
		{
			state.DepthStencilAttachment = depthAttachment;
		}
		state.Topology = Topology::Triangles;

		if (m_Pipeline)
			m_Pipeline->SetState(state);
		else
			m_Pipeline = PipelineGraphics::Create(state);
	}
}
