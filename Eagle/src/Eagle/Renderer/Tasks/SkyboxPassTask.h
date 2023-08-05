#pragma once

#include "RendererTask.h"
#include "Eagle/Renderer/VidWrappers/PipelineGraphics.h"

namespace Eagle
{
	class Image;

	class SkyboxPassTask : public RendererTask
	{
	public:
		SkyboxPassTask(SceneRenderer& renderer, const Ref<Image>& renderTo);

		void RecordCommandBuffer(const Ref<CommandBuffer>& cmd) override;
		void OnResize(glm::uvec2 size) override
		{
			m_IBLPipeline->Resize(size.x, size.y);
			m_SkyPipeline->Resize(size.x, size.y);
		}

	private:
		void InitPipeline();
		void ReloadSkyPipeline();

	private:
		Ref<PipelineGraphics> m_IBLPipeline;
		Ref<PipelineGraphics> m_SkyPipeline;
		Ref<Image> m_FinalImage;

		struct Clouds
		{
			uint32_t bCirrus = 0;
			uint32_t bCumulus = 0;
			uint32_t CumulusLayers = 3u;

			bool operator== (const Clouds& other) const
			{
				return bCirrus == other.bCirrus &&
					bCumulus == other.bCumulus &&
					CumulusLayers == other.CumulusLayers;
			}

			bool operator!= (const Clouds& other) const { return !(*this == other); }
		} m_Clouds;
	};
}
