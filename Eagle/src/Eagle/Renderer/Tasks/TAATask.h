#pragma once

#include "RendererTask.h"
#include "Eagle/Renderer/VidWrappers/PipelineCompute.h"

namespace Eagle
{
	class Image;

	class TAATask : public RendererTask
	{
	public:
		TAATask(SceneRenderer& renderer);
		void RecordCommandBuffer(const Ref<CommandBuffer>& cmd) override;

		void OnResize(glm::uvec2 size) override
		{
			m_HistoryImage->Resize(glm::uvec3(size, 1));
			m_Result->Resize(glm::uvec3(size, 1));
		}

	private:
		void InitPipeline();

	private:
		Ref<PipelineCompute> m_Pipeline;
		Ref<Image> m_FinalImage;
		Ref<Image> m_HistoryImage;
		Ref<Image> m_Result;
	};
}
