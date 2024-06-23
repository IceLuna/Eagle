#pragma once

#include "RendererTask.h"
#include "Eagle/Renderer/VidWrappers/PipelineCompute.h"
#include "Eagle/Renderer/RendererUtils.h"

namespace Eagle
{
	class Image;
	class Buffer;

	class ScreenSpaceReflectionsTask : public RendererTask
	{
	public:
		ScreenSpaceReflectionsTask(SceneRenderer& renderer);
		void RecordCommandBuffer(const Ref<CommandBuffer>& cmd) override;
		void OnResize(const glm::uvec2 size) override;

	private:
		void ClassifyTiles(const Ref<CommandBuffer>& cmd);
		void PrepareIndirectArgs(const Ref<CommandBuffer>& cmd);
		void HZB(const Ref<CommandBuffer>& cmd);
		void Intersection(const Ref<CommandBuffer>& cmd);
		void Reproject(const Ref<CommandBuffer>& cmd);
		void Prefilter(const Ref<CommandBuffer>& cmd);
		void TemporalResolve(const Ref<CommandBuffer>& cmd);
		void Composite(const Ref<CommandBuffer>& cmd);

		void InitPipeline();
		void InitSizeDependentResources();
		void InitResources();

	private:
		Ref<PipelineCompute> m_ClassifyPipeline;
		Ref<PipelineCompute> m_PreparePipeline;
		Ref<PipelineCompute> m_HZBPipeline;
		Ref<PipelineCompute> m_IntersectionPipeline;
		Ref<PipelineCompute> m_ReprojectPipeline;
		Ref<PipelineCompute> m_PrefilterPipeline;
		Ref<PipelineCompute> m_TemporalPipeline;
		Ref<PipelineCompute> m_CompositePipeline;

		Ref<Buffer> m_RayCounter;
		Ref<Buffer> m_IntersectionPassIndirectArgs;
		Ref<Buffer> m_RayList;
		Ref<Buffer> m_DenoiserTileList;
		Ref<Buffer> m_Uniform;

		std::array<Ref<Image>, 2> m_Radiance;
		std::array<Ref<Image>, 2> m_AverageRadiance;
		std::array<Ref<Image>, 2> m_Variance;
		std::array<Ref<Image>, 2> m_SampleCount;
		Ref<Image> m_ReprojectedRadiance;
		Ref<Image> m_Roughness;
		Ref<Image> m_RoughnessHistory;

		Ref<Buffer> m_TempDepthCopy;
		Ref<Image> m_HZB;
		std::vector<ImageView> m_HZBMipViews;
		Ref<Sampler> m_HZBSampler;

		uint32_t m_PingPong = 0;

		glm::uvec2 m_Size;

		struct UniformData
		{
			glm::mat4 View;
			glm::mat4 Proj;
			glm::mat4 InvProj;
			glm::mat4 InvView;
			glm::mat4 InvViewProj;
			glm::mat4 PrevViewProj;
			float RoughnessThreshold;
		} m_UniformData;
	};
}
