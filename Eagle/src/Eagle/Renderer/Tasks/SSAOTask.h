#pragma once

#include "RendererTask.h"
#include "Eagle/Renderer/VidWrappers/PipelineGraphics.h"

namespace Eagle
{
	class SSAOTask : public RendererTask
	{
	public:
		SSAOTask(SceneRenderer& renderer);

		void RecordCommandBuffer(const Ref<CommandBuffer>& cmd) override;
		void OnResize(const glm::uvec2 size) override
		{
			m_SSAOPassImage->Resize(glm::uvec3(size, 1u));
			m_ResultImage->Resize(glm::uvec3(size, 1u));
			m_Pipeline->Resize(size.x, size.y);
			m_BlurPipeline->Resize(size.x, size.y);
		}

		const Ref<Image>& GetResult() const { return m_ResultImage; }
		void InitWithOptions(const SSAOSettings& settings) { GenerateKernels(settings); }

	private:
		void InitPipeline();
		void GenerateKernels(const SSAOSettings& settings);

	private:
		Ref<PipelineGraphics> m_Pipeline;
		Ref<PipelineGraphics> m_BlurPipeline;

		struct hvec2
		{
			uint16_t x;
			uint16_t y;
		};

		glm::detail::hdata a;
		std::vector<glm::vec3> m_Samples;
		std::vector<hvec2> m_Noise; // Rotation around Z. Hence Z = 0.f and we don't need to store it
		Ref<Buffer> m_SamplesBuffer;
		Ref<Image> m_ResultImage;
		Ref<Image> m_SSAOPassImage;
		Ref<Image> m_NoiseImage;

		bool bKernelsDirty = true;
	};
}
