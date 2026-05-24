#pragma once

#include "RendererTask.h"
#include "Eagle/Renderer/VidWrappers/PipelineCompute.h"

namespace Eagle
{
	class Image;

	class FXAATask : public RendererTask
	{
	public:
		FXAATask(SceneRenderer& renderer);
		void RecordCommandBuffer(const Ref<CommandBuffer>& cmd) override;

	private:
		Ref<PipelineCompute> m_Pipeline;

		Ref<Image> m_Image;
	};
}
