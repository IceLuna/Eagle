#pragma once

#include "RendererTask.h"
#include "Eagle/Renderer/VidWrappers/PipelineGraphics.h"

namespace Eagle
{
	class DepthPrepassTask : public RendererTask
	{
	public:
		DepthPrepassTask(SceneRenderer& renderer);

		void RecordCommandBuffer(const Ref<CommandBuffer>& cmd) override;
		void OnResize(glm::uvec2 size) override
		{
			m_SpritesPipeline->Resize(size.x, size.y);
			m_StaticMeshesPipeline->Resize(size.x, size.y);
			m_SkeletalMeshesPipeline->Resize(size.x, size.y);
		}

		void InitWithOptions(const SceneRendererSettings& settings) override
		{
			if (bJitter == settings.InternalState.bJitter)
				return;

			bJitter = settings.InternalState.bJitter;
			InitPipelines();
		}

	private:
		void RenderSprites(const Ref<CommandBuffer>& cmd);
		void RenderStaticMeshes(const Ref<CommandBuffer>& cmd);
		void RenderSkeletalMeshes(const Ref<CommandBuffer>& cmd);

		void InitSpritesPipeline();
		void InitStaticPipeline();
		void InitSkeletalPipeline();

		void InitPipelines()
		{
			InitSpritesPipeline();
			InitStaticPipeline();
			InitSkeletalPipeline();
		}

	private:
		Ref<PipelineGraphics> m_SpritesPipeline;
		Ref<PipelineGraphics> m_StaticMeshesPipeline;
		Ref<PipelineGraphics> m_SkeletalMeshesPipeline;
		bool bJitter = false;
	};
}
