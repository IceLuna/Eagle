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
		InitPipelines();

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
		auto IsOpaque = [](const RendererTriangle& triangle)
		{
			return std::all_of(triangle.Vertices.begin(), triangle.Vertices.end(), [](const RendererDebugVertex& vertex) { return vertex.Opacity >= 1.f; });
		};

		std::vector<RendererDebugVertex> tempData;
		tempData.reserve(triangles.size() * 3);

		// Opaque triangles first
		uint32_t opaqueVerticesCount = 0u;
		for (const bool bOpaque : { true, false })
		{
			for (auto& triangle : triangles)
			{
				if (IsOpaque(triangle) != bOpaque)
					continue;

				for (const auto& vertex : triangle.Vertices)
					tempData.push_back(vertex);
			}
			if (bOpaque)
				opaqueVerticesCount = (uint32_t)tempData.size();
		}

		RenderManager::Submit([task = shared_from_this(), vertices = std::move(tempData), opaqueVerticesCount](const Ref<CommandBuffer>& cmd) mutable
		{
			auto thisRef = Cast<RenderTrianglesTask>(task);
			thisRef->m_Vertices = std::move(vertices);
			thisRef->m_OpaqueVerticesCount = opaqueVerticesCount;
		});
	}

	void RenderTrianglesTask::RenderTriangles(const Ref<CommandBuffer>& cmd)
	{
		EG_CPU_TIMING_SCOPED("Render Debug triangles");
		EG_GPU_TIMING_SCOPED(cmd, "Render Debug triangles");

		auto& stats = m_Renderer.GetStats();
		auto Draw = [&](const Ref<PipelineGraphics>& pipeline, uint32_t verticesCount, uint32_t firstVertex)
		{
			if (verticesCount == 0u)
				return;

			cmd->BeginGraphics(pipeline);
			cmd->SetGraphicsRootConstants(&m_Renderer.GetViewProjection()[0][0], nullptr);
			cmd->Draw(m_VertexBuffer, verticesCount, firstVertex);
			cmd->EndGraphics();
			++stats.DrawCalls;
		};

		const uint32_t verticesCount = (uint32_t)m_Vertices.size();
		Draw(m_Pipeline, m_OpaqueVerticesCount, 0u);
		Draw(m_TranslucentPipeline, verticesCount - m_OpaqueVerticesCount, m_OpaqueVerticesCount);
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

	void RenderTrianglesTask::InitPipelines()
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

		ColorAttachment& translucentColor = state.ColorAttachments[0];
		translucentColor.bBlendEnabled = true;
		translucentColor.BlendingState.BlendOp = BlendOperation::Add;
		translucentColor.BlendingState.BlendSrc = BlendFactor::SrcAlpha;
		translucentColor.BlendingState.BlendDst = BlendFactor::OneMinusSrcAlpha;
		translucentColor.BlendingState.BlendOpAlpha = BlendOperation::Add;
		translucentColor.BlendingState.BlendSrcAlpha = BlendFactor::SrcAlpha;
		translucentColor.BlendingState.BlendDstAlpha = BlendFactor::OneMinusSrcAlpha;
		state.DepthStencilAttachment.bWriteDepth = false;

		if (m_TranslucentPipeline)
			m_TranslucentPipeline->SetState(state);
		else
			m_TranslucentPipeline = PipelineGraphics::Create(state);
	}
}
