#pragma once

#include "RendererTask.h"
#include "Eagle/Renderer/VidWrappers/PipelineCompute.h"

namespace Eagle
{
	class Buffer;

	class PBRPassTask : public RendererTask
	{
	public:
		PBRPassTask(SceneRenderer& renderer, const Ref<Image>& renderTo);

		void RecordCommandBuffer(const Ref<CommandBuffer>& cmd) override;

		virtual void InitWithOptions(const SceneRendererSettings& settings) override
		{
			bool bReloadShader = false;
			bReloadShader |= SetVisualizeCascades(settings.bVisualizeCascades);
			bReloadShader |= SetSoftShadowsEnabled(settings.bEnableSoftShadows);
			bReloadShader |= SetSSAOEnabled(settings.AO != AmbientOcclusion::None);
			bReloadShader |= SetCSMSmoothTransitionEnabled(settings.bEnableCSMSmoothTransition);

			if (bReloadShader)
				m_Shader->SetDefines(m_ShaderDefines);
		}

		const Ref<Buffer>& GetCameraBuffer() const { return m_CameraViewDataBuffer; }
		const Ref<Image>& GetSMDistribution() const { return m_ShadowMapDistribution; }

	private:
		void RecreatePipeline(const PBRConstantsKernelInfo& lightsInfo);
		void InitPipeline();
		void CreateShadowMapDistribution(const Ref<CommandBuffer>& cmd, uint32_t windowSize, uint32_t filterSize);

		bool SetSoftShadowsEnabled(bool bEnable);
		bool SetVisualizeCascades(bool bVisualize);
		bool SetSSAOEnabled(bool bEnabled);
		bool SetCSMSmoothTransitionEnabled(bool bEnabled);

	private:
		Ref<PipelineCompute> m_Pipeline;
		Ref<Shader> m_Shader;
		Ref<Image> m_ResultImage;
		Ref<Buffer> m_CameraViewDataBuffer;
		Ref<Image> m_ShadowMapDistribution; // For soft shadows
		ShaderDefines m_ShaderDefines;
		PBRConstantsKernelInfo m_KernelInfo;

		bool bSoftShadows = false;
		bool bVisualizeCascades = false;
		bool bRequestedToCreateShadowMapDistribution = bSoftShadows;
	};
}
