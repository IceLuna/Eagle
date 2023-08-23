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
		void OnResize(glm::uvec2 size) override
		{
			m_OpaquePipeline->Resize(size.x, size.y);
			m_MaskedPipeline->Resize(size.x, size.y);
		}

		void InitWithOptions(const SceneRendererSettings& settings) override
		{
			if (bMotionRequired == settings.InternalState.bMotionBuffer &&
				bJitter == settings.InternalState.bJitter)
				return;

			bMotionRequired = settings.InternalState.bMotionBuffer;
			bJitter = settings.InternalState.bJitter;
			InitPipeline();
		}

		void InitPipeline();

	private:
		void RenderOpaque(const Ref<CommandBuffer>& cmd);
		void RenderMasked(const Ref<CommandBuffer>& cmd);

	private:
		Ref<PipelineGraphics> m_OpaquePipeline;
		Ref<PipelineGraphics> m_MaskedPipeline;

		uint64_t m_OpaqueTexturesUpdatedFrames[RendererConfig::FramesInFlight] = { 0 };
		uint64_t m_MaskedTexturesUpdatedFrames[RendererConfig::FramesInFlight] = { 0 };
		bool bMotionRequired = false;
		bool bJitter = false;
	};
}
