#pragma once

#include "RendererTask.h"
#include "Eagle/Renderer/VidWrappers/PipelineGraphics.h"

struct CPUMaterial;

namespace Eagle
{
	struct SpriteGeometryData;

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

		struct PushData
		{
			glm::mat4 ViewProj;
			glm::mat4 PrevViewProj;
		};
		static void Draw(const Ref<CommandBuffer>& cmd, const Ref<PipelineGraphics>& pipeline, const SpriteGeometryData& spritesData, const PushData& pushData, RenderStats& stats);

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
