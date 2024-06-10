#pragma once

#include "RendererTask.h"
#include "Eagle/Renderer/VidWrappers/PipelineCompute.h"

namespace Eagle
{
	class Image;
	class Buffer;

	class DOFTask : public RendererTask
	{
	public:
		DOFTask(SceneRenderer& renderer);
		void RecordCommandBuffer(const Ref<CommandBuffer>& cmd) override;

		void OnResize(glm::uvec2 size) override;

		void InitWithOptions(const SceneRendererSettings& settings) override;

	private:
		void InitPipeline();
		void InitMainPipeline();
		void InitResources();

		void TileMinMaxPass(const Ref<CommandBuffer>& cmd);
		void NeighborhoodMinMaxPass(const Ref<CommandBuffer>& cmd);
		void Presort(const Ref<CommandBuffer>& cmd);
		void MainPass(const Ref<CommandBuffer>& cmd);
		void PostFilterPass(const Ref<CommandBuffer>& cmd);
		void UpsamplePass(const Ref<CommandBuffer>& cmd);

	private:
		Ref<PipelineCompute> m_TileHorizontalPipeline;
		Ref<PipelineCompute> m_TileVerticalPipeline;
		Ref<PipelineCompute> m_NeighborhoodPipeline;

		Ref<PipelineCompute> m_PresortEarlyPipeline;
		Ref<PipelineCompute> m_PresortCheapPipeline;
		Ref<PipelineCompute> m_PresortExpensivePipeline;

		Ref<PipelineCompute> m_MainEarlyPipeline;
		Ref<PipelineCompute> m_MainCheapPipeline;
		Ref<PipelineCompute> m_MainExpensivePipeline;

		Ref<PipelineCompute> m_PostFilterPipeline;
		Ref<PipelineCompute> m_UpsamplePipeline;

		Ref<Image> m_TileMaxHorizontal;
		Ref<Image> m_TileMinCOCHorizontal;
		Ref<Image> m_TileMax;
		Ref<Image> m_TileMinCOC;
		Ref<Image> m_NeighborhoodMax;

		Ref<Image> m_Presort;
		Ref<Image> m_Prefilter;
		Ref<Image> m_Main;
		Ref<Image> m_Postfilter;
		Ref<Image> m_AlphaTemp;
		Ref<Image> m_AlphaResult;

		// Used for indirect dispatch
		Ref<Buffer> m_DispatchArgs;
		Ref<Buffer> m_EarlyExitTiles;
		Ref<Buffer> m_CheapTiles;
		Ref<Buffer> m_ExpensiveTiles;
		PostprocessTileStatistics m_Tiles{};

		glm::uvec2 m_Size; // Viewport size
		bool bDebugTiles = false; // Debug output

		struct PushData
		{
			glm::vec2 TexelSize;
			glm::vec2 ApertureShape;
			float ApertureSize;
			float FocalLength;
			float ZNear;
			float ZFar;
			float COCScale;
			float MaxCOC;
			glm::uvec2 Size;
			glm::uvec2 PassSize;
		} m_PushData;

		constexpr static uint32_t s_DOFTileSize = 32; // Same in shaders
	};
}
