#pragma once

#include "RendererTask.h"

#include "Eagle/Renderer/VidWrappers/PipelineGraphics.h"

namespace Eagle
{
	class Buffer;

	class RenderLines2DTask : public RendererTask
	{
	public:
		RenderLines2DTask(SceneRenderer& renderer);

		void RecordCommandBuffer(const Ref<CommandBuffer>& cmd) override;
		void OnResize(const glm::uvec2 size) override { m_Pipeline->Resize(size.x, size.y); }
		void InitWithOptions(const SceneRendererSettings& settings) override
		{
			if (settings.LineWidth == m_LineWidth)
				return;

			m_LineWidth = settings.LineWidth;
			InitPipeline();
		}

		void SetLines(const std::vector<RendererLine2D>& lines);

	private:
		void InitPipeline();

	private:
		Ref<PipelineGraphics> m_Pipeline;
		Ref<Buffer> m_VertexBuffer;
		std::vector<RendererDebugVertex2D> m_Vertices;
		float m_LineWidth = 1.f;

		static constexpr size_t s_BaseVertexBufferSize = 256 * 2 * sizeof(RendererDebugVertex2D);
	};
}
