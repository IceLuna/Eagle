#include "egpch.h"
#include "PostprocessingPassTask.h"

#include "Eagle/Renderer/SceneRenderer.h"
#include "Eagle/Renderer/VidWrappers/RenderCommandManager.h"
#include "Eagle/Renderer/VidWrappers/Image.h"

#include "Eagle/Debug/CPUTimings.h"
#include "Eagle/Debug/GPUTimings.h"

namespace Eagle
{
	PostprocessingPassTask::PostprocessingPassTask(SceneRenderer& renderer)
		: RendererTask(renderer)
	{
		const auto& options = m_Renderer.GetOptions();
		const auto& lens = options.Lens;
		bAutoExposure = options.AutoExposure.bEnable;
		bChromaticAberration = lens.bEnableChromaticAberration;
		bVignette = lens.bEnableVignette;
		bFilmGrain = lens.bEnableFilmGrain;

		InitTonemappingPipeline();
		InitAutoexposureResources();
		InitLensPipeline();

		BufferSpecifications specs{};
		specs.Usage = BufferUsage::StorageBuffer | BufferUsage::TransferDst;
		specs.Layout = BufferLayoutType::StorageBuffer;
		specs.Size = sizeof(float) * 2; // Exposure and Average Luminance
		m_Exposure = Buffer::Create(specs, "Exposure");

		ImageSpecifications imageSpecs{};
		imageSpecs.Size = glm::uvec3(m_Renderer.GetViewportSize(), 1u);
		imageSpecs.Format = ImageFormat::R11G11B10_Float;
		imageSpecs.Usage = ImageUsage::Storage | ImageUsage::Sampled;
		m_Intermediate = Image::Create(imageSpecs, "Postprocessing_Intermediate");

		RenderManager::Submit([exposure = m_Exposure](Ref<CommandBuffer>& cmd) mutable
		{
			cmd->FillBuffer(exposure, 0);
		});
	}

	void PostprocessingPassTask::RecordCommandBuffer(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "Postprocessing Pass");
		EG_CPU_TIMING_SCOPED("Postprocessing Pass");

		const auto& input = m_Renderer.GetHDROutput();
		const auto& output = m_Renderer.GetOutput();
		const auto& options = m_Renderer.GetOptions_RT();
		const bool bLensEnabled = m_LensPipeline.operator bool();

		const ImageLayout inputOldLayout = input->GetLayout();
		const ImageLayout outputOldLayout = output->GetLayout();
		cmd->TransitionLayout(input, inputOldLayout, ImageLayoutType::StorageImage);
		cmd->TransitionLayout(m_Intermediate, m_Intermediate->GetLayout(), ImageLayoutType::StorageImage);
		cmd->TransitionLayout(output, ImageLayoutType::Unknown, ImageLayoutType::StorageImage);

		if (bAutoExposure)
			AutoExposurePass(cmd);
		else
			cmd->Write(m_Exposure, &options.Exposure, sizeof(float), 0, m_Exposure->GetLayout(), BufferLayoutType::StorageBuffer);

		auto& intermediate = bLensEnabled ? m_Intermediate : output;

		TonemappingPass(cmd, intermediate);
		if (bLensEnabled)
		{
			cmd->TransitionLayout(intermediate, ImageLayoutType::StorageImage, ImageReadAccess::NonPixelShaderRead);
			LensPass(cmd, intermediate, output);
		}

