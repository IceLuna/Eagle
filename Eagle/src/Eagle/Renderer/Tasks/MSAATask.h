#pragma once

#include "RendererTask.h"
#include "Eagle/Renderer/VidWrappers/PipelineCompute.h"
#include "Eagle/Renderer/VidWrappers/PipelineGraphics.h"

namespace Eagle
{
	class Image;

	class MSAATask : public RendererTask
	{
	public:
		MSAATask(SceneRenderer& renderer);

		void RecordCommandBuffer(const Ref<CommandBuffer>& cmd) override;
		void OnResize(glm::uvec2 size) override;

		void InitWithOptions(const SceneRendererSettings& settings) override
		{
			if (bJitter == settings.InternalState.bJitter && m_Samples == settings.MSAAParams.Samples)
				return;

			const bool bMSAAChanged = m_Samples != settings.MSAAParams.Samples;
			m_Samples = settings.MSAAParams.Samples;
			bJitter = settings.InternalState.bJitter;
			if (bMSAAChanged)
				CreateMSAATextures();
			InitPipelines();
		}

	private:
		void RenderSprites(const Ref<CommandBuffer>& cmd);
		void RenderStaticMeshes(const Ref<CommandBuffer>& cmd);
		void RenderSkeletalMeshes(const Ref<CommandBuffer>& cmd);

		void CreateMSAATextures();

		void InitSpritesPipeline();
		void InitStaticPipeline();
		void InitSkeletalPipeline();
		void InitApplyPipeline();

		void InitPipelines();

	private:
		Ref<Shader> m_MeshOpaqueDrawShader;
		Ref<Shader> m_MeshMaskedDrawShader;

		Ref<PipelineGraphics> m_SpritesPipeline;
		Ref<PipelineGraphics> m_MaskedSpritesPipeline;
		uint64_t m_SpritesTexturesUpdatedFrames[RendererConfig::FramesInFlight] = { 0 };

		Ref<PipelineGraphics> m_StaticMeshesPipeline;
		Ref<PipelineGraphics> m_MaskedStaticMeshesPipeline;
		uint64_t m_MeshesTexturesUpdatedFrames[RendererConfig::FramesInFlight] = { 0 };

		Ref<PipelineGraphics> m_SkeletalMeshesPipeline;
		Ref<PipelineGraphics> m_MaskedSkeletalMeshesPipeline;
		uint64_t m_SkeletalMeshesTexturesUpdatedFrames[RendererConfig::FramesInFlight] = { 0 };

		Ref<PipelineCompute> m_ApplyMSAAPipeline;

		Ref<Image> m_MSAADepth;
		Ref<Image> m_MSAANormals;

		MSAASamples m_Samples;
		bool bJitter = false;
	};
}
