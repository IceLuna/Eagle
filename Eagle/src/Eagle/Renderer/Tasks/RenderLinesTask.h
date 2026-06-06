#pragma once

#include "RendererTask.h"

#include "Eagle/Renderer/VidWrappers/PipelineGraphics.h"

namespace Eagle
{
	class Buffer;

	class RenderLinesTask : public RendererTask
	{
	public:
		RenderLinesTask(SceneRenderer& renderer);

		void RecordCommandBuffer(const Ref<CommandBuffer>& cmd) override;
		void OnResize(const glm::uvec2 size) override { m_Pipeline->Resize(size.x, size.y); }
		void InitWithOptions(const SceneRendererSettings& settings) override
		{
			if (settings.LineWidth == m_LineWidth
				&& settings.bEnableDebugLinesDepthTest == bEnableDebugLinesDepthTest)
				return;

			m_LineWidth = settings.LineWidth;
			bEnableDebugLinesDepthTest = settings.bEnableDebugLinesDepthTest;

			InitPipeline();
		}

		void SetDebugLines(const std::vector<RendererLine>& lines);

	private:
		void InitPipeline();
		void RenderLines(const Ref<CommandBuffer>& cmd);
		void UploadVertexBuffer(const Ref<CommandBuffer>& cmd);

	private:
		Ref<PipelineGraphics> m_Pipeline;
		Ref<Buffer> m_VertexBuffer;
		std::vector<RendererDebugVertex> m_Vertices;
		float m_LineWidth = 1.f;
		bool bEnableDebugLinesDepthTest = true;

		static constexpr size_t s_DefaultLinesCount = 256; // How much lines we can render without reallocating
		static constexpr size_t s_DefaultLinesVerticesCount = s_DefaultLinesCount * 2;
		static constexpr size_t s_BaseLinesVertexBufferSize = s_DefaultLinesVerticesCount * sizeof(RendererDebugVertex);
	};
}
