#pragma once

#include "RendererTask.h"
#include "Eagle/Renderer/VidWrappers/PipelineGraphics.h"

namespace Eagle
{
	struct SpriteGeometryData;
	struct LitTextGeometryData;

	class TransparencyTask : public RendererTask
	{
	public:
		TransparencyTask(SceneRenderer& renderer);

		void RecordCommandBuffer(const Ref<CommandBuffer>& cmd) override;
		void OnResize(glm::uvec2 size) override
		{
			if (m_OITBuffer)
			{
				const size_t bufferSize = size_t(size.x * size.y * m_Layers) * s_Stride;
				m_OITBuffer->Resize(bufferSize);
			}

			m_CompositePipeline->Resize(size);
			m_MeshesDepthPipeline->Resize(size);
			m_MeshesColorPipeline->Resize(size);
			m_MeshesEntityIDPipeline->Resize(size);

			m_SpritesDepthPipeline->Resize(size);
			m_SpritesColorPipeline->Resize(size);
			m_SpritesEntityIDPipeline->Resize(size);

			m_TextDepthPipeline->Resize(size);
			m_TextColorPipeline->Resize(size);
			m_TextEntityIDPipeline->Resize(size);
		}

		void InitWithOptions(const SceneRendererSettings& settings) override;

	private:
		void RenderMeshesDepth(const Ref<CommandBuffer>& cmd);
		void RenderSpritesDepth(const Ref<CommandBuffer>& cmd, const SpriteGeometryData& spritesData);
		void RenderTextsDepth(const Ref<CommandBuffer>& cmd, const LitTextGeometryData& data);

		void RenderMeshesColor(const Ref<CommandBuffer>& cmd);
		void RenderSpritesColor(const Ref<CommandBuffer>& cmd, const SpriteGeometryData& spritesData);
		void RenderTextsColor(const Ref<CommandBuffer>& cmd, const LitTextGeometryData& data);

		void CompositePass(const Ref<CommandBuffer>& cmd);
		void RenderEntityIDs(const Ref<CommandBuffer>& cmd);

		void InitMeshPipelines();
		void InitSpritesPipelines();
		void InitTextsPipelines();
		void InitCompositePipelines();
		void InitEntityIDPipelines();
		void InitOITBuffer();

		bool SetSoftShadowsEnabled(bool bEnable);
		bool SetVisualizeCascades(bool bVisualize);
		bool SetCSMSmoothTransitionEnabled(bool bEnabled);
		bool SetStutterlessEnabled(bool bEnabled);
		bool SetFogEnabled(bool bEnabled);

		void RecreatePipeline(bool bUpdateDefines);

		struct ColorPushData
		{
			glm::vec3 CameraPos;
			float MaxReflectionLOD;
			glm::ivec2 Size;
			float MaxShadowDistance2; // Square of distance
			float CascadesSmoothTransitionAlpha;
			float IBLIntensity;
			uint32_t PointLights;
			uint32_t SpotLights;
			uint32_t HasDirLight;
		};

	private:
		Ref<PipelineGraphics> m_MeshesDepthPipeline;
		Ref<PipelineGraphics> m_MeshesColorPipeline;
		Ref<PipelineGraphics> m_MeshesEntityIDPipeline;

		Ref<PipelineGraphics> m_SpritesDepthPipeline;
		Ref<PipelineGraphics> m_SpritesColorPipeline;
		Ref<PipelineGraphics> m_SpritesEntityIDPipeline;

		Ref<PipelineGraphics> m_TextDepthPipeline;
		Ref<PipelineGraphics> m_TextColorPipeline;
		Ref<PipelineGraphics> m_TextEntityIDPipeline;

		Ref<PipelineGraphics> m_CompositePipeline;

		Ref<Shader> m_TransparencyColorShader;
		Ref<Shader> m_TransparencyDepthShader;
		Ref<Shader> m_TransparencyCompositeShader;

		Ref<Shader> m_TransparencyTextColorShader;
		Ref<Shader> m_TransparencyTextDepthShader;

		Ref<Buffer> m_OITBuffer;
		uint32_t m_Layers = 4u;
		PBRConstantsKernelInfo m_KernelInfo;

		ColorPushData m_ColorPushData;
		ShaderDefines m_ShaderDefines;

		uint64_t m_TexturesUpdatedFrames[RendererConfig::FramesInFlight] = { 0 };

		bool bSoftShadows = false;
		bool bVisualizeCascades = false;
		bool bStutterlessShaders = false;
		bool bFog = false;

		constexpr static size_t s_FormatSize = GetImageFormatBPP(ImageFormat::R32_UInt) / 8u;
		constexpr static size_t s_Uints = 3ull; // 3 uints. One of the - for storing depth; Rest - for storing color
		constexpr static size_t s_Stride = s_Uints * s_FormatSize;
	};
}
