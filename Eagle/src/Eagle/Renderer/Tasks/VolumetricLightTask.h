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
		void InitPipeline();
		ShaderDefines GetShaderDefines() const;

		uint32_t GetDownscaleFactor() const;
		glm::uvec2 GetResourceSize(glm::uvec2 fullResSize) const;

		void CreateFogNoise(const Ref<CommandBuffer>& cmd);
		void UpdateFogNoiseOffsets(float deltaTime);
		float GetFogScale() const;

		struct ConstantData
		{
			uint32_t VolumetricSamples = 20;
		} m_Constants;

	private:
		Ref<PipelineCompute> m_Pipeline;
		Ref<PipelineCompute> m_CompositePipeline;
		Ref<PipelineCompute> m_BlurPipeline;
		Ref<PipelineCompute> m_PrepareArgsPipeline;

		VolumetricLightsSettings m_VolumetricSettings;
		Ref<Image> m_VolumetricsImage; // Volumetric effect is rendered separately into here. rgb - in-scattered light, a - transmittance
		Ref<Image> m_VolumetricsImageBlurred;
		Ref<Image> m_VolumetricsDepthImage; // Linear depth that each pixel of `m_VolumetricsImage` was traced to
		Ref<Image> m_FogNoise;
		Ref<Buffer> m_IndirectArgs;

		glm::vec3 m_FogNoiseOffsets[2] = { glm::vec3(0.f), glm::vec3(0.f) }; // Displacements of the base & detail layers of the fog noise. In tiles of each layer, wrapped to [0; 1)
		bool bTranslucentShadows = false;
	};
}
