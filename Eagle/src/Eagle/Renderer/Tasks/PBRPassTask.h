#pragma once

#include "RendererTask.h"
#include "Eagle/Renderer/VidWrappers/PipelineGraphics.h"

namespace Eagle
{
	class Buffer;

	class PBRPassTask : public RendererTask
	{
	public:
		PBRPassTask(SceneRenderer& renderer, const Ref<Image>& renderTo);

		void RecordCommandBuffer(const Ref<CommandBuffer>& cmd) override;
		void OnResize(const glm::uvec2 size) override { m_Pipeline->Resize(size.x, size.y); }
		void SetSoftShadowsEnabled(bool bEnable);
		void SetVisualizeCascades(bool bVisualize);

	private:
		void InitPipeline();
		void CreateShadowMapDistribution(const Ref<CommandBuffer>& cmd, uint32_t windowSize, uint32_t filterSize);

	private:
		Ref<PipelineGraphics> m_Pipeline;
		Ref<Shader> m_FragmentShader;
		Ref<Image> m_ResultImage;
		Ref<Buffer> m_CameraViewDataBuffer;
		Ref<Image> m_ShadowMapDistribution; // For soft shadows
		ShaderDefines m_ShaderDefines;
		bool bSoftShadows = false;
		bool bVisualizeCascades = false;
		bool bRequestedToCreateShadowMapDistribution = bSoftShadows;
	};
}
