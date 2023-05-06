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
		const auto& settings = renderer.GetOptions_RT().SSAOSettings;

		InitPipeline();
		GenerateKernels(settings);

		const uint32_t samples = settings.GetNumberOfSamples();

		BufferSpecifications bufferSpecs;
		bufferSpecs.Size = samples * sizeof(glm::vec3);
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
			glm::mat4 ProjectionInv;
			glm::vec3 ViewRow1;
			uint32_t Samples;
			glm::vec3 ViewRow2;
			float Radius;
			glm::vec3 ViewRow3;
			float Bias;
			glm::vec2 NoiseScale;
		} pushData;
		static_assert(sizeof(PushConstants) <= 128u);

		const auto& view = m_Renderer.GetViewMatrix();
		pushData.ProjectionInv = m_Renderer.GetProjectionMatrix();
		pushData.ViewRow1 = view[0];
		pushData.ViewRow2 = view[1];
		pushData.ViewRow3 = view[2];
		
		const glm::vec2 viewportSize = m_Renderer.GetViewportSize();
		// Tile noise texture over screen, based on screen dimensions divided by noise size
		pushData.NoiseScale = viewportSize / float(s_NoiseTextureSize);
		pushData.Samples = samples;
		pushData.Radius = settings.GetRadius();
		pushData.Bias = settings.GetBias();

		auto& gbuffer = m_Renderer.GetGBuffer();
		m_Pipeline->SetImageSampler(gbuffer.Albedo, Sampler::PointSamplerClamp, 0, 0);
		m_Pipeline->SetImageSampler(gbuffer.ShadingNormal, Sampler::PointSamplerClamp, 0, 1);
		m_Pipeline->SetImageSampler(gbuffer.Depth, Sampler::PointSamplerClamp, 0, 2);
		m_Pipeline->SetImageSampler(m_NoiseImage, Sampler::PointSampler, 0, 3);
		m_Pipeline->SetBuffer(m_SamplesBuffer, 0, 4);

		m_BlurPipeline->SetImageSampler(m_SSAOPassImage, Sampler::PointSamplerClamp, 0, 0);

		cmd->TransitionLayout(gbuffer.Depth, gbuffer.Depth->GetLayout(), ImageReadAccess::PixelShaderRead);
		cmd->BeginGraphics(m_Pipeline);
		cmd->SetGraphicsRootConstants(nullptr, &pushData);
		cmd->Draw(6, 0);
		cmd->EndGraphics();
		cmd->TransitionLayout(gbuffer.Depth, gbuffer.Depth->GetLayout(), ImageLayoutType::DepthStencilWrite);


		const glm::vec2 texelSize = 1.f / viewportSize;
		cmd->BeginGraphics(m_BlurPipeline);
		cmd->SetGraphicsRootConstants(nullptr, &texelSize);
		cmd->Draw(6, 0);
		cmd->EndGraphics();
	}
	
	void SSAOTask::InitPipeline()
	{
		ImageSpecifications specs;
		specs.Format = ImageFormat::R8_UNorm;
		specs.Usage = ImageUsage::Sampled | ImageUsage::ColorAttachment;
		specs.Size = glm::uvec3(m_Renderer.GetViewportSize(), 1);
		m_SSAOPassImage = Image::Create(specs, "SSAO_Pass");
		m_ResultImage = Image::Create(specs, "SSAO_Result");

		ColorAttachment attachment;
		attachment.Image = m_SSAOPassImage;
		attachment.ClearOperation = ClearOperation::DontCare;
		attachment.InitialLayout = ImageLayoutType::Unknown;
		attachment.FinalLayout = ImageReadAccess::PixelShaderRead;

		PipelineGraphicsState state;
		state.ColorAttachments.push_back(attachment);
		state.Size = specs.Size;
		state.VertexShader = ShaderLibrary::GetOrLoad("assets/shaders/quad.vert", ShaderType::Vertex);
		state.FragmentShader = Shader::Create("assets/shaders/ssao.frag");
		state.CullMode = CullMode::Back;

		m_Pipeline = PipelineGraphics::Create(state);

		attachment.Image = m_ResultImage;
		state.ColorAttachments[0] = attachment;
		state.FragmentShader = Shader::Create("assets/shaders/ssao_blur.frag");
		m_BlurPipeline = PipelineGraphics::Create(state);
	}
	
	void SSAOTask::GenerateKernels(const SSAOSettings& settings)
	{
		const uint32_t samples = settings.GetNumberOfSamples();
		m_Samples.clear();
		m_Samples.reserve(samples);

		for (uint32_t i = 0; i < samples; ++i)
		{
			// Hemisphere
			glm::vec3 sample(
				Random::Float() * 2.f - 1.f,
				Random::Float() * 2.f - 1.f,
				Random::Float()
			);

			sample = glm::normalize(sample) * Random::Float();

			// To distribute more kernel samples closer to the origin
			float scale = float(i) / float(samples);
			scale = glm::mix(0.1f, 1.f, scale * scale);

			m_Samples.push_back(sample * scale);
		}

		bKernelsDirty = true;
	}
}
