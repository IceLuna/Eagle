#pragma once

#include "RendererTask.h"
#include "Eagle/Renderer/VidWrappers/PipelineCompute.h"

namespace Eagle
{
	class Image;
	class Sampler;

	class ScreenSpaceShadowsTask : public RendererTask
	{
	public:
		ScreenSpaceShadowsTask(SceneRenderer& renderer);
		void RecordCommandBuffer(const Ref<CommandBuffer>& cmd) override;
		void InitWithOptions(const SceneRendererSettings&) override;

		void OnResize(const glm::uvec2 size) override;

		const Ref<Image>& GetResult() const { return m_ShadowMap; }

	private:
		void InitPipeline();

	private:
		Ref<PipelineCompute> m_Pipeline;
		Ref<Image> m_ShadowMap;
		Ref<Sampler> m_Sampler;

		struct Settings
		{
            uint32_t Samples = 60;
            uint32_t HardShadowSamples = 4;
            uint32_t FadeOutSamples = 8;
            float SurfaceThickness = 0.01f;
            float BilinearThreshold = 0.04f;
            float ShadowContrast = 4.0f;
            uint32_t bIgnoreEdgePixels = 0;
            uint32_t bUsePrecisionOffset = 0;
            uint32_t bBilinearSamplingOffsetMode = 0;
            uint32_t bUseEarlyOut = 1;
            uint32_t bDebugOutputEdgeMask = 0;

            Settings& operator= (const ScreenSpaceShadowsSettings& other)
            {
                Samples = other.Samples;
                HardShadowSamples = other.HardShadowSamples;
                FadeOutSamples = other.FadeOutSamples;
                SurfaceThickness = other.SurfaceThickness;
                BilinearThreshold = other.BilinearThreshold;
                ShadowContrast = other.ShadowContrast;
                bIgnoreEdgePixels = (other.bIgnoreEdgePixels ? 1u : 0u);
                bUsePrecisionOffset = (other.bUsePrecisionOffset ? 1u : 0u);
                bBilinearSamplingOffsetMode = (other.bBilinearSamplingOffsetMode ? 1u : 0u);
                bUseEarlyOut = (other.bUseEarlyOut ? 1u : 0u);
                bDebugOutputEdgeMask = (other.bDebugOutputEdgeMask ? 1u : 0u);
                return *this;
            }

            bool operator== (const ScreenSpaceShadowsSettings& other) const
            {
                return Samples == other.Samples &&
                    HardShadowSamples == other.HardShadowSamples &&
                    FadeOutSamples == other.FadeOutSamples &&
                    SurfaceThickness == other.SurfaceThickness &&
                    BilinearThreshold == other.BilinearThreshold &&
                    ShadowContrast == other.ShadowContrast &&
                    bIgnoreEdgePixels == (other.bIgnoreEdgePixels ? 1u : 0u) &&
                    bUsePrecisionOffset == (other.bUsePrecisionOffset ? 1u : 0u) &&
                    bBilinearSamplingOffsetMode == (other.bBilinearSamplingOffsetMode ? 1u : 0u) &&
                    bUseEarlyOut == (other.bUseEarlyOut ? 1u : 0u) &&
                    bDebugOutputEdgeMask == (other.bDebugOutputEdgeMask ? 1u : 0u);
            }

            bool operator!= (const ScreenSpaceShadowsSettings& other) const
            {
                return !((*this) == other);
            }
		};
        Settings m_Settings;
	};
}
