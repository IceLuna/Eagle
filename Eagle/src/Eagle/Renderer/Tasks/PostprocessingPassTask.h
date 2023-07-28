#pragma once

#include "RendererTask.h"
#include "Eagle/Renderer/VidWrappers/PipelineCompute.h"

namespace Eagle
{
	class Image;

	class PostprocessingPassTask : public RendererTask
	{
	public:
		PostprocessingPassTask(SceneRenderer& renderer, const Ref<Image>& input, const Ref<Image>& output);

		void RecordCommandBuffer(const Ref<CommandBuffer>& cmd) override;

		void InitWithOptions(const SceneRendererSettings& settings) override;

	private:
		void InitPipeline();

	private:
		Ref<PipelineCompute> m_Pipeline;
		Ref<Image> m_Input;
		Ref<Image> m_Output;
		bool bFog = false;
	};
}
