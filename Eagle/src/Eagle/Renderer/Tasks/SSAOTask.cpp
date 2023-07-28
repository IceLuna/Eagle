#include "egpch.h"
#include "SSAOTask.h"
#include "Eagle/Renderer/VidWrappers/RenderCommandManager.h"
#include "Eagle/Renderer/SceneRenderer.h"
#include "Eagle/Renderer/RenderManager.h"

#include "Eagle/Core/Random.h"

#include "Eagle/Debug/CPUTimings.h"
#include "Eagle/Debug/GPUTimings.h"

namespace Eagle
{
	static uint16_t ToFloat16(float value)
	{
		uint32_t fltInt32;
		uint16_t fltInt16;

		memcpy(&fltInt32, &value, sizeof(float));

		fltInt16 = (fltInt32 >> 31) << 5;
		unsigned short tmp = (fltInt32 >> 23) & 0xff;
		tmp = (tmp - 0x70) & ((unsigned int)((int)(0x70 - tmp) >> 4) >> 27);
		fltInt16 = (fltInt16 | tmp) << 10;
		fltInt16 |= (fltInt32 >> 13) & 0x3ff;

		return fltInt16;
	}

	static constexpr uint32_t s_NoiseTextureSize = 4;

	SSAOTask::SSAOTask(SceneRenderer& renderer) : RendererTask(renderer)
	{
		ImageSpecifications specs;
		specs.Format = ImageFormat::R8_UNorm;
		specs.Usage = ImageUsage::Sampled | ImageUsage::Storage;
		specs.Size = glm::uvec3(m_Renderer.GetViewportSize(), 1);
		m_SSAOPassImage = Image::Create(specs, "SSAO_Pass");
		m_ResultImage = Image::Create(specs, "SSAO_Result");

		const auto& settings = renderer.GetOptions_RT().SSAOSettings;
		m_SamplesCount = settings.GetNumberOfSamples();
		InitPipeline();
		GenerateKernels();

		const uint32_t samples = settings.GetNumberOfSamples();

		BufferSpecifications bufferSpecs;
		bufferSpecs.Size = samples * sizeof(glm::vec3);
		bufferSpecs.Layout = BufferLayoutType::StorageBuffer;
		bufferSpecs.Usage = BufferUsage::StorageBuffer | BufferUsage::TransferDst;
		m_SamplesBuffer = Buffer::Create(bufferSpecs, "SSAO_Samples");

		ImageSpecifications noiseSpecs{};
		noiseSpecs.Usage = ImageUsage::TransferDst | ImageUsage::Sampled;
		noiseSpecs.Size = glm::uvec3(s_NoiseTextureSize, s_NoiseTextureSize, 1);
		noiseSpecs.Format = ImageFormat::R16G16_Float;
		m_NoiseImage = Image::Create(noiseSpecs, "SSAO_Noise");
	}

	void SSAOTask::RecordCommandBuffer(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "SSAO Pass");
		EG_CPU_TIMING_SCOPED("SSAO Pass");

		const auto& settings = m_Renderer.GetOptions_RT().SSAOSettings;
		const uint32_t samples = settings.GetNumberOfSamples();

		if (bUploadNoise)
		{
			struct hvec2
			{
				uint16_t x;
				uint16_t y;
			};

			constexpr uint32_t noisePixels = s_NoiseTextureSize * s_NoiseTextureSize;
			std::vector<hvec2> noise; // Rotation around Z. Hence Z = 0.f and we don't need to store it
			noise.reserve(noisePixels);
			for (uint32_t i = 0; i < noisePixels; ++i)
			{
				hvec2 data;
				data.x = ToFloat16(Random::Float() * 2.f - 1.f);
				data.y = ToFloat16(Random::Float() * 2.f - 1.f);
				noise.emplace_back(data);
			}

			cmd->Write(m_NoiseImage, noise.data(), noise.size() * sizeof(hvec2), ImageLayoutType::Unknown, ImageReadAccess::PixelShaderRead);
			cmd->TransitionLayout(m_NoiseImage, ImageReadAccess::PixelShaderRead, ImageReadAccess::PixelShaderRead);
			bUploadNoise = false;
		}

		if (bKernelsDirty)
		{
			const size_t newBufferSize = samples * sizeof(glm::vec3);

			// Resize if needed
			m_SamplesBuffer->Resize(newBufferSize);

			cmd->Write(m_SamplesBuffer, m_Samples.data(), newBufferSize, 0, BufferLayoutType::Unknown, BufferLayoutType::StorageBuffer);
			cmd->StorageBufferBarrier(m_SamplesBuffer);

			bKernelsDirty = false;
		}
	
		struct PushConstants
		{
			glm::mat4 Projection;
			glm::vec3 ViewRow1;
			uint32_t unused;
			glm::vec3 ViewRow2;
			float Radius;
			glm::vec3 ViewRow3;
			float Bias;
			glm::vec2 NoiseScale;
			glm::ivec2 Size;
		} pushData;
		static_assert(sizeof(PushConstants) <= 128u);
		const glm::vec2 viewportSize = m_ResultImage->GetSize();

