#include "egpch.h"
#include "GTAOTask.h"
#include "Eagle/Core/Application.h"

#include "Eagle/Renderer/SceneRenderer.h"
#include "Eagle/Renderer/RenderManager.h"
#include "Eagle/Renderer/VidWrappers/RenderCommandManager.h"

#include "Eagle/Debug/CPUTimings.h"
#include "Eagle/Debug/GPUTimings.h"

namespace Eagle
{
	GTAOTask::GTAOTask(SceneRenderer& renderer)
		: RendererTask(renderer)
	{
		const auto& gtao = m_Renderer.GetOptions_RT().GTAOSettings;
		m_Quality = gtao.Quality;
		bHalfRes = gtao.bHalfRes;

		InitResources();
		InitPipeline();
	}

	void GTAOTask::RecordCommandBuffer(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "GTAO");
		EG_CPU_TIMING_SCOPED("GTAO");

		auto& gBuffer = m_Renderer.GetGBuffer();

		const ImageLayout oldDepthLayout = gBuffer.Depth->GetLayout();
		cmd->TransitionLayout(gBuffer.Depth, oldDepthLayout, ImageReadAccess::PixelShaderRead);

		const ImageLayout oldNormalsLayout = gBuffer.Normals->GetLayout();
		cmd->TransitionLayout(gBuffer.Normals, oldNormalsLayout, ImageReadAccess::PixelShaderRead);

		// Prepare constants
		{
			const glm::mat4& projMatrix = m_Renderer.GetProjectionMatrix();
			const auto& gtaoSettings = m_Renderer.GetOptions_RT().GTAOSettings;

			auto& consts = m_Constants;
			consts.ViewportSize = m_PassSize;
			consts.ViewportPixelSize = glm::vec2(1) / glm::vec2(consts.ViewportSize);

			const float tanHalfFOVY = m_Renderer.IsProjectionFlipped() ? -1.0f / projMatrix[1][1] : 1.0f / projMatrix[1][1];
			const float tanHalfFOVX = 1.0f / projMatrix[0][0];
			consts.CameraTanHalfFOV = { tanHalfFOVX, tanHalfFOVY };

			consts.NDCToViewMul = { consts.CameraTanHalfFOV.x * 2.0f, consts.CameraTanHalfFOV.y * -2.0f };
			consts.NDCToViewAdd = { consts.CameraTanHalfFOV.x * -1.0f, consts.CameraTanHalfFOV.y * 1.0f };

			consts.NDCToViewMul_x_PixelSize = { consts.NDCToViewMul.x * consts.ViewportPixelSize.x, consts.NDCToViewMul.y * consts.ViewportPixelSize.y };

			consts.EffectRadius = gtaoSettings.Radius;

			consts.EffectFalloffRange = gtaoSettings.FalloffRange;
			consts.DenoiseBlurBeta = (1.2f);

			consts.RadiusMultiplier = 1;
			consts.SampleDistributionPower = 2;
			consts.ThinOccluderCompensation = 0;
			consts.FinalValuePower = 2.2f;
			consts.DepthMIPSamplingOffset = 3.3f;
			consts.NoiseIndex = 0;// (RenderManager::GetFrameNumber_RT() % 64);

			consts.CameraPlanes.x = m_Renderer.GetZNear();
			consts.CameraPlanes.y = m_Renderer.GetZFar();
			consts.FinalPass = 0u;

			static_assert(sizeof(GTAOConstants) <= 128);
		}
		
		m_PingPong = 0;
		Downsample(cmd);
		GTAO(cmd);
		Denoiser(cmd);
		if (bHalfRes)
		{
			Interleave(cmd);
		}

