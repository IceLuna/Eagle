#pragma once

#include "RendererTask.h"
#include "Eagle/Renderer/VidWrappers/PipelineGraphics.h"
#include "Eagle/Renderer/VidWrappers/PipelineCompute.h"

namespace Eagle
{
	class GTAOTask : public RendererTask
	{
	public:
		GTAOTask(SceneRenderer& renderer);

		void RecordCommandBuffer(const Ref<CommandBuffer>& cmd) override;
		void OnResize(glm::uvec2 size) override
		{
			m_HalfSize = glm::max(size / 2u, glm::uvec2(1u));
			m_HalfTexelSize = 1.f / glm::vec2(m_HalfSize);
			m_HalfNumGroups = { glm::ceil(m_HalfSize.x / float(s_TileSize)), glm::ceil(m_HalfSize.y / float(s_TileSize)) };

			m_HalfDepth->Resize({ m_HalfSize, 1u });
			m_HalfDepthPrev->Resize({ m_HalfSize, 1u });
			m_HalfMotion->Resize({ m_HalfSize, 1u });
			m_Denoised->Resize({ m_HalfSize, 1u });
			m_DenoisedPrev->Resize({ m_HalfSize, 1u });
			m_GTAOPassImage->Resize({ m_HalfSize, 1u });
			m_DownsamplePipeline->Resize(m_HalfSize);
		}

		void InitWithOptions(const SceneRendererSettings& settings) override
		{
			if (m_Samples == settings.GTAOSettings.GetNumberOfSamples())
				return;

			m_Samples = settings.GTAOSettings.GetNumberOfSamples();
			InitPipeline();
		}

		const Ref<Image>& GetResult() const { return m_Denoised; }

	private:
		void InitPipeline();
		void Downsample(const Ref<CommandBuffer>& cmd);
		void GTAO(const Ref<CommandBuffer>& cmd);
		void Denoiser(const Ref<CommandBuffer>& cmd);
		void CopyToPrev(const Ref<CommandBuffer>& cmd);

	private:
		Ref<PipelineGraphics> m_DownsamplePipeline;
		Ref<PipelineCompute> m_GTAOPipeline;
		Ref<PipelineCompute> m_DenoiserPipeline;

		Ref<Image> m_HalfDepth;
		Ref<Image> m_HalfDepthPrev;
		Ref<Image> m_HalfMotion;

		Ref<Image> m_Denoised;
		Ref<Image> m_DenoisedPrev;

		Ref<Image> m_GTAOPassImage;

		glm::uvec2 m_HalfSize = glm::uvec2(1u);
		glm::vec2 m_HalfTexelSize = glm::vec2(0.f);
		glm::uvec2 m_HalfNumGroups = glm::uvec2(1u);

		uint32_t m_Samples = 0;

		constexpr static uint32_t s_TileSize = 8;
	};
}
