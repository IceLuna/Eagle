#pragma once

#include "RendererTask.h"

namespace Eagle
{
	class Image;
	class Sampler;
	class PipelineCompute;
	struct ImageView;

	class BloomPassTask : public RendererTask
	{
	public:
		BloomPassTask(SceneRenderer& renderer, const Ref<Image>& input);

		void RecordCommandBuffer(const Ref<CommandBuffer>& cmd) override;
		void OnResize(const glm::uvec2 size) override;

	private:
		void InitPipeline();

	private:
		Ref<PipelineCompute> m_DownscalePipeline;
		Ref<PipelineCompute> m_UpscalePipeline;
		Ref<Image> m_InputImage;
		Ref<Sampler> m_BloomSampler;
		Ref<Sampler> m_DirtSampler;
		std::vector<ImageView> m_MipViews;
	};
}
