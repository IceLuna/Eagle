#pragma once

#include "RendererTask.h"
#include "Eagle/Renderer/VidWrappers/PipelineCompute.h"
#include "Eagle/Renderer/RendererUtils.h"

namespace Eagle
{
	class Image;

	// Calculates animations for skeletal meshes. Outputs skinned vertices into a different buffer (GetSkinnedVertices()).
	// Note: skinned vertices are already in the world space coords
	class SkinCacheTask : public RendererTask
	{
	public:
		SkinCacheTask(SceneRenderer& renderer);
		void RecordCommandBuffer(const Ref<CommandBuffer>& cmd) override;

		void InitWithOptions(const SceneRendererSettings& settings) override
		{
			if (bMotionRequired == settings.InternalState.bMotionBuffer)
				return;

			bMotionRequired = settings.InternalState.bMotionBuffer;
			InitPipeline();
		}

		const Ref<Buffer>& GetSkinnedVertices() const { return m_SkinnedVertices; }
		const Ref<Buffer>& GetPrevSkinnedVerticesPositions() const { return m_PrevSkinnedVerticesPosition; }

	private:
		void InitPipeline();

	private:
		Ref<PipelineCompute> m_Pipeline;
		Ref<Buffer> m_SkinnedVertices;
		Ref<Buffer> m_PrevSkinnedVerticesPosition;
		bool bMotionRequired = false;
		bool bVerticesValid = false;
	};
}
