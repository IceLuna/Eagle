#include "egpch.h"
#include "PostprocessingPassTask.h"

#include "Eagle/Renderer/SceneRenderer.h"
#include "Eagle/Renderer/VidWrappers/RenderCommandManager.h"
#include "Eagle/Renderer/VidWrappers/Image.h"

#include "Eagle/Debug/CPUTimings.h"
#include "Eagle/Debug/GPUTimings.h"

namespace Eagle
{
	PostprocessingPassTask::PostprocessingPassTask(SceneRenderer& renderer, const Ref<Image>& input, const Ref<Image>& output)
		: RendererTask(renderer)
		, m_Input(input)
		, m_Output(output)
	{
		bAutoExposure = m_Renderer.GetOptions().AutoExposure.bEnable;

		InitPipeline();
		InitAutoexposureResources();

		BufferSpecifications specs{};
		specs.Usage = BufferUsage::StorageBuffer | BufferUsage::TransferDst;
		specs.Layout = BufferLayoutType::StorageBuffer;
		specs.Size = sizeof(float) * 2; // Exposure and Average Luminance
		m_Exposure = Buffer::Create(specs, "Exposure");

		RenderManager::Submit([exposure = m_Exposure](Ref<CommandBuffer>& cmd) mutable
		{
			cmd->FillBuffer(exposure, 0);
		});
	}

	void PostprocessingPassTask::RecordCommandBuffer(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "Postprocessing Pass");
		EG_CPU_TIMING_SCOPED("Postprocessing Pass");

		const auto& options = m_Renderer.GetOptions_RT();

		const ImageLayout inputOldLayout = m_Input->GetLayout();
		cmd->TransitionLayout(m_Input, inputOldLayout, ImageLayoutType::StorageImage);

		if (bAutoExposure)
			AutoExposurePass(cmd);
		else
			cmd->Write(m_Exposure, &options.Exposure, sizeof(float), 0, BufferLayoutType::Unknown, BufferLayoutType::StorageBuffer);

		ApplyPass(cmd);

