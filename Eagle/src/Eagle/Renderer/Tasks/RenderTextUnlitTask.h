#pragma once

#include "RendererTask.h"
#include "Eagle/Renderer/VidWrappers/PipelineGraphics.h"

namespace Eagle
{
	class RenderTextUnlitTask : public RendererTask
	{
	public:
		RenderTextUnlitTask(SceneRenderer& renderer, const Ref<Image>& renderTo);

		void RecordCommandBuffer(const Ref<CommandBuffer>& cmd) override;
		void OnResize(glm::uvec2 size) override { m_Pipeline->Resize(size.x, size.y); }

	private:
		void InitPipeline();

	private:
		Ref<PipelineGraphics> m_Pipeline;
		Ref<Image> m_ResultImage;
	};
}
