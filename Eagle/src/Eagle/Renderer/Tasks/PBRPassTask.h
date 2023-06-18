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

	private:
		struct ConstantKernelInfo
		{
			uint32_t PointLightsCount = 0;
			uint32_t SpotLightsCount = 0;
			bool bHasDirLight = false;
			bool bHasIrradiance = false;

			bool operator== (const ConstantKernelInfo& other) const
			{
				return PointLightsCount == other.PointLightsCount &&
					SpotLightsCount == other.SpotLightsCount &&
					bHasDirLight == other.bHasDirLight &&
					bHasIrradiance == other.bHasIrradiance;
			}

			bool operator!= (const ConstantKernelInfo& other) const
			{
				return !((*this) == other);
			}
		};

		void RecreatePipeline(const ConstantKernelInfo& lightsInfo);
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
		ConstantKernelInfo m_KernelInfo;

		bool bSoftShadows = false;
		bool bVisualizeCascades = false;
		bool bRequestedToCreateShadowMapDistribution = bSoftShadows;
	};
}