		constexpr uint32_t s_TileSize = 8;
		const glm::uvec2 numGroupds = { glm::ceil(viewportSize.x / float(s_TileSize)), glm::ceil(viewportSize.y / float(s_TileSize)) };

		// AO
		{
			EG_GPU_TIMING_SCOPED(cmd, "SSAO. AO");
			EG_CPU_TIMING_SCOPED("SSAO. AO");

			const auto& view = m_Renderer.GetViewMatrix();
			pushData.Projection = m_Renderer.GetProjectionMatrix();
			pushData.ViewRow1 = view[0];
			pushData.ViewRow2 = view[1];
			pushData.ViewRow3 = view[2];
			pushData.Size = m_ResultImage->GetSize();

			// Tile noise texture over screen, based on screen dimensions divided by noise size
			pushData.NoiseScale = viewportSize / float(s_NoiseTextureSize);
			pushData.Radius = settings.GetRadius();
			pushData.Bias = settings.GetBias();

			auto& gbuffer = m_Renderer.GetGBuffer();
			m_Pipeline->SetImageSampler(gbuffer.Geometry_Shading_Normals, Sampler::PointSamplerClamp, 0, 0);
			m_Pipeline->SetImageSampler(gbuffer.Depth, Sampler::PointSamplerClamp, 0, 1);
			m_Pipeline->SetImageSampler(m_NoiseImage, Sampler::PointSampler, 0, 2);
			m_Pipeline->SetBuffer(m_SamplesBuffer, 0, 3);
			m_Pipeline->SetImage(m_SSAOPassImage, 0, 4);

			cmd->TransitionLayout(m_SSAOPassImage, m_SSAOPassImage->GetLayout(), ImageLayoutType::StorageImage);
			cmd->TransitionLayout(gbuffer.Depth, gbuffer.Depth->GetLayout(), ImageReadAccess::PixelShaderRead);

			cmd->Dispatch(m_Pipeline, numGroupds.x, numGroupds.y, 1, &pushData);

			cmd->TransitionLayout(m_SSAOPassImage, m_SSAOPassImage->GetLayout(), ImageReadAccess::PixelShaderRead);
			cmd->TransitionLayout(gbuffer.Depth, gbuffer.Depth->GetLayout(), ImageLayoutType::DepthStencilWrite);
		}

		// Blur
		{
			EG_GPU_TIMING_SCOPED(cmd, "SSAO. Blur");
			EG_CPU_TIMING_SCOPED("SSAO. Blur");

			struct BlurPushData
			{
				glm::ivec2 Size;
				glm::vec2 TexelSize;
			} blurPushData;
			blurPushData.Size = pushData.Size;
			blurPushData.TexelSize = 1.f / viewportSize;

			m_BlurPipeline->SetImageSampler(m_SSAOPassImage, Sampler::PointSamplerClamp, 0, 0);
			m_BlurPipeline->SetImage(m_ResultImage, 0, 1);

			cmd->TransitionLayout(m_ResultImage, m_ResultImage->GetLayout(), ImageLayoutType::StorageImage);
			cmd->Dispatch(m_BlurPipeline, numGroupds.x, numGroupds.y, 1, &blurPushData);
			cmd->TransitionLayout(m_ResultImage, m_ResultImage->GetLayout(), ImageReadAccess::PixelShaderRead);
		}
	}
	
	void SSAOTask::InitPipeline()
	{
		// SSAO pipeline
		{
			ShaderSpecializationInfo constants;
			constants.MapEntries.push_back({ 0, 0, sizeof(uint32_t) });
			constants.Data = &m_SamplesCount;
			constants.Size = sizeof(uint32_t);

			PipelineComputeState state;
			state.ComputeShader = Shader::Create("assets/shaders/ssao.comp", ShaderType::Compute);
			state.ComputeSpecializationInfo = constants;
			m_Pipeline = PipelineCompute::Create(state);
		}

		// Blur pipeline
		{
			PipelineComputeState state;
			state.ComputeShader = Shader::Create("assets/shaders/ssao_blur.comp", ShaderType::Compute);
			m_BlurPipeline = PipelineCompute::Create(state);
		}
	}
	
	void SSAOTask::GenerateKernels()
	{
		m_Samples.resize(m_SamplesCount);

		for (uint32_t i = 0; i < m_SamplesCount; ++i)
		{
			// Hemisphere
			glm::vec3 sample(
				Random::Float() * 2.f - 1.f,
				Random::Float() * 2.f - 1.f,
				Random::Float()
			);

			sample = glm::normalize(sample) * Random::Float();

			// To distribute more kernel samples closer to the origin
			float scale = float(i) / float(m_SamplesCount);
			scale = glm::mix(0.1f, 1.f, scale * scale);

			m_Samples[i] = (sample * scale);
		}

		bKernelsDirty = true;
	}
}
