#pragma once

#include "RendererTask.h"
#include "Eagle/Renderer/VidWrappers/PipelineGraphics.h"

namespace Eagle
{
	class RenderMeshesTask : public RendererTask
	{
	public:
		RenderMeshesTask(SceneRenderer& renderer);

		void RecordCommandBuffer(const Ref<CommandBuffer>& cmd) override;
		void OnResize(glm::uvec2 size) override { m_Pipeline->Resize(size.x, size.y); }

		void InitWithOptions(const SceneRendererSettings& settings) override
		{
			if (bMotionRequired == settings.InternalState.bMotionBuffer &&
				bJitter == settings.InternalState.bJitter)
				return;

			bMotionRequired = settings.InternalState.bMotionBuffer;
			bJitter = settings.InternalState.bJitter;

			InitPipeline();
		}

		inline static const std::vector<PipelineGraphicsState::VertexInputAttribute> PerInstanceAttribs = { { 4u } }; // Locations of Per-Instance data in shader

	private:
		void InitPipeline();
		void Render(const Ref<CommandBuffer>& cmd);

	private:
		Ref<PipelineGraphics> m_Pipeline;

		uint64_t m_TexturesUpdatedFrame = 0;
		bool bMotionRequired = false;
		bool bJitter = false;
	};
}
