#pragma once

#include "RendererTask.h"
#include "Eagle/Renderer/VidWrappers/PipelineGraphics.h"

namespace Eagle
{
	class Image;

	class PostprocessingPassTask : public RendererTask
	{
	public:
		PostprocessingPassTask(SceneRenderer& renderer, const Ref<Image>& input, const Ref<Image>& output);

		void RecordCommandBuffer(const Ref<CommandBuffer>& cmd) override;
		void OnResize(const glm::uvec2 size) override { m_Pipeline->Resize(size.x, size.y); }

		void InitWithOptions(const SceneRendererSettings& settings) override;

	private:
		void InitPipeline();

	private:
		Ref<PipelineGraphics> m_Pipeline;
		Ref<Image> m_Input;
		Ref<Image> m_Output;
	};
}