		cmd->TransitionLayout(m_Denoised, m_Denoised->GetLayout(), ImageReadAccess::PixelShaderRead);
		cmd->TransitionLayout(gBuffer.Depth, gBuffer.Depth->GetLayout(), oldDepthLayout);
		cmd->TransitionLayout(gBuffer.Normals, gBuffer.Normals->GetLayout(), oldNormalsLayout);
	}

	void GTAOTask::OnResize(glm::uvec2 size)
	{
		if (bHalfRes)
			m_PassSize = glm::max(size / 2u, glm::uvec2(1u));
		else
			m_PassSize = glm::max(size, glm::uvec2(1u));

		if (bHalfRes)
		{
			m_Depth->Resize({ m_PassSize, 1u });
			m_DownsamplePipeline->Resize(m_PassSize);
		}

		m_Denoised->Resize({ size, 1u });
		for (uint32_t i = 0; i < 2; ++i)
			m_GTAOPassImage[i]->Resize({ m_PassSize, 1u });
		m_GTAOEdgesImage->Resize({ m_PassSize, 1u });
	}

	void GTAOTask::Downsample(const Ref<CommandBuffer>& cmd)
	{
		const auto& gBuffer = m_Renderer.GetGBuffer();
		if (bHalfRes)
		{
			EG_GPU_TIMING_SCOPED(cmd, "GTAO. Downsample");
			EG_CPU_TIMING_SCOPED("GTAO. Downsample");
			auto& stats = m_Renderer.GetStats();

			m_DownsamplePipeline->SetImageSampler(gBuffer.Depth, Sampler::PointSampler, 0, 0);

			cmd->BeginGraphics(m_DownsamplePipeline);
			cmd->Draw(6, 0);
			cmd->EndGraphics();
			++stats.DrawCalls;
		}
		else
		{
			m_Depth = gBuffer.Depth;
		}
	}

	void GTAOTask::GTAO(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "GTAO. AO");
		EG_CPU_TIMING_SCOPED("GTAO. AO");

		m_GTAOPipeline->SetImageSampler(m_Depth, Sampler::PointSamplerClamp, 0, 0);
		m_GTAOPipeline->SetImageSampler(m_Renderer.GetGBuffer().Normals, Sampler::PointSamplerClamp, 0, 1);
		m_GTAOPipeline->SetImageSampler(RenderManager::GetHilbertCurve()->GetImage(), Sampler::PointSampler, 0, 2);
		m_GTAOPipeline->SetBuffer(m_Renderer.GetCameraMatricesBuffer(), 0, 3);
		m_GTAOPipeline->SetImage(m_GTAOEdgesImage, 0, 4);
		m_GTAOPipeline->SetImage(m_GTAOPassImage[0], 0, 5);

		cmd->TransitionLayout(m_GTAOEdgesImage, m_GTAOEdgesImage->GetLayout(), ImageLayoutType::StorageImage);
		cmd->TransitionLayout(m_GTAOPassImage[0], m_GTAOPassImage[0]->GetLayout(), ImageLayoutType::StorageImage);
		cmd->Barrier(m_Depth);

		const glm::uvec3 groupSize = m_GTAOPipeline->GetWorkGroupSize();
		const glm::uvec2 numGroups = CalcNumGroups(m_PassSize, groupSize);
		cmd->Dispatch(m_GTAOPipeline, numGroups, &m_Constants);

		cmd->TransitionLayout(m_GTAOEdgesImage, m_GTAOEdgesImage->GetLayout(), ImageReadAccess::PixelShaderRead);
		cmd->TransitionLayout(m_GTAOPassImage[0], m_GTAOPassImage[0]->GetLayout(), ImageLayoutType::StorageImage);

		auto& stats = m_Renderer.GetStats();
		++stats.Dispatches;
	}

	void GTAOTask::Denoiser(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "GTAO. Denoiser");
		EG_CPU_TIMING_SCOPED("GTAO. Denoiser");

		auto& stats = m_Renderer.GetStats();

		const glm::uvec3 groupSize = m_DenoiserPipeline->GetWorkGroupSize();
		const glm::uvec2 numGroups = glm::uvec2((m_PassSize.x + (groupSize.x * 2u) - 1u) / (groupSize.x * 2u), (m_PassSize.y + groupSize.y - 1u) / groupSize.y);

		const uint32_t numPasses = glm::max(1u, m_Renderer.GetOptions_RT().GTAOSettings.Quality.NumberOfBlurPasses);
		for (uint32_t i = 0; i < numPasses; ++i)
		{
			const bool bFinalPass = i == (numPasses - 1);
			m_Constants.FinalPass = bFinalPass ? 1u : 0u;

			auto& input = m_GTAOPassImage[m_PingPong];

			// If in half res mode, then output into the temp image, since we have the upscale pass
			auto& output = bFinalPass && !bHalfRes ? m_Denoised : m_GTAOPassImage[1u - m_PingPong];

			cmd->TransitionLayout(input, input->GetLayout(), ImageReadAccess::NonPixelShaderRead);
			cmd->TransitionLayout(output, output->GetLayout(), ImageLayoutType::StorageImage);

			// TODO: Remove/Fix when manual descriptors system is implemented
			m_DenoiserPipeline->ResetDescriptors();

			m_DenoiserPipeline->SetImageSampler(input, Sampler::PointSamplerClamp, 0, 0);
			m_DenoiserPipeline->SetImageSampler(m_GTAOEdgesImage, Sampler::PointSamplerClamp, 0, 1);
			m_DenoiserPipeline->SetImageSampler(m_Depth, Sampler::PointSamplerClamp, 0, 2);
			m_DenoiserPipeline->SetImage(output, 0, 3);

			cmd->Dispatch(m_DenoiserPipeline, numGroups, &m_Constants);
			++stats.Dispatches;

			m_PingPong = 1u - m_PingPong;
		}

	}

	void GTAOTask::Interleave(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "GTAO. Upscale to native");
		EG_CPU_TIMING_SCOPED("GTAO. Upscale to native");

		const glm::ivec2 size = m_Renderer.GetViewportSize();
		auto& input = m_GTAOPassImage[1u - m_PingPong];
		auto& output = m_Denoised;

		cmd->TransitionLayout(input, input->GetLayout(), ImageReadAccess::NonPixelShaderRead);
		cmd->TransitionLayout(output, output->GetLayout(), ImageLayoutType::StorageImage);

		m_InterleavePipeline->SetImageSampler(input, Sampler::BilinearSamplerClamp, 0, 0);
		m_InterleavePipeline->SetImageSampler(m_GTAOEdgesImage, Sampler::PointSamplerClamp, 0, 1);
		m_InterleavePipeline->SetImage(output, 0, 2);

		const glm::uvec3 groupSize = m_InterleavePipeline->GetWorkGroupSize();
		const glm::uvec2 numGroups = CalcNumGroups(size, groupSize);
		cmd->Dispatch(m_InterleavePipeline, numGroups, &size);
	}

	void GTAOTask::InitResources()
	{
		const glm::uvec3 viewportSize = glm::uvec3(m_Renderer.GetViewportSize(), 1u);
		m_PassSize = bHalfRes ? glm::max(viewportSize / 2u, glm::uvec3(1u)) : viewportSize;

		if (bHalfRes)
		{
			ImageSpecifications depthSpecs;
			depthSpecs.Size = glm::uvec3(m_PassSize, 1u);
			depthSpecs.Usage = ImageUsage::Sampled | ImageUsage::ColorAttachment;
			depthSpecs.Format = ImageFormat::R32_Float;
			m_Depth = Image::Create(depthSpecs, "GTAO_Depth");
		}
		else
		{
			m_Depth.reset();
		}

		ImageSpecifications specs;
		specs.Format = ImageFormat::R8_UNorm;
		specs.Usage = ImageUsage::Sampled | ImageUsage::Storage;
		specs.Size = glm::uvec3(m_PassSize, 1u);
		m_GTAOPassImage[0] = Image::Create(specs, "GTAO_Pass[0]");
		m_GTAOPassImage[1] = Image::Create(specs, "GTAO_Pass[1]");
		m_GTAOEdgesImage = Image::Create(specs, "GTAO_Pass_Edges");

		specs.Usage = ImageUsage::Sampled | ImageUsage::Storage;
		specs.Size = viewportSize;
		m_Denoised = Image::Create(specs, "GTAO_Denoised");
	}

	void GTAOTask::InitPipeline()
	{
		// Downsample
		if (bHalfRes)
		{
			const glm::uvec2 size = m_Depth->GetSize();

			ColorAttachment depthAttachment;
			depthAttachment.Image = m_Depth;
			depthAttachment.ClearOperation = ClearOperation::DontCare;
			depthAttachment.InitialLayout = ImageLayoutType::Unknown;
			depthAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;

			PipelineGraphicsState downsamplesState;
			downsamplesState.ColorAttachments.push_back(depthAttachment);
			downsamplesState.Size = size;
			downsamplesState.VertexShader = Shader::Create("quad.vert", ShaderType::Vertex);
			downsamplesState.FragmentShader = Shader::Create("XeGTAO/downsample.frag", ShaderType::Fragment);
			downsamplesState.CullMode = CullMode::Back;

			m_DownsamplePipeline = PipelineGraphics::Create(downsamplesState);

			PipelineComputeState state;
			state.ComputeShader = Shader::Create("XeGTAO/interleave.comp", ShaderType::Compute);
			m_InterleavePipeline = PipelineCompute::Create(state);
		}
		else
		{
			m_DownsamplePipeline.reset();
			m_InterleavePipeline.reset();
		}

		// GTAO Pipeline
		{
			ShaderSpecializationInfo constants;
			constants.MapEntries.push_back({0, 0, sizeof(uint32_t)});
			constants.MapEntries.push_back({1, sizeof(uint32_t), sizeof(uint32_t)});
			constants.Data = &m_Quality;
			constants.Size = sizeof(m_Quality);
			
			PipelineComputeState state;
			state.ComputeSpecializationInfo = constants;
			state.ComputeShader = Shader::Create("XeGTAO/gtao.comp", ShaderType::Compute);

			m_GTAOPipeline = PipelineCompute::Create(state);
		}

		// Denoiser pipeline
		{
			PipelineComputeState state;
			state.ComputeShader = Shader::Create("XeGTAO/denoiser.comp", ShaderType::Compute);

			m_DenoiserPipeline = PipelineCompute::Create(state);
		}
	}
}
