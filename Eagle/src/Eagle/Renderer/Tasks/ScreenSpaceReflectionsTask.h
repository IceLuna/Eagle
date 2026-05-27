#pragma once

#include "RendererTask.h"
#include "Eagle/Renderer/VidWrappers/PipelineCompute.h"

#include <FidelityFX/host/ffx_sssr.h>

namespace Eagle
{
	class Image;
	class Buffer;

	class ScreenSpaceReflectionsTask : public RendererTask
	{
	public:
		ScreenSpaceReflectionsTask(SceneRenderer& renderer);
		~ScreenSpaceReflectionsTask();
		void RecordCommandBuffer(const Ref<CommandBuffer>& cmd) override;
		void OnResize(const glm::uvec2 size) override;

	private:
		void CreateSSSRContext();
		void DestroySSSRContext();

		void InitPipeline();
		void InitResources();

	private:
		// SSSR Context members
		FfxSssrContextDescription m_InitializationParameters = {};
		FfxSssrContext            m_Context;

		ScopedDataBuffer m_ScratchBuffer;
		Ref<PipelineCompute> m_CompositePipeline;
		Ref<PipelineCompute> m_DecodeNormalsPipeline;
		Ref<Image> m_Result;
		Ref<Image> m_Normals;
		glm::uvec2 m_Size = glm::uvec2(0);
	};
}