		cmd->TransitionLayout(m_Input, ImageLayoutType::StorageImage, inputOldLayout);
	}

	void PostprocessingPassTask::InitWithOptions(const SceneRendererSettings& settings)
	{
		if (bAutoExposure == settings.AutoExposure.bEnable)
			return;
		
		bAutoExposure = settings.AutoExposure.bEnable;
		InitAutoexposureResources();
	}
	
	void PostprocessingPassTask::InitPipeline()
	{
		PipelineComputeState state;
		state.ComputeShader = Shader::Create("postprocessing/postprocessing.comp", ShaderType::Compute);

		m_Pipeline = PipelineCompute::Create(state);
	}

	void PostprocessingPassTask::InitAutoexposureResources()
	{
		if (!bAutoExposure)
		{
			m_HistogramPipeline.reset();
			m_AveragePipeline.reset();
			m_Histogram.reset();
			return;
		}

		{
			BufferSpecifications specs{};
			specs.Usage = BufferUsage::StorageBuffer;
			specs.Layout = BufferLayoutType::StorageBuffer;
			specs.Size = s_TileSize * s_TileSize * sizeof(uint32_t);
			m_Histogram = Buffer::Create(specs, "Histogram");
		}

		PipelineComputeState state{};
		state.ComputeShader = Shader::Create("postprocessing/autoexposure_histogram.comp", ShaderType::Compute);
		m_HistogramPipeline = PipelineCompute::Create(state);

		state.ComputeShader = Shader::Create("postprocessing/autoexposure_average.comp", ShaderType::Compute);
		m_AveragePipeline = PipelineCompute::Create(state);
	}

	void PostprocessingPassTask::AutoExposurePass(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "Postprocessing. Auto Exposure");
		EG_CPU_TIMING_SCOPED("Postprocessing. Auto Exposure");

		const auto& options = m_Renderer.GetOptions_RT().AutoExposure;
		const float ts = Application::Get().GetTimestep();
		
		const float minLogLum = options.MinLogLum;
		const float maxLogLum = options.MaxLogLum;
		const float timeCoeff = glm::clamp(1.0f - glm::exp(-ts * options.AdaptationSpeed), 0.0f, 1.0f);

		const uint32_t downscaleFactor = options.bHalfResolution ? 2u : 1u;
		const glm::uvec2 size = m_Input->GetSize() / downscaleFactor;

		{
			EG_GPU_TIMING_SCOPED(cmd, "Postprocessing. Calculate Histogram");
			EG_CPU_TIMING_SCOPED("Postprocessing. Calculate Histogram");

			struct PushData
			{
				glm::ivec2 Size;
				int DownscaleFactor;
				float MinLog2Lum;
				float InvLog2Lum;
			} pushData;
			static_assert(sizeof(PushData) <= 128);

			m_HistogramPipeline->SetImage(m_Input, 0, 0);
			m_HistogramPipeline->SetBuffer(m_Histogram, 0, 1);

			pushData.Size = size;
			pushData.DownscaleFactor = int(downscaleFactor);
			pushData.MinLog2Lum = minLogLum;
			pushData.InvLog2Lum = 1.0f / (maxLogLum - minLogLum);

			glm::uvec2 numGroups = { glm::ceil(size.x / float(s_TileSize)), glm::ceil(size.y / float(s_TileSize)) };
			cmd->Dispatch(m_HistogramPipeline, numGroups.x, numGroups.y, 1, &pushData);
		}

		{
			EG_GPU_TIMING_SCOPED(cmd, "Postprocessing. Calculate Average");
			EG_CPU_TIMING_SCOPED("Postprocessing. Calculate Average");

			struct PushData
			{
				uint32_t NumPixels;
				float TimeCoef;
				float MinLog2Lum;
				float LogLumRange;
				float AdaptationKey;
			} pushData;
			static_assert(sizeof(PushData) <= 128);

			m_AveragePipeline->SetBuffer(m_Histogram, 0, 0);
			m_AveragePipeline->SetBuffer(m_Exposure, 0, 1);

			pushData.NumPixels = size.x * size.y;
			pushData.TimeCoef = timeCoeff;
			pushData.MinLog2Lum = minLogLum;
			pushData.LogLumRange = (maxLogLum - minLogLum);
			pushData.AdaptationKey = options.AdaptationKey;

			cmd->Barrier(m_Histogram);
			cmd->Dispatch(m_AveragePipeline, 1, 1, 1, &pushData);
			cmd->Barrier(m_Exposure);
		}
	}
	
	void PostprocessingPassTask::ApplyPass(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "Postprocessing. Apply");
		EG_CPU_TIMING_SCOPED("Postprocessing. Apply");

		const auto& options = m_Renderer.GetOptions_RT();

		struct PushData
		{
			glm::ivec2 Size;
			float InvGamma;
			float PhotolinearScale;
			float WhitePoint;
			uint32_t TonemappingMethod;
		} pushData;
		static_assert(sizeof(PushData) <= 128);

		constexpr uint32_t tileSize = 8;
		const glm::uvec2 size = m_Input->GetSize();
		glm::uvec2 numGroups = { glm::ceil(size.x / float(tileSize)), glm::ceil(size.y / float(tileSize)) };

		pushData.Size = size;
		pushData.InvGamma = 1.f / options.Gamma;
		pushData.PhotolinearScale = m_Renderer.GetPhotoLinearScale();
		pushData.WhitePoint = options.FilmicTonemappingParams.WhitePoint;
		pushData.TonemappingMethod = (uint32_t)options.Tonemapping;

		m_Pipeline->SetImage(m_Input, 0, 0);
		m_Pipeline->SetImage(m_Output, 0, 1);
		m_Pipeline->SetBuffer(m_Exposure, 0, 2);

		cmd->TransitionLayout(m_Output, ImageLayoutType::Unknown, ImageLayoutType::StorageImage);
		cmd->Dispatch(m_Pipeline, numGroups.x, numGroups.y, 1, &pushData);
		cmd->TransitionLayout(m_Output, ImageLayoutType::StorageImage, ImageReadAccess::PixelShaderRead);
	}
}
