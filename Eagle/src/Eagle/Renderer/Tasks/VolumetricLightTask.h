#pragma once

#include "RendererTask.h"
#include "Eagle/Renderer/VidWrappers/PipelineCompute.h"

namespace Eagle
{
	class Buffer;

	class VolumetricLightTask : public RendererTask
	{
	public:
		VolumetricLightTask(SceneRenderer& renderer, const Ref<Image>& renderTo);

		void RecordCommandBuffer(const Ref<CommandBuffer>& cmd) override;

		virtual void InitWithOptions(const SceneRendererSettings& settings) override
		{
			bool bReloadPipeline = false;
			bool bVolumetricFogChanged = false;
			if (m_VolumetricSettings != settings.VolumetricSettings)
			{
				bReloadPipeline |= m_VolumetricSettings.Samples != settings.VolumetricSettings.Samples;
				bVolumetricFogChanged = m_VolumetricSettings.bFogEnable != settings.VolumetricSettings.bFogEnable;
				bReloadPipeline |= bVolumetricFogChanged;

				m_VolumetricSettings = settings.VolumetricSettings;
				m_Constants.VolumetricSamples = m_VolumetricSettings.Samples;

				if (m_VolumetricSettings.bEnable)
				{
					if (!m_VolumetricsImage)
					{
						ImageSpecifications specs;
						specs.Format = ImageFormat::R16G16B16A16_Float;
						specs.Size = glm::max(m_ResultImage->GetSize() / 2u, glm::uvec3(1u));
						specs.Usage = ImageUsage::ColorAttachment | ImageUsage::Sampled | ImageUsage::Storage;
						m_VolumetricsImage = Image::Create(specs, "PBR_Volumetric");
					}
				}
				else
					m_VolumetricsImage.reset();
			}
			
			const bool bStutterlessChanged = bStutterlessShaders != settings.bStutterlessShaders;
			const bool translucentShadowsChanged = bTranslucentShadows != settings.bTranslucentShadows;
			bReloadPipeline |= bStutterlessChanged;
			bReloadPipeline |= translucentShadowsChanged;
			bTranslucentShadows = settings.bTranslucentShadows;
			bStutterlessShaders = settings.bStutterlessShaders;

			if (bReloadPipeline)
				InitPipeline(bStutterlessChanged, translucentShadowsChanged, bVolumetricFogChanged);
		}

		virtual void OnResize(glm::uvec2 size) override;

	private:
		void InitPipeline(bool bStutterlessChanged, bool translucentShadowsChanged, bool bVolumetricFogChanged);

		struct ConstantData
		{
			uint32_t PointLightsCount = 0;
			uint32_t SpotLightsCount = 0;
			uint32_t bHasDirLight = 0;
			uint32_t VolumetricSamples = 20;

			bool operator== (const ConstantData& other) const
			{
				return PointLightsCount == other.PointLightsCount &&
					SpotLightsCount == other.SpotLightsCount &&
					bHasDirLight == other.bHasDirLight &&
					VolumetricSamples == other.VolumetricSamples;
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

		Ref<Image> m_ResultImage;
		VolumetricLightsSettings m_VolumetricSettings;
		Ref<Image> m_VolumetricsImage; // Volumetric effect is rendered separately into here. Half res
		Ref<Image> m_VolumetricsImageBlurred;
		
		bool bStutterlessShaders = false;
		bool bTranslucentShadows = false;
	};
}
