#pragma once

#include "RendererTask.h"
#include "Eagle/Renderer/VidWrappers/PipelineGraphics.h"

namespace Eagle
{
	class RenderTextLitTask : public RendererTask
	{
	public:
		RenderTextLitTask(SceneRenderer& renderer);

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

	private:
		void InitPipeline();

	private:
		Ref<PipelineGraphics> m_Pipeline;

		bool bMotionRequired = false;
		bool bJitter = false;
	};
}
