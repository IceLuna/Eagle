#pragma once

#include "RendererTask.h"
#include "Eagle/Renderer/VidWrappers/PipelineGraphics.h"

namespace Eagle
{
	class Image;

	class GridTask : public RendererTask
	{
	public:
		GridTask(SceneRenderer& renderer, const Ref<Image>& output);
		void RecordCommandBuffer(const Ref<CommandBuffer>& cmd) override;
		void InitWithOptions(const SceneRendererSettings& settings) override
		{
			if (settings.InternalState.bJitter == bJitter)
				return;

			bJitter = settings.InternalState.bJitter;
			InitPipeline();
		}

		void OnResize(const glm::uvec2 size) override { m_Pipeline->Resize(size); }

	private:
		void InitPipeline();

	private:
		Ref<PipelineGraphics> m_Pipeline;
		Ref<Image> m_Output;
		bool bJitter = false;
	};
}
