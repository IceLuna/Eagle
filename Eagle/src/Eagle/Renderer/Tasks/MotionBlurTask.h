#pragma once

#include "RendererTask.h"
#include "Eagle/Renderer/VidWrappers/PipelineCompute.h"

namespace Eagle
{
	class Image;
	class Buffer;

	class MotionBlurTask : public RendererTask
	{
	public:
		MotionBlurTask(SceneRenderer& renderer);
		void RecordCommandBuffer(const Ref<CommandBuffer>& cmd) override;

		void OnResize(glm::uvec2 size) override;

		void InitWithOptions(const SceneRendererSettings& settings) override;

	private:
		void InitPipeline();
		void InitMainPipeline();
		void InitResources();

		void TileMinMaxPass(const Ref<CommandBuffer>& cmd);
		void NeighborhoodMinMaxPass(const Ref<CommandBuffer>& cmd);
		void MainPass(const Ref<CommandBuffer>& cmd);

	private:
		Ref<PipelineCompute> m_TileHorizontalPipeline;
		Ref<PipelineCompute> m_TileVerticalPipeline;
		Ref<PipelineCompute> m_NeighborhoodPipeline;

		Ref<PipelineCompute> m_MainEarlyPipeline;
		Ref<PipelineCompute> m_MainCheapPipeline;
		Ref<PipelineCompute> m_MainExpensivePipeline;

		Ref<Image> m_TileMinHorizontal;
		Ref<Image> m_TileMaxHorizontal;
		Ref<Image> m_TileMin;
		Ref<Image> m_TileMax;
		Ref<Image> m_NeighborhoodMax;
		Ref<Image> m_ColorCopy;

		// Used for indirect dispatch
		Ref<Buffer> m_DispatchArgs;
		Ref<Buffer> m_EarlyExitTiles;
		Ref<Buffer> m_CheapTiles;
		Ref<Buffer> m_ExpensiveTiles;
		PostprocessTileStatistics m_Tiles{};

		glm::uvec2 m_Size; // Viewport size
		uint32_t m_NumSamples = 16;
		bool bDebugTiles = false; // Debug output

		struct PushData
		{
			float ZNear = 0.f;
			float ZFar = 1.f;
			glm::vec2 TexelSize;
			glm::uvec2 Size;
			glm::uvec2 PassSize;
			//uint32_t NumSamples = 16; // TODO: Make it a const_id
		} m_PushData;

		constexpr static uint32_t s_TileSize = 16; // Same in shaders
	};
}
