#pragma once

#include "RendererTask.h"

#include "Eagle/Renderer/VidWrappers/PipelineGraphics.h"

namespace Eagle
{
	class Buffer;

	class RenderTrianglesTask : public RendererTask
	{
	public:
		RenderTrianglesTask(SceneRenderer& renderer);

		void RecordCommandBuffer(const Ref<CommandBuffer>& cmd) override;
		void OnResize(const glm::uvec2 size) override { m_Pipeline->Resize(size.x, size.y); }
		void InitWithOptions(const SceneRendererSettings& settings) override
		{
			if (settings.InternalState.bJitter == bJitter)
				return;

			bJitter = settings.InternalState.bJitter;

			InitPipeline();
		}

		void SetDebugTriangles(const std::vector<RendererTriangle>& lines);

	private:
		void InitPipeline();
		void RenderTriangles(const Ref<CommandBuffer>& cmd);
		void UploadVertexBuffer(const Ref<CommandBuffer>& cmd);

	private:
		Ref<PipelineGraphics> m_Pipeline;
		Ref<Buffer> m_VertexBuffer;
		std::vector<RendererDebugVertex> m_Vertices;
		bool bJitter = false;

		static constexpr size_t s_DefaultTrianglesCount = 256; // How much triangles we can render without reallocating
		static constexpr size_t s_DefaultTrianglesVerticesCount = s_DefaultTrianglesCount * 3;
		static constexpr size_t s_BaseLinesVertexBufferSize = s_DefaultTrianglesVerticesCount * sizeof(RendererDebugVertex);
	};
}
