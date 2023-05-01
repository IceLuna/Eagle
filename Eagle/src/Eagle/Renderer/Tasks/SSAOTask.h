#pragma once

#include "RendererTask.h"
#include "Eagle/Renderer/VidWrappers/PipelineGraphics.h"

namespace Eagle
{
	class SSAOTask : public RendererTask
	{
	public:
		SSAOTask(SceneRenderer& renderer, const SSAOSettings& settings);

		void RecordCommandBuffer(const Ref<CommandBuffer>& cmd) override;
		void OnResize(const glm::uvec2 size) override
		{
			m_SSAOPassImage->Resize(glm::uvec3(size, 1u));
			m_ResultImage->Resize(glm::uvec3(size, 1u));
			m_Pipeline->Resize(size.x, size.y);
			m_BlurPipeline->Resize(size.x, size.y);
		}

		const Ref<Image>& GetResult() const { return m_ResultImage; }
		void InitWithOptions(const SSAOSettings& settings)
		{
			if (m_Samples.size() != settings.GetNumberOfSamples())
				GenerateKernels(settings);
		}

	private:
		void InitPipeline();
		void GenerateKernels(const SSAOSettings& settings);

	private:
		Ref<PipelineGraphics> m_Pipeline;
		Ref<PipelineGraphics> m_BlurPipeline;

		std::vector<glm::vec3> m_Samples;
		Ref<Buffer> m_SamplesBuffer;
		Ref<Image> m_ResultImage;
		Ref<Image> m_SSAOPassImage;
		Ref<Image> m_NoiseImage;

		bool bKernelsDirty = true;
		bool bUploadNoise = true;
	};
}
