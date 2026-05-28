#pragma once

#include "RendererTask.h"
#include "GeometryManagerTask.h"
#include "Eagle/Renderer/VidWrappers/PipelineGraphics.h"

namespace Eagle
{
	struct LitTextGeometryData;

	class RenderTextLitTask : public RendererTask
	{
	public:
		RenderTextLitTask(SceneRenderer& renderer);

		void RecordCommandBuffer(const Ref<CommandBuffer>& cmd) override;
		void OnResize(glm::uvec2 size) override
		{
			m_OpaquePipeline->Resize(size.x, size.y);
			m_MaskedPipeline->Resize(size.x, size.y);
		}

		void InitWithOptions(const SceneRendererSettings& settings) override
		{
			if (bMotionRequired == settings.InternalState.bMotionBuffer &&
				bJitter == settings.InternalState.bJitter &&
				bGeometricSpecularAA == settings.bGeometricSpecularAA)
				return;

			bMotionRequired = settings.InternalState.bMotionBuffer;
			bJitter = settings.InternalState.bJitter;
			bGeometricSpecularAA = settings.bGeometricSpecularAA;

			InitPipeline();
		}

		static void Draw(const Ref<CommandBuffer>& cmd, const Ref<PipelineGraphics>& pipeline, const LitTextGeometryData& data, const void* vertexPushData, RenderStats& stats, const Ref<Framebuffer>& fb);
		static void Draw(const Ref<CommandBuffer>& cmd, const Ref<PipelineGraphics>& pipeline, const QuadsRenderData<LitTextGeometryData>::BlendModeGeomType& data, const void* vertexPushData, RenderStats& stats);

	private:
		void InitPipeline();
		void RenderOpaque(const Ref<CommandBuffer>& cmd);
		void RenderMasked(const Ref<CommandBuffer>& cmd);

	private:
		Ref<PipelineGraphics> m_OpaquePipeline;
		Ref<PipelineGraphics> m_MaskedPipeline;

		uint64_t m_OpaqueTexturesUpdatedFrames[RendererConfig::FramesInFlight] = { 0 };
		uint64_t m_MaskedTexturesUpdatedFrames[RendererConfig::FramesInFlight] = { 0 };

		bool bMotionRequired = false;
		bool bJitter = false;
		bool bGeometricSpecularAA = true;
	};
}
