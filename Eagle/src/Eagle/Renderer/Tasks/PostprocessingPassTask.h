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
		PostprocessingPassTask(SceneRenderer& renderer);
		void RecordCommandBuffer(const Ref<CommandBuffer>& cmd) override;

		void InitWithOptions(const SceneRendererSettings&) override;
		void OnResize(const glm::uvec2 size) override;

	private:
		void InitTonemappingPipeline();
		void InitAutoexposureResources();
		void InitLensPipeline();

		void AutoExposurePass(const Ref<CommandBuffer>& cmd);
		void TonemappingPass(const Ref<CommandBuffer>& cmd, const Ref<Image>& output);
		void LensPass(const Ref<CommandBuffer>& cmd, const Ref<Image>& input, const Ref<Image>& output);

		bool ShouldUseLens() const { return bChromaticAberration || bVignette || bFilmGrain; }

		uint32_t CalcGrainSeed(Timestep deltaTime, float seedUpdateRate);

	private:
		Ref<PipelineCompute> m_TonemappingPipeline;
		
		Ref<PipelineCompute> m_HistogramPipeline;
		Ref<PipelineCompute> m_AveragePipeline;
		Ref<Buffer> m_Histogram;
		Ref<Buffer> m_Exposure;

		Ref<Image> m_Intermediate;

		Ref<PipelineCompute> m_LensPipeline;

		TonemappingMethod m_Tonemapping = TonemappingMethod::AgX;
		bool bAutoExposure = false;
		bool bChromaticAberration = false;
		bool bVignette = false;
		bool bFilmGrain = false;

		uint32_t m_FilmSeed = 0;
		float m_SeedTimer = 0.0f;

		static constexpr uint32_t s_TileSize = 16u;
	};
}
