#pragma once

#include "RendererTask.h"
#include "Eagle/Renderer/VidWrappers/PipelineCompute.h"

namespace Eagle
{
	class SSAOTask : public RendererTask
	{
	public:
		SSAOTask(SceneRenderer& renderer);

		void RecordCommandBuffer(const Ref<CommandBuffer>& cmd) override;
		void OnResize(glm::uvec2 size) override
		{
			m_SSAOPassImage->Resize(glm::uvec3(size, 1u));
			m_ResultImage->Resize(glm::uvec3(size, 1u));
		}

		const Ref<Image>& GetResult() const { return m_ResultImage; }
		void InitWithOptions(const SceneRendererSettings& settings) override
		{
			const SSAOSettings& ssaoSettings = settings.SSAOSettings;
			if (m_Samples.size() != ssaoSettings.GetNumberOfSamples())
			{
				GenerateKernels(ssaoSettings);
				InitPipeline(ssaoSettings.GetNumberOfSamples());
			}
		}

	private:
		void InitPipeline(uint32_t samples);
		void GenerateKernels(const SSAOSettings& settings);

	private:
		Ref<PipelineCompute> m_Pipeline;
		Ref<PipelineCompute> m_BlurPipeline;

		std::vector<glm::vec3> m_Samples;
		Ref<Buffer> m_SamplesBuffer;
		Ref<Image> m_ResultImage;
		Ref<Image> m_SSAOPassImage;
		Ref<Image> m_NoiseImage;

		bool bKernelsDirty = true;
		bool bUploadNoise = true;
	};
}