		cmd->TransitionLayout(output, output->GetLayout(), outputOldLayout);
		cmd->TransitionLayout(input, ImageLayoutType::StorageImage, inputOldLayout);
	}

	void PostprocessingPassTask::InitWithOptions(const SceneRendererSettings& settings)
	{
		const auto& lens = settings.Lens;

		if (bAutoExposure == settings.AutoExposure.bEnable &&
			bChromaticAberration == lens.bEnableChromaticAberration &&
			bVignette == lens.bEnableVignette &&
			bFilmGrain == lens.bEnableFilmGrain)
		{
			return;
		}

		bAutoExposure = settings.AutoExposure.bEnable;
		bChromaticAberration = lens.bEnableChromaticAberration;
		bVignette = lens.bEnableVignette;
		bFilmGrain = lens.bEnableFilmGrain;

		InitAutoexposureResources();
		InitLensPipeline();
		InitTonemappingPipeline();
	}

	void PostprocessingPassTask::OnResize(const glm::uvec2 size)
	{
		m_Intermediate->Resize(glm::uvec3(size, 1u));
	}
	
	void PostprocessingPassTask::InitTonemappingPipeline()
	{
		// If lens is enabled, tonemapping will write to an intermediate texture
		const bool bUsesLens = ShouldUseLens();
		ShaderDefines defines;
		defines["OUTPUT_FORMAT"] = bUsesLens ? "r11f_g11f_b10f" : "rgba8";

		PipelineComputeState state;
		state.ComputeShader = Shader::Create("postprocessing/postprocessing.comp", ShaderType::Compute, defines);

		m_TonemappingPipeline = PipelineCompute::Create(state);
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

	void PostprocessingPassTask::InitLensPipeline()
	{
		if (!ShouldUseLens())
		{
			m_LensPipeline.reset();
			return;
		}

		ShaderDefines defines;
		if (bChromaticAberration)
			defines["EG_CHROMATIC_ABERRATION"] = "";
		if (bVignette)
			defines["EG_VIGNETTE"] = "";
		if (bFilmGrain)
			defines["EG_FILM_GRAIN"] = "";

		PipelineComputeState state{};
		state.ComputeShader = Shader::Create("lens/lens.comp", ShaderType::Compute, defines);
		m_LensPipeline = PipelineCompute::Create(state);
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

		const auto& input = m_Renderer.GetHDROutput();
		const uint32_t downscaleFactor = options.bHalfResolution ? 2u : 1u;
		const glm::uvec2 size = input->GetSize() / downscaleFactor;

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

			m_HistogramPipeline->SetImage(input, 0, 0);
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
	
	void PostprocessingPassTask::TonemappingPass(const Ref<CommandBuffer>& cmd, const Ref<Image>& output)
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
		const auto& input = m_Renderer.GetHDROutput();
		const glm::uvec2 size = input->GetSize();
		glm::uvec2 numGroups = { glm::ceil(size.x / float(tileSize)), glm::ceil(size.y / float(tileSize)) };

		pushData.Size = size;
		pushData.InvGamma = 1.f / options.Gamma;
		pushData.PhotolinearScale = m_Renderer.GetPhotoLinearScale();
		pushData.WhitePoint = options.FilmicTonemappingParams.WhitePoint;
		pushData.TonemappingMethod = (uint32_t)options.Tonemapping;

		m_TonemappingPipeline->SetImage(input, 0, 0);
		m_TonemappingPipeline->SetImage(output, 0, 1);
		m_TonemappingPipeline->SetBuffer(m_Exposure, 0, 2);

		cmd->Dispatch(m_TonemappingPipeline, numGroups.x, numGroups.y, 1, &pushData);

		auto& stats = m_Renderer.GetStats();
		++stats.Dispatches;
	}
	
	void PostprocessingPassTask::LensPass(const Ref<CommandBuffer>& cmd, const Ref<Image>& input, const Ref<Image>& output)
	{
		EG_GPU_TIMING_SCOPED(cmd, "Postprocessing. Lens Pass");
		EG_CPU_TIMING_SCOPED("Postprocessing. Lens Pass");

		const auto& lens = m_Renderer.GetOptions_RT().Lens;
		const Timestep ts = Application::Get().GetTimestep();

		struct PushData
		{
			glm::uvec2 Size;
			float ChromaticIntensity;
			float VignetteIntensity;
			float FilmGrainScale;
			float FilmGrainAmount;
			uint32_t GrainSeed;
		} pushData;
		static_assert(sizeof(PushData) <= 128);

		// Note: TileSize of 8 is expected by FFX
		constexpr uint32_t tileSize = 8;
		const glm::uvec2 size = output->GetSize();
		glm::uvec2 numGroups = { glm::ceil(size.x / float(tileSize)), glm::ceil(size.y / float(tileSize)) };

		pushData.Size = size;
		pushData.ChromaticIntensity = lens.ChromaticIntensity;
		pushData.VignetteIntensity = lens.VignetteIntensity;
		pushData.FilmGrainScale = lens.FilmGrainScale;
		pushData.FilmGrainAmount = lens.FilmGrainAmount;
		pushData.GrainSeed = CalcGrainSeed(ts, lens.FilmGrainSeedUpdateRate);

		m_LensPipeline->SetImageSampler(input, Sampler::BilinearSamplerClamp, 0, 0);
		m_LensPipeline->SetImage(output, 0, 1);

		cmd->Dispatch(m_LensPipeline, numGroups.x, numGroups.y, 1, &pushData);

		auto& stats = m_Renderer.GetStats();
		++stats.Dispatches;
	}

	uint32_t PostprocessingPassTask::CalcGrainSeed(Timestep deltaTime, float seedUpdateRate)
	{
		// Update seed for grain at fixed time intervals
		m_SeedTimer += deltaTime;
		if (m_SeedTimer >= seedUpdateRate)
		{
			++m_FilmSeed;
			m_SeedTimer = 0.0;
		}
		return m_FilmSeed;
	}
}
