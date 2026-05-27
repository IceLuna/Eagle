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

		const Ref<Sampler>& GetPCFSampler() const { return m_PCFSampler; }
		const Ref<Sampler>& GetPointSampler() const { return m_PointSampler; }
		const Ref<Sampler>& GetColoredShadowMapsSampler() const { return m_ColoredShadowMapSampler; }

	private:
		void InitOpacityMaskedMeshPipelines();
		void InitTranslucentMeshPipelines();

		void InitOpacitySkeletalMeshPipelines();
		void InitMaskedSkeletalMeshPipelines();
		void InitTranslucentSkeletalMeshPipelines();

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
		void InitColoredDirectionalLightFramebuffers(std::vector<Ref<Framebuffer>>& framebuffers, const Ref<PipelineGraphics>& pipeline);
		void FreeColoredDirectionalLightShadowMaps();

		void HandlePointLightResources(const Ref<CommandBuffer>& cmd);
		void HandleSpotLightResources(const Ref<CommandBuffer>& cmd);
		void HandleDirectionalLightResources(const Ref<CommandBuffer>& cmd);
		void ClearShadowMaps(const Ref<CommandBuffer>& cmd);
		void PrepareShadowMapsForSampling(const Ref<CommandBuffer>& cmd);

		void HandleColoredPointLightShadowMaps();
		void HandleColoredSpotLightShadowMaps();

		void ShadowPassOpaqueMeshes(const Ref<CommandBuffer>& cmd);
		void ShadowPassMaskedMeshes(const Ref<CommandBuffer>& cmd);
		void ShadowPassTranslucentMeshes(const Ref<CommandBuffer>& cmd);

		void ShadowPassOpaqueSkeletalMeshes(const Ref<CommandBuffer>& cmd);
		void ShadowPassMaskedSkeletalMeshes(const Ref<CommandBuffer>& cmd);
		void ShadowPassTranslucentSkeletalMeshes(const Ref<CommandBuffer>& cmd);

		void ShadowPassOpaqueSprites(const Ref<CommandBuffer>& cmd);
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

		Ref<Sampler> m_PCFSampler;
		Ref<Sampler> m_PointSampler;
		Ref<Sampler> m_ColoredShadowMapSampler;

		// Point Light
		std::vector<Ref<Framebuffer>> m_PLFramebuffers;
		std::vector<Ref<Image>> m_PLShadowMaps;
		Ref<Buffer> m_PLVPsBuffer;
		std::vector<glm::mat4> m_PLVPs;
		//Colored
		std::vector<Ref<Framebuffer>> m_PLCFramebuffers;
		std::vector<Ref<Image>> m_PLCShadowMaps;
		std::vector<Ref<Image>> m_PLCDShadowMaps;

		// Spot Light
		std::vector<Ref<Framebuffer>> m_SLFramebuffers;
		std::vector<Ref<Image>> m_SLShadowMaps;
		// Colored
		std::vector<Ref<Framebuffer>> m_SLCFramebuffers;
		std::vector<Ref<Image>> m_SLCShadowMaps;
		std::vector<Ref<Image>> m_SLCDShadowMaps;

		// Directional Light
		std::vector<Ref<Image>> m_DLShadowMaps;
		std::vector<Ref<Framebuffer>> m_DLFramebuffers;
		// Colored
		std::vector<Ref<Image>> m_DLCShadowMaps;
		std::vector<Ref<Image>> m_DLCDShadowMaps;
		std::vector<Ref<Framebuffer>> m_DLCFramebuffers;

		// For opacity meshes
		Ref<PipelineGraphics> m_OpacityMPLPipeline;
		Ref<PipelineGraphics> m_OpacityMSLPipeline;
		Ref<PipelineGraphics> m_OpacityMDLPipeline;

		// For translucent meshes
		Ref<PipelineGraphics> m_TranslucentMPLPipeline;
		Ref<PipelineGraphics> m_TranslucentMSLPipeline;
		Ref<PipelineGraphics> m_TranslucentMDLPipeline;

		// For masked meshes
		Ref<PipelineGraphics> m_MaskedMPLPipeline;
		Ref<PipelineGraphics> m_MaskedMSLPipeline;
		Ref<PipelineGraphics> m_MaskedMDLPipeline;

		// For opacity skeletal meshes
		Ref<PipelineGraphics> m_OpacitySMPLPipeline;
		Ref<PipelineGraphics> m_OpacitySMSLPipeline;
		Ref<PipelineGraphics> m_OpacitySMDLPipeline;

		// For translucent skeletal meshes
		Ref<PipelineGraphics> m_TranslucentSMPLPipeline;
		Ref<PipelineGraphics> m_TranslucentSMSLPipeline;
		Ref<PipelineGraphics> m_TranslucentSMDLPipeline;

		// For masked skeletal meshes
		Ref<PipelineGraphics> m_MaskedSMPLPipeline;
		Ref<PipelineGraphics> m_MaskedSMSLPipeline;
		Ref<PipelineGraphics> m_MaskedSMDLPipeline;

		// For opacity sprites
		Ref<PipelineGraphics> m_OpacitySPLPipeline;
		Ref<PipelineGraphics> m_OpacitySSLPipeline;
		Ref<PipelineGraphics> m_OpacitySDLPipeline;

		// For translucent sprites
		Ref<PipelineGraphics> m_TranslucentSPLPipeline;
		Ref<PipelineGraphics> m_TranslucentSSLPipeline;
		Ref<PipelineGraphics> m_TranslucentSDLPipeline;

		// For masked sprites
		Ref<PipelineGraphics> m_MaskedSPLPipeline;
		Ref<PipelineGraphics> m_MaskedSSLPipeline;
		Ref<PipelineGraphics> m_MaskedSDLPipeline;

		// For opaque lit texts
		Ref<PipelineGraphics> m_OpaqueLitTPLPipeline;
		Ref<PipelineGraphics> m_OpaqueLitTSLPipeline;
		Ref<PipelineGraphics> m_OpaqueLitTDLPipeline;

		// For translucent lit texts
		Ref<PipelineGraphics> m_TranslucentLitTPLPipeline;
		Ref<PipelineGraphics> m_TranslucentLitTSLPipeline;
		Ref<PipelineGraphics> m_TranslucentLitTDLPipeline;

		// For masked lit texts
		Ref<PipelineGraphics> m_MaskedLitTPLPipeline;
		Ref<PipelineGraphics> m_MaskedLitTSLPipeline;
		Ref<PipelineGraphics> m_MaskedLitTDLPipeline;

		// For unlit texts
		Ref<PipelineGraphics> m_UnlitTPLPipeline;
		Ref<PipelineGraphics> m_UnlitTSLPipeline;
		Ref<PipelineGraphics> m_UnlitTDLPipeline;

		uint64_t m_MaskedMeshesDLTexturesUpdatedFrames[RendererConfig::FramesInFlight] = { 0 };
		uint64_t m_MaskedMeshesPLTexturesUpdatedFrames[RendererConfig::FramesInFlight] = { 0 };
		uint64_t m_MaskedMeshesSLTexturesUpdatedFrames[RendererConfig::FramesInFlight] = { 0 };

		uint64_t m_TranslucentMeshesDLTexturesUpdatedFrames[RendererConfig::FramesInFlight] = { 0 };
		uint64_t m_TranslucentMeshesPLTexturesUpdatedFrames[RendererConfig::FramesInFlight] = { 0 };
		uint64_t m_TranslucentMeshesSLTexturesUpdatedFrames[RendererConfig::FramesInFlight] = { 0 };

		uint64_t m_MaskedSkeletalMeshesDLTexturesUpdatedFrames[RendererConfig::FramesInFlight] = { 0 };
		uint64_t m_MaskedSkeletalMeshesPLTexturesUpdatedFrames[RendererConfig::FramesInFlight] = { 0 };
		uint64_t m_MaskedSkeletalMeshesSLTexturesUpdatedFrames[RendererConfig::FramesInFlight] = { 0 };

		uint64_t m_TranslucentSkeletalMeshesDLTexturesUpdatedFrames[RendererConfig::FramesInFlight] = { 0 };
		uint64_t m_TranslucentSkeletalMeshesPLTexturesUpdatedFrames[RendererConfig::FramesInFlight] = { 0 };
		uint64_t m_TranslucentSkeletalMeshesSLTexturesUpdatedFrames[RendererConfig::FramesInFlight] = { 0 };

		uint64_t m_MaskedSpritesDLTexturesUpdatedFrames[RendererConfig::FramesInFlight] = { 0 };
		uint64_t m_MaskedSpritesPLTexturesUpdatedFrames[RendererConfig::FramesInFlight] = { 0 };
		uint64_t m_MaskedSpritesSLTexturesUpdatedFrames[RendererConfig::FramesInFlight] = { 0 };

		uint64_t m_TranslucentSpritesDLTexturesUpdatedFrames[RendererConfig::FramesInFlight] = { 0 };
		uint64_t m_TranslucentSpritesPLTexturesUpdatedFrames[RendererConfig::FramesInFlight] = { 0 };
		uint64_t m_TranslucentSpritesSLTexturesUpdatedFrames[RendererConfig::FramesInFlight] = { 0 };

		uint64_t m_MaskedLitTextsDLTexturesUpdatedFrames[RendererConfig::FramesInFlight] = { 0 };
		uint64_t m_MaskedLitTextsPLTexturesUpdatedFrames[RendererConfig::FramesInFlight] = { 0 };
		uint64_t m_MaskedLitTextsSLTexturesUpdatedFrames[RendererConfig::FramesInFlight] = { 0 };

		uint64_t m_TranslucentLitTextsDLTexturesUpdatedFrames[RendererConfig::FramesInFlight] = { 0 };
		uint64_t m_TranslucentLitTextsPLTexturesUpdatedFrames[RendererConfig::FramesInFlight] = { 0 };
		uint64_t m_TranslucentLitTextsSLTexturesUpdatedFrames[RendererConfig::FramesInFlight] = { 0 };

		bool bVolumetricLightsEnabled = false;
		bool bTranslucencyShadowsEnabled = false;
		bool bUseVolumetricLights = false;
	};
}
