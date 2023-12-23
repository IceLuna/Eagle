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
		void OnResize(const glm::uvec2 size) { m_Pipeline->Resize(size.x, size.y); }
		void InitWithOptions(const SceneRendererSettings& settings) override
		{
			if (settings.LineWidth == m_LineWidth && settings.InternalState.bJitter == bJitter)
				return;

			m_LineWidth = settings.LineWidth;
			bJitter = settings.InternalState.bJitter;

			InitPipeline();
		}

		void SetDebugLines(const std::vector<RendererLine>& lines);

		struct LineVertex
		{
			glm::vec3 Color = glm::vec3{ 0.f, 0.f, 0.f };
			glm::vec3 Position = glm::vec3{ 0.f };
		};

	private:
		void InitPipeline();
		void RenderLines(const Ref<CommandBuffer>& cmd);
		void UploadVertexBuffer(const Ref<CommandBuffer>& cmd);

	private:
		Ref<PipelineGraphics> m_Pipeline;
		Ref<Buffer> m_VertexBuffer;
		std::vector<LineVertex> m_Vertices;
		float m_LineWidth = 1.f;
		bool bJitter = false;

		static constexpr size_t s_DefaultLinesCount = 256; // How much lines we can render without reallocating
		static constexpr size_t s_DefaultLinesVerticesCount = s_DefaultLinesCount * 2;
		static constexpr size_t s_BaseLinesVertexBufferSize = s_DefaultLinesVerticesCount * sizeof(LineVertex);
	};
}
