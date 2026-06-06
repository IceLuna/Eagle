#pragma once

#include "RendererTask.h"
#include "GeometryManagerTask.h"
#include "Eagle/Renderer/VidWrappers/PipelineGraphics.h"

namespace Eagle
{
	class RenderTextUnlitTask : public RendererTask
	{
	public:
		RenderTextUnlitTask(SceneRenderer& renderer);

		void RecordCommandBuffer(const Ref<CommandBuffer>& cmd) override;
		void OnResize(glm::uvec2 size) override { m_Pipeline->Resize(size.x, size.y); }

		static void Draw(const Ref<CommandBuffer>& cmd, const Ref<PipelineGraphics>& pipeline, const QuadsRenderData<UnlitTextGeometryData>::BlendModeGeomType& data, const void* pushData, RenderStats& stats);
		static void Draw(const Ref<CommandBuffer>& cmd, const Ref<PipelineGraphics>& pipeline, const UnlitTextGeometryData& data, const void* pushData, RenderStats& stats, const Ref<Framebuffer>& fb);

	private:
		void InitPipeline();

	private:
		Ref<PipelineGraphics> m_Pipeline;
	};
}
