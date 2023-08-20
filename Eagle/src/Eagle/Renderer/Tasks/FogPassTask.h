#pragma once

#include "RendererTask.h"
#include "Eagle/Renderer/VidWrappers/PipelineCompute.h"
#include "Eagle/Renderer/RendererUtils.h"

namespace Eagle
{
	class Image;

	class FogPassTask : public RendererTask
	{
	public:
		FogPassTask(SceneRenderer& renderer, const Ref<Image>& renderTo);
		void RecordCommandBuffer(const Ref<CommandBuffer>& cmd) override;

		const Ref<Buffer>& GetFogDataBuffer() const { return m_FogDataBuffer; }

	private:
		void InitPipeline();

		struct FogData
		{
			glm::vec3 FogColor;
			float FogMin;
			float FogMax;
			float Density;
			FogEquation FogEquation;
		} m_FogData;

	private:
		Ref<PipelineCompute> m_Pipeline;
		Ref<Buffer> m_FogDataBuffer;
		Ref<Image> m_Result;
	};
}
