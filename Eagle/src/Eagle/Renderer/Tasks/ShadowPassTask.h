#pragma once

#include "RendererTask.h"
#include "Eagle/Renderer/RendererUtils.h"

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
		void InitWithOptions(const SceneRendererSettings& settings) override;

		const std::vector<Ref<Image>>& GetPointLightShadowMaps() const { return m_PLShadowMaps; }
		const std::vector<Ref<Image>>& GetPointLightShadowMapsColored() const { return m_PLCShadowMaps; }
		const std::vector<Ref<Image>>& GetPointLightShadowMapsColoredDepth() const { return m_PLCDShadowMaps; }

		const std::vector<Ref<Image>>& GetSpotLightShadowMaps() const { return m_SLShadowMaps; }
		const std::vector<Ref<Image>>& GetSpotLightShadowMapsColored() const { return m_SLCShadowMaps; }
		const std::vector<Ref<Image>>& GetSpotLightShadowMapsColoredDepth() const { return m_SLCDShadowMaps; }

		const std::vector<Ref<Image>>& GetDirectionalLightShadowMaps() const { return m_DLShadowMaps; }
		const std::vector<Ref<Image>>& GetDirectionalLightShadowMapsColored() const { return m_DLCShadowMaps; }
		const std::vector<Ref<Image>>& GetDirectionalLightShadowMapsColoredDepth() const { return m_DLCDShadowMaps; }

		const std::vector<Ref<Sampler>>& GetPointLightShadowMapsSamplers() const { return m_PLShadowMapSamplers; }
		const std::vector<Ref<Sampler>>& GetSpotLightShadowMapsSamplers() const { return m_SLShadowMapSamplers; }
		const std::vector<Ref<Sampler>>& GetDirectionalLightShadowMapsSamplers() const { return m_DLShadowMapSamplers; }

	private:
		void InitOpacityMaskedMeshPipelines();
		void InitTranslucentMeshPipelines();

		void InitOpacitySpritesPipelines();
		void InitMaskedSpritesPipelines();
		void InitTranslucentSpritesPipelines();

		void InitOpaqueLitTextsPipelines();
		void InitMaskedLitTextsPipelines();
		void InitTranslucentLitTextsPipelines();

		void InitUnlitTextsPipelines();

		void CreateIfNeededDirectionalLightShadowMaps();
		void InitDirectionalLightShadowMaps();
		void FreeDirectionalLightShadowMaps();

		void CreateIfNeededColoredDirectionalLightShadowMaps();
		void InitColoredDirectionalLightShadowMaps();
		void InitColoredDirectionalLightFramebuffers(std::vector<Ref<Framebuffer>>& framebuffers, const Ref<PipelineGraphics>& pipeline, bool bIncludeDepth);
		void FreeColoredDirectionalLightShadowMaps();

		void HandlePointLightResources(const Ref<CommandBuffer>& cmd);
		void HandleSpotLightResources(const Ref<CommandBuffer>& cmd);
		void ClearFramebuffers(const Ref<CommandBuffer>& cmd);

		void HandleColoredPointLightShadowMaps();
		void HandleColoredSpotLightShadowMaps();

		void ShadowPassOpacityMeshes(const Ref<CommandBuffer>& cmd);
		void ShadowPassMaskedMeshes(const Ref<CommandBuffer>& cmd);
		void ShadowPassTranslucentMeshes(const Ref<CommandBuffer>& cmd);

		void ShadowPassOpacitySprites(const Ref<CommandBuffer>& cmd);
		void ShadowPassMaskedSprites(const Ref<CommandBuffer>& cmd);
		void ShadowPassTranslucentSprites(const Ref<CommandBuffer>& cmd);

		void ShadowPassOpaqueLitTexts(const Ref<CommandBuffer>& cmd);
		void ShadowPassMaskedLitTexts(const Ref<CommandBuffer>& cmd);
		void ShadowPassTranslucentLitTexts(const Ref<CommandBuffer>& cmd);

		void ShadowPassUnlitTexts(const Ref<CommandBuffer>& cmd);

		glm::uvec2 GetPointLightSMSize(float distanceToCamera, float maxShadowDistance);
		glm::uvec2 GetSpotLightSMSize(float distanceToCamera, float maxShadowDistance);

	private:
		ShadowMapsSettings m_Settings;

		std::vector<size_t> m_PointLightIndices;
		std::vector<size_t> m_SpotLightIndices;

		// Point Light
		std::vector<Ref<Framebuffer>> m_PLFramebuffers;
		std::vector<Ref<Image>> m_PLShadowMaps;
		std::vector<Ref<Sampler>> m_PLShadowMapSamplers;
		Ref<Buffer> m_PLVPsBuffer;
		//Colored
		std::vector<Ref<Framebuffer>> m_PLCFramebuffers;
		std::vector<Ref<Framebuffer>> m_PLCFramebuffers_NoDepth;
		std::vector<Ref<Image>> m_PLCShadowMaps;
		std::vector<Ref<Image>> m_PLCDShadowMaps;

		// Spot Light
		std::vector<Ref<Framebuffer>> m_SLFramebuffers;
		std::vector<Ref<Image>> m_SLShadowMaps;
		// Colored
		std::vector<Ref<Framebuffer>> m_SLCFramebuffers;
		std::vector<Ref<Framebuffer>> m_SLCFramebuffers_NoDepth;
		std::vector<Ref<Image>> m_SLCShadowMaps;
		std::vector<Ref<Image>> m_SLCDShadowMaps;

		const std::vector<Ref<Sampler>>& m_SLShadowMapSamplers; // Spot light samplers are the same as for point lights

		// Directional Light
		std::vector<Ref<Image>> m_DLShadowMaps;
		std::vector<Ref<Sampler>> m_DLShadowMapSamplers;
		std::vector<Ref<Framebuffer>> m_DLFramebuffers;
		// Colored
		std::vector<Ref<Image>> m_DLCShadowMaps;
		std::vector<Ref<Image>> m_DLCDShadowMaps;
		std::vector<Ref<Framebuffer>> m_DLCFramebuffers;
		std::vector<Ref<Framebuffer>> m_DLCFramebuffers_NoDepth;

		// For opacity meshes
		Ref<PipelineGraphics> m_OpacityMPLPipeline;
		Ref<PipelineGraphics> m_OpacityMSLPipeline;
		Ref<PipelineGraphics> m_OpacityMDLPipeline;

		// For translucent meshes
		Ref<PipelineGraphics> m_TranslucentMPLPipeline;
		Ref<PipelineGraphics> m_TranslucentMSLPipeline;
		Ref<PipelineGraphics> m_TranslucentMDLPipeline;

		Ref<PipelineGraphics> m_TranslucentMPLPipeline_NoDepth;
		Ref<PipelineGraphics> m_TranslucentMSLPipeline_NoDepth;
		Ref<PipelineGraphics> m_TranslucentMDLPipeline_NoDepth;

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

		// For translucent sprites
		Ref<PipelineGraphics> m_TranslucentSPLPipeline;
		Ref<PipelineGraphics> m_TranslucentSSLPipeline;
		Ref<PipelineGraphics> m_TranslucentSDLPipeline;
		Ref<PipelineGraphics> m_TranslucentSPLPipelineClearing;
		Ref<PipelineGraphics> m_TranslucentSSLPipelineClearing;
		Ref<PipelineGraphics> m_TranslucentSDLPipelineClearing;

		Ref<PipelineGraphics> m_TranslucentSPLPipeline_NoDepth;
		Ref<PipelineGraphics> m_TranslucentSPLPipelineClearing_NoDepth;
		Ref<PipelineGraphics> m_TranslucentSSLPipeline_NoDepth;
		Ref<PipelineGraphics> m_TranslucentSSLPipelineClearing_NoDepth;
		Ref<PipelineGraphics> m_TranslucentSDLPipeline_NoDepth;
		Ref<PipelineGraphics> m_TranslucentSDLPipelineClearing_NoDepth;

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

		// For translucent lit texts
		Ref<PipelineGraphics> m_TranslucentLitTPLPipeline;
		Ref<PipelineGraphics> m_TranslucentLitTSLPipeline;
		Ref<PipelineGraphics> m_TranslucentLitTDLPipeline;
		Ref<PipelineGraphics> m_TranslucentLitTPLPipelineClearing;
		Ref<PipelineGraphics> m_TranslucentLitTSLPipelineClearing;
		Ref<PipelineGraphics> m_TranslucentLitTDLPipelineClearing;

		Ref<PipelineGraphics> m_TranslucentLitTPLPipeline_NoDepth;
		Ref<PipelineGraphics> m_TranslucentLitTPLPipelineClearing_NoDepth;
		Ref<PipelineGraphics> m_TranslucentLitTSLPipeline_NoDepth;
		Ref<PipelineGraphics> m_TranslucentLitTSLPipelineClearing_NoDepth;
		Ref<PipelineGraphics> m_TranslucentLitTDLPipeline_NoDepth;
		Ref<PipelineGraphics> m_TranslucentLitTDLPipelineClearing_NoDepth;

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

		// Used as a flag to indicate that atleast one draw happened
		bool bDidDrawDL = false;
		bool bDidDrawPL = false;
		bool bDidDrawSL = false;

		// Colored
		bool bDidDrawDLC = false;
		bool bDidDrawPLC = false;
		bool bDidDrawSLC = false;

		uint64_t m_MaskedMeshesDLTexturesUpdatedFrames[RendererConfig::FramesInFlight] = { 0 };
		uint64_t m_MaskedMeshesPLTexturesUpdatedFrames[RendererConfig::FramesInFlight] = { 0 };
		uint64_t m_MaskedMeshesSLTexturesUpdatedFrames[RendererConfig::FramesInFlight] = { 0 };

		uint64_t m_TranslucentMeshesDLTexturesUpdatedFrames[RendererConfig::FramesInFlight] = { 0 };
		uint64_t m_TranslucentMeshesPLTexturesUpdatedFrames[RendererConfig::FramesInFlight] = { 0 };
		uint64_t m_TranslucentMeshesSLTexturesUpdatedFrames[RendererConfig::FramesInFlight] = { 0 };

		uint64_t m_MaskedSpritesDLTexturesUpdatedFrames[RendererConfig::FramesInFlight] = { 0 };
		uint64_t m_MaskedSpritesPLTexturesUpdatedFrames[RendererConfig::FramesInFlight] = { 0 };
		uint64_t m_MaskedSpritesSLTexturesUpdatedFrames[RendererConfig::FramesInFlight] = { 0 };

		uint64_t m_TranslucentSpritesDLTexturesUpdatedFrames[RendererConfig::FramesInFlight] = { 0 };
		uint64_t m_TranslucentSpritesPLTexturesUpdatedFrames[RendererConfig::FramesInFlight] = { 0 };
		uint64_t m_TranslucentSpritesSLTexturesUpdatedFrames[RendererConfig::FramesInFlight] = { 0 };

		bool bVolumetricLightsEnabled = false;
		bool bTranslucencyShadowsEnabled = false;
	};
}
