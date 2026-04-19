#include "egpch.h"
#include "BloomPassTask.h"

#include "Eagle/Renderer/SceneRenderer.h"
#include "Eagle/Renderer/VidWrappers/RenderCommandManager.h"
#include "Eagle/Renderer/VidWrappers/PipelineCompute.h"
#include "Eagle/Renderer/VidWrappers/Sampler.h"
#include "Eagle/Renderer/VidWrappers/Texture.h"

#include "Eagle/Asset/Asset.h"

#include "Eagle/Debug/CPUTimings.h"
#include "Eagle/Debug/GPUTimings.h"

namespace Eagle
{
	BloomPassTask::BloomPassTask(SceneRenderer& renderer)
		: RendererTask(renderer)
	{
		InitPipeline();

		m_MipViews.resize(16);
		std::fill(m_MipViews.begin(), m_MipViews.end(), ImageView{});
		const uint32_t mipsCount = m_Renderer.GetHDROutput()->GetMipsCount();
		m_BloomSampler = Sampler::Create(FilterMode::Bilinear, AddressMode::Clamp, CompareOperation::Never, 0.f, float(mipsCount - 1), 1.f);
		m_DirtSampler = Sampler::Create(FilterMode::Bilinear, AddressMode::Clamp, CompareOperation::Never, 0.f, 0.f, 1.f);

		for (uint32_t mip = 0; mip < mipsCount; ++mip)
			m_MipViews[mip] = ImageView{ mip };
	}

	void BloomPassTask::RecordCommandBuffer(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "Bloom Pass");
		EG_CPU_TIMING_SCOPED("Bloom Pass");

		auto& stats = m_Renderer.GetStats();
		const auto& input = m_Renderer.GetHDROutput();

		const uint32_t mipCount = input->GetMipsCount();
		const glm::uvec2 inputSize = input->GetSize();
		const ImageLayout inputLayout = input->GetLayout();
		glm::uvec2 mipSize = inputSize >> 1u;

		const auto& bloomSettings = m_Renderer.GetOptions_RT().BloomSettings;
		uint32_t lastDownscaledMip = 0;

		// Downscale
		{
			EG_GPU_TIMING_SCOPED(cmd, "Bloom Pass Downscale");
			EG_CPU_TIMING_SCOPED("Bloom Pass Downscale");
			struct PushConstants
			{
				glm::vec4 Threshold; // x -> threshold, yzw -> (threshold - knee, 2.0 * knee, 0.25 * knee)
				glm::uvec2 Size;
				glm::vec2 TexelSize;
				uint32_t  MipLevel;
				uint32_t  bUseThreshold;
			} pushData;
			pushData.Threshold = glm::vec4(bloomSettings.Threshold, bloomSettings.Threshold - bloomSettings.Knee, 2.f * bloomSettings.Knee, 0.25f * bloomSettings.Knee);

			const glm::uvec3 groupSize = m_DownscalePipeline->GetWorkGroupSize();

			m_DownscalePipeline->SetImageSampler(input, m_BloomSampler, 0, 0);
			m_DownscalePipeline->SetImageArray(input, m_MipViews, 0, 1);
			cmd->TransitionLayout(input, inputLayout, ImageLayoutType::StorageImage);

			for (uint32_t mip = 0; mip < mipCount - 1; ++mip)
			{
				const glm::uvec2 numGroups = CalcNumGroups(mipSize, groupSize);
				if (glm::min(numGroups.x, numGroups.y) == 0)
					break;

				pushData.Size = mipSize;
				pushData.TexelSize = glm::vec2(1.f / mipSize.x, 1.f / mipSize.y);
				pushData.MipLevel = mip;
				pushData.bUseThreshold = uint32_t(mip == 0);

				++stats.Dispatches;
				cmd->Dispatch(m_DownscalePipeline, numGroups.x, numGroups.y, 1, &pushData);
				cmd->TransitionLayout(input, m_MipViews[mip], ImageLayoutType::StorageImage, ImageLayoutType::StorageImage);
				cmd->TransitionLayout(input, m_MipViews[mip + 1], ImageLayoutType::StorageImage, ImageLayoutType::StorageImage);

				mipSize >>= 1u;
				lastDownscaledMip = mip + 1;
			}
		}

