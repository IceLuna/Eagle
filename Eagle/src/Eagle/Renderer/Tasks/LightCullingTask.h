#pragma once

#include "RendererTask.h"
#include "Eagle/Renderer/VidWrappers/PipelineCompute.h"
#include "Eagle/Renderer/Tasks/LightsManagerTask.h"

namespace Eagle
{
	class LightCullingTask : public RendererTask
	{
	public:
		LightCullingTask(SceneRenderer& renderer);
		void RecordCommandBuffer(const Ref<CommandBuffer>& cmd) override;

		void InitWithOptions(const SceneRendererSettings& settings) override
		{
			if (settings.bVisualizeLightTiles == bVisualizeTiles)
				return;

			bVisualizeTiles = settings.bVisualizeLightTiles;
			InitPipelines();
		}

		void OnResize(const glm::uvec2 size) override;

		const auto& GetCulledPointLights() const { return m_CulledPointLights; }
		const auto& GetCulledSpotLights() const { return m_CulledSpotLights; }

		const Ref<Buffer>& GetCulledPointLightsBuffer() const { return m_CulledPointLightsBuffer; }
		const Ref<Buffer>& GetCulledSpotLightsBuffer() const { return m_CulledSpotLightsBuffer; }
		const Ref<Buffer>& GetLightsCountersBuffer() const { return m_CountersBuffer; }

		const Ref<Buffer>& GetTiles_Opaque_PL() const { return m_Tiles_Opaque_PL; }
		const Ref<Buffer>& GetTiles_Opaque_SL() const { return m_Tiles_Opaque_SL; }
		const Ref<Buffer>& GetTiles_Translucent_PL() const { return m_Tiles_Translucent_PL; }
		const Ref<Buffer>& GetTiles_Translucent_SL() const { return m_Tiles_Translucent_SL; }

		uint32_t GetTilesBufferWidth() const { return m_TilesBufferWidth; }

	private:
		void InitPipelines();

		void CullForCameraView(const Ref<CommandBuffer>& cmd);
		void Cull(const Ref<CommandBuffer>& cmd);

	private:
		Ref<PipelineCompute> m_TileCulling;

		Ref<Buffer> m_CulledPointLightsBuffer;
		Ref<Buffer> m_CulledSpotLightsBuffer;
		Ref<Buffer> m_CountersBuffer; // Stores two counters: how many culled point & spot lights we have to process

		Ref<Buffer> m_Tiles_Opaque_PL;
		Ref<Buffer> m_Tiles_Opaque_SL;
		Ref<Buffer> m_Tiles_Translucent_PL;
		Ref<Buffer> m_Tiles_Translucent_SL;

		std::vector<LightsManagerTask::PointLight> m_CulledPointLights;
		std::vector<LightsManagerTask::SpotLight> m_CulledSpotLights;

		uint32_t m_TilesBufferWidth = 0;

		bool bVisualizeTiles = false;
	};
}
