#pragma once

#include "RendererTask.h"
#include "Eagle/Renderer/VidWrappers/PipelineCompute.h"

namespace Eagle
{
	class Buffer;

	class VolumetricLightTask : public RendererTask
	{
	public:
		VolumetricLightTask(SceneRenderer& renderer);

		void RecordCommandBuffer(const Ref<CommandBuffer>& cmd) override;
		void InitWithOptions(const SceneRendererSettings& settings) override;
		void OnResize(glm::uvec2 size) override;

	private:
		void InitPipeline(bool translucentShadowsChanged, bool bVolumetricFogChanged);

		struct ConstantData
		{
			uint32_t VolumetricSamples = 20;

			bool operator== (const ConstantData& other) const
			{
				return VolumetricSamples == other.VolumetricSamples;
			}

			bool operator!= (const ConstantData& other) const
			{
				return !((*this) == other);
			}

		} m_Constants;

	private:
		Ref<PipelineCompute> m_Pipeline;
		Ref<PipelineCompute> m_CompositePipeline;
		Ref<PipelineCompute> m_GuassianPipeline;

		VolumetricLightsSettings m_VolumetricSettings;
		Ref<Image> m_VolumetricsImage; // Volumetric effect is rendered separately into here. Half res
		Ref<Image> m_VolumetricsImageBlurred;
		
		float m_Time = 0.0;
		bool bTranslucentShadows = false;
	};
}
