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
			if (m_VolumetricSettings != settings.VolumetricSettings)
			{
				bReloadPipeline |= m_VolumetricSettings.Samples != settings.VolumetricSettings.Samples;

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
			bReloadPipeline |= bStutterlessChanged;
			bStutterlessShaders = settings.bStutterlessShaders;

			if (bReloadPipeline)
				InitPipeline(bStutterlessChanged);
		}

		virtual void OnResize(glm::uvec2 size) override
		{
			const glm::uvec2 halfSize = glm::max(size / 2u, glm::uvec2(1u));
			m_VolumetricsImage->Resize(glm::uvec3(halfSize, 1u));
		}

	private:
		void InitPipeline(bool bStutterlessChanged);

		struct ConstantData
		{
			PBRConstantsKernelInfo PBRInfo;
			uint32_t VolumetricSamples;
		} m_Constants;

	private:
		Ref<PipelineCompute> m_Pipeline;
		Ref<PipelineCompute> m_CompositePipeline;

		Ref<Image> m_ResultImage;
		VolumetricLightsSettings m_VolumetricSettings;
		Ref<Image> m_VolumetricsImage; // Volumetric effect is rendered separately into here. Half res
		
		bool bStutterlessShaders = false;
	};
}
