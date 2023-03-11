#pragma once

#include "RendererTask.h"

namespace Eagle
{
	class Buffer;
	class Image;
	class Sampler;
	class Framebuffer;
	class PipelineGraphics;

	class ShadowPassTask : public RendererTask
	{
	public:
		ShadowPassTask(SceneRenderer& renderer);

		void RecordCommandBuffer(const Ref<CommandBuffer>& cmd) override;
		void OnResize(const glm::uvec2 size) override {}

		const std::vector<Ref<Image>>& GetPointLightShadowMaps() const { return m_PLShadowMaps; }
		const std::vector<Ref<Image>>& GetSpotLightShadowMaps() const { return m_SLShadowMaps; }
		const std::vector<Ref<Image>>& GetDirectionalLightShadowMaps() const { return m_DLShadowMaps; }

		const std::vector<Ref<Sampler>>& GetPointLightShadowMapsSamplers() const { return m_PLShadowMapSamplers; }
		const std::vector<Ref<Sampler>>& GetSpotLightShadowMapsSamplers() const { return m_SLShadowMapSamplers; }
		const std::vector<Ref<Sampler>>& GetDirectionalLightShadowMapsSamplers() const { return m_DLShadowMapSamplers; }

	private:
		void InitMeshPipelines();
		void InitSpritesPipelines();
		void ShadowPassMeshes(const Ref<CommandBuffer>& cmd);
		void ShadowPassSprites(const Ref<CommandBuffer>& cmd);

	private:
		// Point Light
		std::vector<Ref<Framebuffer>> m_PLFramebuffers;
		std::vector<Ref<Image>> m_PLShadowMaps;
		std::vector<Ref<Sampler>> m_PLShadowMapSamplers;
		Ref<Buffer> m_PLVPsBuffer;

		// Spot Light
		std::vector<Ref<Framebuffer>> m_SLFramebuffers;
		std::vector<Ref<Image>> m_SLShadowMaps;
		const std::vector<Ref<Sampler>>& m_SLShadowMapSamplers; // Spot light samplers are the same as for point lights

		// Directional Light
		std::vector<Ref<Image>> m_DLShadowMaps;
		std::vector<Ref<Sampler>> m_DLShadowMapSamplers;
		std::vector<Ref<Framebuffer>> m_DLFramebuffers;

		// For meshes
		Ref<PipelineGraphics> m_MPLPipeline;
		Ref<PipelineGraphics> m_MSLPipeline;
		Ref<PipelineGraphics> m_MDLPipeline;

		// For sprites
		Ref<PipelineGraphics> m_SPLPipeline;
		Ref<PipelineGraphics> m_SSLPipeline;
		Ref<PipelineGraphics> m_SDLPipeline;
	};
}
