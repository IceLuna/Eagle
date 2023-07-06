#pragma once

#include "RendererTask.h"
#include "Eagle/Renderer/VidWrappers/PipelineGraphics.h"

struct CPUMaterial;

namespace Eagle
{
	class RenderSpritesTask : public RendererTask
	{
	public:
		RenderSpritesTask(SceneRenderer& renderer);

		void RecordCommandBuffer(const Ref<CommandBuffer>& cmd) override;
		void OnResize(glm::uvec2 size) override { m_Pipeline->Resize(size.x, size.y); }

		void InitWithOptions(const SceneRendererSettings& settings) override
		{
			if (bMotionRequired == settings.OptionalGBuffers.bMotion)
				return;

			bMotionRequired = settings.OptionalGBuffers.bMotion;
			InitPipeline();
		}

		void InitPipeline();

	private:
		Ref<PipelineGraphics> m_Pipeline;
		uint64_t m_TexturesUpdatedFrame = 0;
		bool bMotionRequired = false;
	};
}