		// Upscale
		{
			EG_GPU_TIMING_SCOPED(cmd, "Bloom Pass Upscale");
			EG_CPU_TIMING_SCOPED("Bloom Pass Upscale");

			struct PushConstants
			{
				glm::uvec2 Size = glm::uvec2(0);
				glm::vec2 TexelSize = glm::vec2(0);
				uint32_t MipLevel = 0;
				float BloomIntensity = 1.f;
				float DirtIntensity = 1.f;
				uint32_t UseDirt = 0;
			} pushData;

			const bool bUseDirt = bloomSettings.Dirt.operator bool();

			pushData.BloomIntensity = bloomSettings.Intensity;
			pushData.DirtIntensity = bloomSettings.DirtIntensity;
			pushData.UseDirt = uint32_t(bUseDirt);

			const auto& dirtTexture = bUseDirt ? bloomSettings.Dirt->GetTexture() : Texture2D::DummyTexture;

			m_UpscalePipeline->SetImageSampler(input, m_BloomSampler, 0, 0);
			m_UpscalePipeline->SetImageArray(input, m_MipViews, 0, 1);
			m_UpscalePipeline->SetImageSampler(dirtTexture->GetImage(), m_DirtSampler, 0, 2);

			const glm::uvec3 groupSize = m_UpscalePipeline->GetWorkGroupSize();
			for (uint32_t mip = lastDownscaledMip; mip >= 1 ; --mip)
			{
				mipSize.x = uint32_t(glm::max(1.0, glm::floor(float(inputSize.x) / glm::pow(2.0, mip - 1))));
				mipSize.y = uint32_t(glm::max(1.0, glm::floor(float(inputSize.y) / glm::pow(2.0, mip - 1))));

				const glm::uvec2 numGroups = CalcNumGroups(mipSize, groupSize);
				if (glm::min(numGroups.x, numGroups.y) == 0)
					break;

				pushData.Size = mipSize;
				pushData.TexelSize = glm::vec2(1.f / mipSize.x, 1.f / mipSize.y);
				pushData.MipLevel = mip;

				++stats.Dispatches;
				cmd->Dispatch(m_UpscalePipeline, numGroups.x, numGroups.y, 1, &pushData);
				cmd->TransitionLayout(input, m_MipViews[mip], ImageLayoutType::StorageImage, ImageLayoutType::StorageImage);
				cmd->TransitionLayout(input, m_MipViews[mip - 1], ImageLayoutType::StorageImage, ImageLayoutType::StorageImage);
			}
		}

		cmd->TransitionLayout(input, ImageLayoutType::StorageImage, inputLayout);
	}

	void BloomPassTask::OnResize(const glm::uvec2 size)
	{
		std::fill(m_MipViews.begin(), m_MipViews.end(), ImageView{});
		const uint32_t mipsCount = m_Renderer.GetHDROutput()->GetMipsCount();
		m_BloomSampler = Sampler::Create(FilterMode::Bilinear, AddressMode::Clamp, CompareOperation::Never, 0.f, float(mipsCount - 1), 1.f);
		for (uint32_t mip = 0; mip < mipsCount; ++mip)
			m_MipViews[mip] = ImageView{ mip };
	}

	void BloomPassTask::InitPipeline()
	{
		// Downscale pipeline
		{
			PipelineComputeState state;
			state.ComputeShader = Shader::Create("bloom/bloom_downscale.comp", ShaderType::Compute);
			m_DownscalePipeline = PipelineCompute::Create(state);
		}
		// Upscale pipeline
		{
			PipelineComputeState state;
			state.ComputeShader = Shader::Create("bloom/bloom_upscale.comp", ShaderType::Compute);
			m_UpscalePipeline = PipelineCompute::Create(state);
		}
	}
}
