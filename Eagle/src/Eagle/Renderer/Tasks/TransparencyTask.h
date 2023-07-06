#pragma once

#include "RendererTask.h"
#include "Eagle/Renderer/VidWrappers/PipelineGraphics.h"

namespace Eagle
{

	class TransparencyTask : public RendererTask
	{
	public:
		TransparencyTask(SceneRenderer& renderer);

		void RecordCommandBuffer(const Ref<CommandBuffer>& cmd) override;
		void OnResize(glm::uvec2 size) override
		{
			constexpr size_t formatSize = GetImageFormatBPP(ImageFormat::R32_UInt) / 8u;
			const size_t bufferSize = size_t(size.x * size.y * m_Layers) * 2ull * formatSize;
			m_OITBuffer->Resize(bufferSize);

			m_CompositePipeline->Resize(size);
			m_MeshesDepthPipeline->Resize(size);
			m_MeshesColorPipeline->Resize(size);
			m_SpritesDepthPipeline->Resize(size);
			m_SpritesColorPipeline->Resize(size);
		}

		void InitWithOptions(const SceneRendererSettings& settings) override;

	private:
		void RenderMeshesDepth(const Ref<CommandBuffer>& cmd);
		void RenderSpritesDepth(const Ref<CommandBuffer>& cmd);
		void RenderMeshesColor(const Ref<CommandBuffer>& cmd);
		void RenderSpritesColor(const Ref<CommandBuffer>& cmd);
		void CompositePass(const Ref<CommandBuffer>& cmd);

		void InitMeshPipelines();
		void InitSpritesPipelines();
		void InitCompositePipelines();

	private:
		Ref<PipelineGraphics> m_MeshesDepthPipeline;
		Ref<PipelineGraphics> m_MeshesColorPipeline;

		Ref<PipelineGraphics> m_SpritesDepthPipeline;
		Ref<PipelineGraphics> m_SpritesColorPipeline;

		Ref<PipelineGraphics> m_CompositePipeline;

		Ref<Shader> m_TransparencyColorShader;
		Ref<Shader> m_TransparencyDepthShader;
		Ref<Shader> m_TransparencyCompositeShader;

		Ref<Buffer> m_OITBuffer;
		uint32_t m_Layers = 8u;

		uint64_t m_MeshesTexturesUpdatedFrame = 0;
		uint64_t m_SpritesTexturesUpdatedFrame = 0;
	};
}
