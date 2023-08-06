#pragma once

#include "RendererTask.h"

namespace Eagle
{
	class Shader;
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
		void OnResize(glm::uvec2 size) override {}

		const std::vector<Ref<Image>>& GetPointLightShadowMaps() const { return m_PLShadowMaps; }
		const std::vector<Ref<Image>>& GetSpotLightShadowMaps() const { return m_SLShadowMaps; }
		const std::vector<Ref<Image>>& GetDirectionalLightShadowMaps() const { return m_DLShadowMaps; }

		const std::vector<Ref<Sampler>>& GetPointLightShadowMapsSamplers() const { return m_PLShadowMapSamplers; }
		const std::vector<Ref<Sampler>>& GetSpotLightShadowMapsSamplers() const { return m_SLShadowMapSamplers; }
		const std::vector<Ref<Sampler>>& GetDirectionalLightShadowMapsSamplers() const { return m_DLShadowMapSamplers; }

	private:
		void InitMeshPipelines();
		void InitOpacitySpritesPipelines();
		void InitMaskedSpritesPipelines();
		void InitOpaqueLitTextsPipelines();
		void InitMaskedLitTextsPipelines();
		void InitUnlitTextsPipelines();

		void CreateIfNeededDirectionalLightShadowMaps();
		void FreeDirectionalLightShadowMaps();

		void ShadowPassOpacityMeshes(const Ref<CommandBuffer>& cmd);
		void ShadowPassMaskedMeshes(const Ref<CommandBuffer>& cmd);
		void ShadowPassOpacitySprites(const Ref<CommandBuffer>& cmd);
		void ShadowPassMaskedSprites(const Ref<CommandBuffer>& cmd);
		void ShadowPassOpaqueLitTexts(const Ref<CommandBuffer>& cmd);
		void ShadowPassMaskedLitTexts(const Ref<CommandBuffer>& cmd);
		void ShadowPassUnlitTexts(const Ref<CommandBuffer>& cmd);

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

		// For opacity meshes
		Ref<PipelineGraphics> m_OpacityMPLPipeline;
		Ref<PipelineGraphics> m_OpacityMSLPipeline;
		Ref<PipelineGraphics> m_OpacityMDLPipeline;

		// For masked meshes
		Ref<PipelineGraphics> m_MaskedMPLPipeline;
		Ref<PipelineGraphics> m_MaskedMSLPipeline;
		Ref<PipelineGraphics> m_MaskedMDLPipeline;
		Ref<PipelineGraphics> m_MaskedMPLPipelineClearing;
		Ref<PipelineGraphics> m_MaskedMSLPipelineClearing;
		Ref<PipelineGraphics> m_MaskedMDLPipelineClearing;

		// For opacity sprites
		Ref<PipelineGraphics> m_OpacitySPLPipeline;
		Ref<PipelineGraphics> m_OpacitySSLPipeline;
		Ref<PipelineGraphics> m_OpacitySDLPipeline;
		Ref<PipelineGraphics> m_OpacitySPLPipelineClearing;
		Ref<PipelineGraphics> m_OpacitySSLPipelineClearing;
		Ref<PipelineGraphics> m_OpacitySDLPipelineClearing;

		// For masked sprites
		Ref<PipelineGraphics> m_MaskedSPLPipeline;
		Ref<PipelineGraphics> m_MaskedSSLPipeline;
		Ref<PipelineGraphics> m_MaskedSDLPipeline;
		Ref<PipelineGraphics> m_MaskedSPLPipelineClearing;
		Ref<PipelineGraphics> m_MaskedSSLPipelineClearing;
		Ref<PipelineGraphics> m_MaskedSDLPipelineClearing;

		// For opaque lit texts
		Ref<PipelineGraphics> m_OpaqueLitTPLPipeline;
		Ref<PipelineGraphics> m_OpaqueLitTSLPipeline;
		Ref<PipelineGraphics> m_OpaqueLitTDLPipeline;
		Ref<PipelineGraphics> m_OpaqueLitTPLPipelineClearing;
		Ref<PipelineGraphics> m_OpaqueLitTSLPipelineClearing;
		Ref<PipelineGraphics> m_OpaqueLitTDLPipelineClearing;

		// For masked lit texts
		Ref<PipelineGraphics> m_MaskedLitTPLPipeline;
		Ref<PipelineGraphics> m_MaskedLitTSLPipeline;
		Ref<PipelineGraphics> m_MaskedLitTDLPipeline;
		Ref<PipelineGraphics> m_MaskedLitTPLPipelineClearing;
		Ref<PipelineGraphics> m_MaskedLitTSLPipelineClearing;
		Ref<PipelineGraphics> m_MaskedLitTDLPipelineClearing;

		// For unlit texts
		Ref<PipelineGraphics> m_UnlitTPLPipeline;
		Ref<PipelineGraphics> m_UnlitTSLPipeline;
		Ref<PipelineGraphics> m_UnlitTDLPipeline;
		Ref<PipelineGraphics> m_UnlitTPLPipelineClearing;
		Ref<PipelineGraphics> m_UnlitTSLPipelineClearing;
		Ref<PipelineGraphics> m_UnlitTDLPipelineClearing;

		Ref<Shader> m_TextFragShader;
		Ref<Shader> m_MaskedTextFragShader;

		// Used as a flag to indicate that atleast one draw happened
		bool bDidDrawDL = false;
		bool bDidDrawPL = false;
		bool bDidDrawSL = false;

		uint64_t m_MaskedMeshesDLTexturesUpdatedFrame = 0;
		uint64_t m_MaskedMeshesPLTexturesUpdatedFrame = 0;
		uint64_t m_MaskedMeshesSLTexturesUpdatedFrame = 0;

		uint64_t m_MaskedSpritesDLTexturesUpdatedFrame = 0;
		uint64_t m_MaskedSpritesPLTexturesUpdatedFrame = 0;
		uint64_t m_MaskedSpritesSLTexturesUpdatedFrame = 0;
	};
}
