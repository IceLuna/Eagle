#pragma once

#include "RendererTask.h"
#include "Eagle/Renderer/VidWrappers/PipelineCompute.h"

namespace Eagle
{
	class Image;
	class Buffer;

	class PostprocessingPassTask : public RendererTask
	{
	public:
		PostprocessingPassTask(SceneRenderer& renderer, const Ref<Image>& input, const Ref<Image>& output);
		void RecordCommandBuffer(const Ref<CommandBuffer>& cmd) override;

		void InitWithOptions(const SceneRendererSettings&) override;

	private:
		void InitPipeline();
		void InitAutoexposureResources();

		void AutoExposurePass(const Ref<CommandBuffer>& cmd);
		void ApplyPass(const Ref<CommandBuffer>& cmd);

	private:
		Ref<PipelineCompute> m_Pipeline;
		Ref<Image> m_Input;
		Ref<Image> m_Output;
		
		Ref<PipelineCompute> m_HistogramPipeline;
		Ref<PipelineCompute> m_AveragePipeline;
		Ref<Buffer> m_Histogram;
		Ref<Buffer> m_Exposure;

		bool bAutoExposure = false;

		static constexpr uint32_t s_TileSize = 16u;
	};
}
