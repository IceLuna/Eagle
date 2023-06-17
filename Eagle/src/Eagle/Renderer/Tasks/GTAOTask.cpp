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
		const glm::uvec3 size = glm::max(glm::uvec3(m_Renderer.GetViewportSize(), 1u) / 2u, glm::uvec3(1u));
		m_HalfSize = size;
		m_HalfTexelSize = 1.f / glm::vec2(m_HalfSize);
		m_HalfNumGroups = { glm::ceil(size.x / float(s_TileSize)), glm::ceil(size.y / float(s_TileSize)) };

		ImageSpecifications depthSpecs;
		depthSpecs.Size = size;
		depthSpecs.Usage = ImageUsage::Sampled | ImageUsage::ColorAttachment | ImageUsage::TransferSrc;
		depthSpecs.Format = ImageFormat::R32_Float;
		m_HalfDepth = Image::Create(depthSpecs, "GTAO_HalfDepth");

		depthSpecs.Usage = ImageUsage::Sampled | ImageUsage::TransferDst;
		m_HalfDepthPrev = Image::Create(depthSpecs, "GTAO_HalfDepth_Prev");

		ImageSpecifications motionSpecs;
		motionSpecs.Size = size;
		motionSpecs.Usage = ImageUsage::Sampled | ImageUsage::ColorAttachment;
		motionSpecs.Format = ImageFormat::R16G16_Float;
		m_HalfMotion = Image::Create(motionSpecs, "GTAO_HalfMotion");

		ImageSpecifications specs;
		specs.Format = ImageFormat::R8_UNorm;
		specs.Usage = ImageUsage::Sampled | ImageUsage::Storage;
		specs.Size = size;
		m_GTAOPassImage = Image::Create(specs, "GTAO_Pass");

		specs.Usage = ImageUsage::Sampled | ImageUsage::Storage | ImageUsage::TransferSrc;
		m_Denoised = Image::Create(specs, "GTAO_Denoised");

		specs.Usage = ImageUsage::Sampled | ImageUsage::TransferDst;
		m_DenoisedPrev = Image::Create(specs, "GTAO_Denoised_Prev");

		m_Samples = m_Renderer.GetOptions_RT().GTAOSettings.GetNumberOfSamples();
		InitPipeline(m_Samples);
	}

	void GTAOTask::RecordCommandBuffer(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "GTAO");
		EG_CPU_TIMING_SCOPED("GTAO");

		auto& gBuffer = m_Renderer.GetGBuffer();
		const ImageLayout oldDepthLayout = gBuffer.Depth->GetLayout();
		cmd->TransitionLayout(gBuffer.Depth, oldDepthLayout, ImageReadAccess::PixelShaderRead);

		Downsample(cmd);
		GTAO(cmd);
		Denoiser(cmd);
		CopyToPrev(cmd);

		cmd->TransitionLayout(gBuffer.Depth, gBuffer.Depth->GetLayout(), oldDepthLayout);
	}

	void GTAOTask::Downsample(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "GTAO. Downsample");
		EG_CPU_TIMING_SCOPED("GTAO. Downsample");

		auto& gBuffer = m_Renderer.GetGBuffer();

		// Inputs
		m_DownsamplePipeline->SetImageSampler(gBuffer.Depth, Sampler::PointSampler, 0, 0);
		m_DownsamplePipeline->SetImageSampler(gBuffer.Motion, Sampler::PointSampler, 0, 1);

		cmd->BeginGraphics(m_DownsamplePipeline);
		cmd->Draw(6, 0);
		cmd->EndGraphics();
	}

	void GTAOTask::GTAO(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "GTAO. AO");
		EG_CPU_TIMING_SCOPED("GTAO. AO");

		struct PushData
		{
			glm::mat4 ProjInv;
			glm::vec3 ViewRow1;
			int SizeX;
			glm::vec3 ViewRow2;
			int SizeY;
			glm::vec3 ViewRow3;
			float Radius;
			float RadRotationTemporal;
		} pushData;
		static_assert(sizeof(PushData) <= 128);

		const auto& gtaoSettings = m_Renderer.GetOptions_RT().GTAOSettings;
		const auto& view = m_Renderer.GetViewMatrix();

		pushData.ProjInv = glm::inverse(m_Renderer.GetProjectionMatrix());
		pushData.SizeX = int(m_HalfSize.x);
		pushData.SizeY = int(m_HalfSize.y);
		pushData.Radius = gtaoSettings.GetRadius();
		pushData.ViewRow1 = view[0];
		pushData.ViewRow2 = view[1];
		pushData.ViewRow3 = view[2];
		pushData.RadRotationTemporal = GetRadRotationTemporal(RenderManager::GetFrameNumber());

		m_GTAOPipeline->SetImageSampler(m_HalfDepth, Sampler::PointSamplerClamp, 0, 0);
		m_GTAOPipeline->SetImageSampler(m_Renderer.GetGBuffer().Geometry_Shading_Normals, Sampler::PointSamplerClamp, 0, 1);
		m_GTAOPipeline->SetImage(m_GTAOPassImage, 0, 2);

		cmd->TransitionLayout(m_GTAOPassImage, m_GTAOPassImage->GetLayout(), ImageLayoutType::StorageImage);
		cmd->Barrier(m_HalfDepth);

		cmd->Dispatch(m_GTAOPipeline, m_HalfNumGroups.x, m_HalfNumGroups.y, 1, &pushData);

		cmd->TransitionLayout(m_GTAOPassImage, m_GTAOPassImage->GetLayout(), ImageReadAccess::PixelShaderRead);
	}

	void GTAOTask::Denoiser(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "GTAO. Denoiser");
		EG_CPU_TIMING_SCOPED("GTAO. Denoiser");

		m_DenoiserPipeline->SetImageSampler(m_GTAOPassImage, Sampler::PointSamplerClamp, 0, 0);
		m_DenoiserPipeline->SetImageSampler(m_DenoisedPrev, Sampler::PointSampler, 0, 1);
		m_DenoiserPipeline->SetImageSampler(m_HalfMotion, Sampler::PointSamplerClamp, 0, 2);
		m_DenoiserPipeline->SetImageSampler(m_HalfDepth, Sampler::PointSamplerClamp, 0, 3);
		m_DenoiserPipeline->SetImageSampler(m_HalfDepthPrev, Sampler::PointSamplerClamp, 0, 4);

		struct PushData
		{
			glm::uvec2 Size;
			glm::vec2 TexelSize;
		} pushData;
		static_assert(sizeof(PushData) <= 128);

		pushData.Size = m_HalfSize;
		pushData.TexelSize = m_HalfTexelSize;

		// Output
		m_DenoiserPipeline->SetImage(m_Denoised, 0, 5);

		cmd->TransitionLayout(m_HalfDepthPrev, m_HalfDepthPrev->GetLayout(), ImageReadAccess::PixelShaderRead);
		cmd->TransitionLayout(m_DenoisedPrev, m_DenoisedPrev->GetLayout(), ImageReadAccess::PixelShaderRead);
		cmd->TransitionLayout(m_Denoised, m_Denoised->GetLayout(), ImageLayoutType::StorageImage);

		cmd->Dispatch(m_DenoiserPipeline, m_HalfNumGroups.x, m_HalfNumGroups.y, 1, &pushData);

		cmd->TransitionLayout(m_Denoised, m_Denoised->GetLayout(), ImageReadAccess::PixelShaderRead);
	}

	void GTAOTask::CopyToPrev(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "GTAO. Copy to prev");
		EG_CPU_TIMING_SCOPED("GTAO. Copy to prev");

		cmd->CopyImage(m_HalfDepth, ImageView{}, m_HalfDepthPrev, ImageView{}, glm::ivec3(0), glm::ivec3(0), glm::uvec3(m_HalfSize, 1));
		cmd->CopyImage(m_Denoised, ImageView{}, m_DenoisedPrev, ImageView{}, glm::ivec3(0), glm::ivec3(0), glm::uvec3(m_HalfSize, 1));
	}

	void GTAOTask::InitPipeline(uint32_t samples)
	{
		const glm::uvec2 size = m_HalfDepth->GetSize();

		// Downsample
		{
			ColorAttachment depthAttachment;
			depthAttachment.Image = m_HalfDepth;
			depthAttachment.ClearOperation = ClearOperation::DontCare;
			depthAttachment.InitialLayout = ImageLayoutType::Unknown;
			depthAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;

			ColorAttachment motionAttachment;
			motionAttachment.Image = m_HalfMotion;
			motionAttachment.ClearOperation = ClearOperation::DontCare;
			motionAttachment.InitialLayout = ImageLayoutType::Unknown;
			motionAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;

			PipelineGraphicsState downsamplesState;
			downsamplesState.ColorAttachments.push_back(depthAttachment);
			downsamplesState.ColorAttachments.push_back(motionAttachment);
			downsamplesState.Size = size;
			downsamplesState.VertexShader = ShaderLibrary::GetOrLoad("assets/shaders/quad.vert", ShaderType::Vertex);
			downsamplesState.FragmentShader = Shader::Create("assets/shaders/gtao_downsample.frag", ShaderType::Fragment);
			downsamplesState.CullMode = CullMode::Back;

			m_DownsamplePipeline = PipelineGraphics::Create(downsamplesState);
		}

		// GTAO Pipeline
		{
			ShaderDefines defines;
			defines["EG_GTAO_SAMPLES"] = std::to_string(samples);

			PipelineComputeState state;
			state.ComputeShader = Shader::Create("assets/shaders/gtao.comp", ShaderType::Compute, defines);

			m_GTAOPipeline = PipelineCompute::Create(state);
		}

		// Denoiser pipeline
		{
			PipelineComputeState state;
			state.ComputeShader = Shader::Create("assets/shaders/gtao_denoiser.comp", ShaderType::Compute);

			m_DenoiserPipeline = PipelineCompute::Create(state);
		}
	}
}
