#pragma once

#include "RendererTask.h"
#include "Eagle/Renderer/VidWrappers/PipelineCompute.h"

namespace Eagle
{
	class HZBTask : public RendererTask
	{
	public:
        HZBTask(SceneRenderer& renderer);

		void RecordCommandBuffer(const Ref<CommandBuffer>& cmd) override;
		void OnResize(const glm::uvec2 size) override;

	private:
        Ref<PipelineCompute> m_Pipeline;
        Ref<Buffer> m_TempDepthCopy;
        std::vector<ImageView> m_HZBMipViews;
	};
}
