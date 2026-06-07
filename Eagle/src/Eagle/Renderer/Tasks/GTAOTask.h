#pragma once

#include "RendererTask.h"
#include "Eagle/Renderer/VidWrappers/PipelineGraphics.h"
#include "Eagle/Renderer/VidWrappers/PipelineCompute.h"
#include "Eagle/Renderer/VidWrappers/Texture.h"

namespace Eagle
{
	class GTAOTask : public RendererTask
	{
	public:
		GTAOTask(SceneRenderer& renderer);

		void RecordCommandBuffer(const Ref<CommandBuffer>& cmd) override;
		void OnResize(glm::uvec2 size) override;

		void InitWithOptions(const SceneRendererSettings& settings) override
		{
			if (m_Quality == settings.GTAOSettings.Quality &&
				bGenerateBentNormals == settings.GTAOSettings.bGenerateBentNormals &&
				bHalfRes == settings.GTAOSettings.bHalfRes)
				return;

			m_Quality = settings.GTAOSettings.Quality;
			bGenerateBentNormals = settings.GTAOSettings.bGenerateBentNormals;
			bHalfRes = settings.GTAOSettings.bHalfRes;

			InitResources();
			InitPipeline();
		}

		const Ref<Image>& GetResult() const { return m_Denoised; }
		const Ref<Image>& GetBentNormals() const { return m_GTAOBentNormalsPassImage; }

	private:
		void InitResources();
		void InitPipeline();

		void Downsample(const Ref<CommandBuffer>& cmd);
		void GTAO(const Ref<CommandBuffer>& cmd);
		void Denoiser(const Ref<CommandBuffer>& cmd);
		void Interleave(const Ref<CommandBuffer>& cmd);

	private:
		struct GTAOConstants
		{
			glm::ivec2 ViewportSize;
			glm::vec2  ViewportPixelSize;                  // .zw == 1.0 / ViewportSize.xy

			glm::vec2  CameraPlanes; // Near; Far
			glm::vec2  CameraTanHalfFOV;

			glm::vec2  NDCToViewMul;
			glm::vec2  NDCToViewAdd;

			glm::vec2  NDCToViewMul_x_PixelSize;
			float      EffectRadius;                       // world (viewspace) maximum size of the shadow
			float      EffectFalloffRange;

			float      RadiusMultiplier;
			float      FinalValuePower;
			float      DenoiseBlurBeta;
			uint32_t   FinalPass;

			float      SampleDistributionPower;
			float      ThinOccluderCompensation;
			float      DepthMIPSamplingOffset;
			int        NoiseIndex;                         // frameIndex % 64 if using TAA or 0 otherwise
		} m_Constants;

		Ref<PipelineGraphics> m_DownsamplePipeline;
		Ref<PipelineCompute> m_GTAOPipeline;
		Ref<PipelineCompute> m_DenoiserPipeline;
		Ref<PipelineCompute> m_InterleavePipeline;

		Ref<Image> m_Depth;

		Ref<Image> m_Denoised;

		Ref<Image> m_GTAOPassImage[2];
		Ref<Image> m_GTAOBentNormalsPassImage;
		Ref<Image> m_GTAOEdgesImage;
		
		glm::uvec2 m_PassSize = glm::uvec2(1u);
		uint32_t m_PingPong = 0;

		struct QualityParams
		{
			uint32_t Samples = 1;
			uint32_t StepsPerSample = 1;

			QualityParams& operator=(const GTAOSettings::QualityParams& params)
			{
				Samples = params.NumberOfSamples;
				StepsPerSample = params.StepsPerSample;

				return *this;
			}

			bool operator==(const GTAOSettings::QualityParams& params)
			{
				return Samples == params.NumberOfSamples &&
					StepsPerSample == params.StepsPerSample;
			}
		};
		QualityParams m_Quality;
		bool bGenerateBentNormals = false;
		bool bHalfRes = false;

		constexpr static uint32_t s_TileSize = 8;
	};
}
