#pragma once

#include "RendererTask.h"
#include "Eagle/Renderer/VidWrappers/PipelineGraphics.h"
#include "Eagle/Renderer/VidWrappers/PipelineCompute.h"

#include "GeometryManagerTask.h"

namespace Eagle
{
	struct FrustumCulledMeshes;

	class RenderMeshesTask : public RendererTask
	{
	public:
		RenderMeshesTask(SceneRenderer& renderer);

		void RecordCommandBuffer(const Ref<CommandBuffer>& cmd) override;
		void OnResize(glm::uvec2 size) override
		{
			m_OpaquePipeline->Resize(size.x, size.y);
			m_MaskedPipeline->Resize(size.x, size.y);
		}

		void InitWithOptions(const SceneRendererSettings& settings) override
		{
			if (bMotionRequired == settings.InternalState.bMotionBuffer &&
				bGeometricSpecularAA == settings.bGeometricSpecularAA)
				return;

			bMotionRequired = settings.InternalState.bMotionBuffer;
			bGeometricSpecularAA = settings.bGeometricSpecularAA;

			InitPipeline();
		}

		static void DrawCulled(const Ref<CommandBuffer>& cmd, const Ref<PipelineGraphics>& pipeline, const StaticMeshGeometryData& buffers, const FrustumCulledMeshes& meshes,
			MaterialBlendMode blendMode, RenderStats& stats, const void* vertexPushData = nullptr);
		static void DrawUnculledShadowCasters(const Ref<CommandBuffer>& cmd, const Ref<PipelineGraphics>& pipeline, const StaticMeshGeometryData& buffers, const FrustumCulledMeshes& meshes,
			MaterialBlendMode blendMode, RenderStats& stats, const void* vertexPushData = nullptr, const Ref<Framebuffer>& framebuffer = nullptr, CullMode singleSidedCullMode = CullMode::Front);

		inline static const std::vector<PipelineGraphicsState::VertexInputAttribute> PerInstanceAttribs = { { 4u } }; // Locations of Per-Instance data in shader

	private:
		void InitPipeline();
		void RenderOpaque(const Ref<CommandBuffer>& cmd);
		void RenderMasked(const Ref<CommandBuffer>& cmd);

	private:
		Ref<PipelineGraphics> m_OpaquePipeline;
		Ref<PipelineGraphics> m_MaskedPipeline;

		uint64_t m_OpaqueTexturesUpdatedFrames[RendererConfig::FramesInFlight] = { 0 };
		uint64_t m_MaskedTexturesUpdatedFrames[RendererConfig::FramesInFlight] = { 0 };
		bool bMotionRequired = false;
		bool bGeometricSpecularAA = true;
	};
}
