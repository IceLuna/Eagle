#include "egpch.h"
#include "MotionBlurTask.h"

#include "Eagle/Renderer/SceneRenderer.h"
#include "Eagle/Renderer/VidWrappers/RenderCommandManager.h"

#include "Eagle/Debug/CPUTimings.h"
#include "Eagle/Debug/GPUTimings.h"

namespace Eagle
{
	MotionBlurTask::MotionBlurTask(SceneRenderer& renderer)
		: RendererTask(renderer)
	{
		const auto& settings = m_Renderer.GetOptions().MotionBlur;
		bDebugTiles = settings.bDebugOutput;
		m_NumSamples = settings.NumSamples;
		InitPipeline();
		m_Size = m_Renderer.GetViewportSize();
		InitResources();

		BufferSpecifications specs{};
		specs.Usage = BufferUsage::StorageBuffer | BufferUsage::IndirectBuffer | BufferUsage::TransferDst;
		specs.Size = sizeof(PostprocessTileStatistics);
		m_DispatchArgs = Buffer::Create(specs, "DOF_DispatchArgs");
	}

	void MotionBlurTask::RecordCommandBuffer(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "Motion Blur");
		EG_CPU_TIMING_SCOPED("Motion Blur");

		m_PushData.ZNear = m_Renderer.GetZNear();
		m_PushData.ZFar = m_Renderer.GetZFar();
		m_PushData.Strength = m_Renderer.GetOptions_RT().MotionBlur.Strength;

		auto& depth = m_Renderer.GetGBuffer().Depth;
		const ImageLayout oldDepthLayout = depth->GetLayout();
		{
			EG_GPU_TIMING_SCOPED(cmd, "Motion Blur. Prepare buffers");
			EG_CPU_TIMING_SCOPED("Motion Blur. Prepare buffers");

			cmd->TransitionLayout(depth, oldDepthLayout, ImageReadAccess::PixelShaderRead);

			m_Tiles.EarlyExit.ThreadGroupCount = m_Tiles.Cheap.ThreadGroupCount = m_Tiles.Expensive.ThreadGroupCount = glm::uvec4(0, 1, 1, 0);
			cmd->Write(m_DispatchArgs, &m_Tiles, sizeof(m_Tiles), 0, m_DispatchArgs->GetLayout(), BufferLayoutType::StorageBuffer);

			cmd->FillBuffer(m_EarlyExitTiles, 0);
			cmd->FillBuffer(m_CheapTiles, 0);
			cmd->FillBuffer(m_ExpensiveTiles, 0);
		}

		TileMinMaxPass(cmd);
		NeighborhoodMinMaxPass(cmd);
		MainPass(cmd);

		cmd->TransitionLayout(depth, ImageReadAccess::PixelShaderRead, oldDepthLayout);
	}

	void MotionBlurTask::TileMinMaxPass(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "Motion Blur. Tile min-max");
		EG_CPU_TIMING_SCOPED("Motion Blur. Tile min-max");

		auto& stats = m_Renderer.GetStats();
		auto& motion = m_Renderer.GetGBuffer().Motion;
		const ImageLayout oldMotionLayout = motion->GetLayout();
		cmd->TransitionLayout(motion, oldMotionLayout, ImageReadAccess::PixelShaderRead);
		{
			EG_GPU_TIMING_SCOPED(cmd, "Motion Blur. Tile min-max. Horizontal");
			EG_CPU_TIMING_SCOPED("Motion Blur. Tile min-max. Horizontal");

			cmd->TransitionLayout(m_TileMinHorizontal, m_TileMinHorizontal->GetLayout(), ImageLayoutType::StorageImage);
			cmd->TransitionLayout(m_TileMaxHorizontal, m_TileMaxHorizontal->GetLayout(), ImageLayoutType::StorageImage);
			cmd->TransitionLayout(m_TileMin, m_TileMin->GetLayout(), ImageLayoutType::StorageImage);
			cmd->TransitionLayout(m_TileMax, m_TileMax->GetLayout(), ImageLayoutType::StorageImage);

			m_TileHorizontalPipeline->SetImageSampler(motion, Sampler::PointSamplerClamp, 0, 0);
			m_TileHorizontalPipeline->SetImage(m_TileMinHorizontal, 0, 1);
			m_TileHorizontalPipeline->SetImage(m_TileMaxHorizontal, 0, 2);

			m_PushData.PassSize = m_TileMaxHorizontal->GetSize();
			m_PushData.Size = m_Size;
			m_PushData.TexelSize = 1.f / glm::vec2(m_PushData.Size);

			constexpr uint32_t tileSize = 8;
			const auto& size = m_PushData.PassSize;
			glm::uvec2 numGroups = { glm::ceil(size.x / float(tileSize)), glm::ceil(size.y / float(tileSize)) };
			cmd->Dispatch(m_TileHorizontalPipeline, numGroups.x, numGroups.y, 1, &m_PushData);
			++stats.Dispatches;
		}
		cmd->TransitionLayout(motion, ImageReadAccess::PixelShaderRead, oldMotionLayout);

		{
			EG_GPU_TIMING_SCOPED(cmd, "Motion Blur. Tile min-max. Vertical");
			EG_CPU_TIMING_SCOPED("Motion Blur. Tile min-max. Vertical");

			m_TileVerticalPipeline->SetImage(m_TileMinHorizontal, 0, 0);
			m_TileVerticalPipeline->SetImage(m_TileMin, 0, 1);
			m_TileVerticalPipeline->SetImage(m_TileMaxHorizontal, 0, 2);
			m_TileVerticalPipeline->SetImage(m_TileMax, 0, 3);

			m_PushData.PassSize = m_TileMax->GetSize();
			m_PushData.Size = m_TileMaxHorizontal->GetSize();
			m_PushData.TexelSize = 1.f / glm::vec2(m_PushData.Size);

			cmd->Barrier(m_TileMinHorizontal);
			cmd->Barrier(m_TileMaxHorizontal);

			constexpr uint32_t tileSize = 8;
			const auto& size = m_PushData.PassSize;
			glm::uvec2 numGroups = { glm::ceil(size.x / float(tileSize)), glm::ceil(size.y / float(tileSize)) };
			cmd->Dispatch(m_TileVerticalPipeline, numGroups.x, numGroups.y, 1, &m_PushData);
			++stats.Dispatches;
		}
	}

	void MotionBlurTask::NeighborhoodMinMaxPass(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "Motion Blur. Neighborhood min-max");
		EG_CPU_TIMING_SCOPED("Motion Blur. Neighborhood min-max");

		auto& stats = m_Renderer.GetStats();

		m_NeighborhoodPipeline->SetImage(m_TileMin, 0, 0);
		m_NeighborhoodPipeline->SetImage(m_TileMax, 0, 1);
		m_NeighborhoodPipeline->SetImage(m_NeighborhoodMax, 0, 2);
		m_NeighborhoodPipeline->SetBuffer(m_DispatchArgs, 0, 3);
		m_NeighborhoodPipeline->SetBuffer(m_EarlyExitTiles, 0, 4);
		m_NeighborhoodPipeline->SetBuffer(m_CheapTiles, 0, 5);
		m_NeighborhoodPipeline->SetBuffer(m_ExpensiveTiles, 0, 6);

		m_PushData.PassSize = m_NeighborhoodMax->GetSize();
		m_PushData.Size = m_TileMax->GetSize();
		m_PushData.TexelSize = 1.f / glm::vec2(m_PushData.Size);

		cmd->TransitionLayout(m_NeighborhoodMax, ImageLayoutType::Unknown, ImageLayoutType::StorageImage);
		cmd->Barrier(m_TileMax);

		constexpr uint32_t tileSize = 8;
		const auto& size = m_PushData.PassSize;
		glm::uvec2 numGroups = { glm::ceil(size.x / float(tileSize)), glm::ceil(size.y / float(tileSize)) };
		cmd->Dispatch(m_NeighborhoodPipeline, numGroups.x, numGroups.y, 1, &m_PushData);

		cmd->Barrier(m_EarlyExitTiles);
		cmd->Barrier(m_CheapTiles);
		cmd->Barrier(m_ExpensiveTiles);

		cmd->TransitionLayout(m_DispatchArgs, BufferLayoutType::StorageBuffer, BufferReadAccess::IndirectArgument);
		++stats.Dispatches;
	}

	void MotionBlurTask::MainPass(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "Motion Blur. Main Pass");
		EG_CPU_TIMING_SCOPED("Motion Blur. Main Pass");

		auto& stats = m_Renderer.GetStats();

		m_PushData.PassSize = m_Size;
		m_PushData.Size = m_Size;
		m_PushData.TexelSize = 1.f / glm::vec2(m_PushData.Size);

		const auto& color = m_Renderer.GetHDROutput();
		const auto& depth = m_Renderer.GetGBuffer().Depth;
		const auto& motion = m_Renderer.GetGBuffer().Motion;
		auto setDescriptors = [this, &color, &depth, &motion](Ref<PipelineCompute>& pipeline, const Ref<Buffer>& tiles)
		{
			pipeline->SetImage(m_ColorCopy, 0, 0);
			pipeline->SetImage(m_NeighborhoodMax, 0, 1);
			pipeline->SetImageSampler(depth, Sampler::PointSamplerClamp, 0, 2);
			pipeline->SetImageSampler(motion, Sampler::PointSamplerClamp, 0, 3);
			pipeline->SetImage(color, 0, 4);
			pipeline->SetBuffer(tiles, 0, 5);
		};

		setDescriptors(m_MainEarlyPipeline, m_EarlyExitTiles);
		setDescriptors(m_MainCheapPipeline, m_CheapTiles);
		setDescriptors(m_MainExpensivePipeline, m_ExpensiveTiles);

		cmd->CopyImage(color, m_ColorCopy, m_ColorCopy->GetLayout(), ImageLayoutType::StorageImage);

		const ImageLayout colorLayout = color->GetLayout();
		const ImageLayout motionLayout = motion->GetLayout();
		cmd->TransitionLayout(color, colorLayout, ImageLayoutType::StorageImage);
		cmd->TransitionLayout(motion, motionLayout, ImageReadAccess::NonPixelShaderRead);

		cmd->DispatchIndirect(m_MainEarlyPipeline, m_DispatchArgs, offsetof(PostprocessTileStatistics, EarlyExit), &m_PushData);
		++stats.Dispatches;
		cmd->DispatchIndirect(m_MainCheapPipeline, m_DispatchArgs, offsetof(PostprocessTileStatistics, Cheap), &m_PushData);
		++stats.Dispatches;
		cmd->DispatchIndirect(m_MainExpensivePipeline, m_DispatchArgs, offsetof(PostprocessTileStatistics, Expensive), &m_PushData);
		++stats.Dispatches;

		cmd->TransitionLayout(color, ImageLayoutType::StorageImage, colorLayout);
		cmd->TransitionLayout(motion, ImageReadAccess::NonPixelShaderRead, motionLayout);
	}

	void MotionBlurTask::OnResize(glm::uvec2 size)
	{
		m_Size = size;
		InitResources();
	}

	void MotionBlurTask::InitWithOptions(const SceneRendererSettings& settings)
	{
		if (settings.MotionBlur.bDebugOutput == bDebugTiles && settings.MotionBlur.NumSamples == m_NumSamples)
			return;
		
		bDebugTiles = settings.MotionBlur.bDebugOutput;
		m_NumSamples = settings.MotionBlur.NumSamples;
		InitMainPipeline();
	}

	void MotionBlurTask::InitPipeline()
	{
		PipelineComputeState state{};

		state.ComputeShader = Shader::Create("motion_blur/tile_min_max_horizontal.comp", ShaderType::Compute);
		m_TileHorizontalPipeline = PipelineCompute::Create(state);

		state.ComputeShader = Shader::Create("motion_blur/tile_min_max_vertical.comp", ShaderType::Compute);
		m_TileVerticalPipeline = PipelineCompute::Create(state);

		state.ComputeShader = Shader::Create("motion_blur/neighborhood_min_max.comp", ShaderType::Compute);
		m_NeighborhoodPipeline = PipelineCompute::Create(state);

		InitMainPipeline();
	}

	void MotionBlurTask::InitMainPipeline()
	{
		PipelineComputeState state{};

		ShaderSpecializationInfo constants;
		constants.MapEntries.push_back({ 0, 0, sizeof(uint32_t) });
		constants.Data = &m_NumSamples;
		constants.Size = sizeof(m_NumSamples);

		state.ComputeSpecializationInfo = constants;

		ShaderDefines defines;
		if (bDebugTiles)
			defines["MOTIONBLUR_DEBUG_TILING"] = "";

		{
			auto localDefines = defines;
			localDefines["MOTIONBLUR_EARLYEXIT"] = "";
			state.ComputeShader = Shader::Create("motion_blur/main.comp", ShaderType::Compute, localDefines);
			m_MainEarlyPipeline = PipelineCompute::Create(state);
		}

		{
			auto localDefines = defines;
			localDefines["MOTIONBLUR_CHEAP"] = "";
			state.ComputeShader = Shader::Create("motion_blur/main.comp", ShaderType::Compute, localDefines);
			m_MainCheapPipeline = PipelineCompute::Create(state);
		}

		state.ComputeShader = Shader::Create("motion_blur/main.comp", ShaderType::Compute, defines);
		m_MainExpensivePipeline = PipelineCompute::Create(state);
	}

	void MotionBlurTask::InitResources()
	{
		{
			ImageSpecifications specs = m_Renderer.GetHDROutput()->GetSpecs();
			specs.Usage = ImageUsage::Storage | ImageUsage::TransferDst;
			specs.Layout = ImageLayoutType::Unknown;
			specs.MipsCount = 1;
			m_ColorCopy = Image::Create(specs, "MotionBlur. Color copy");
		}

		ImageSpecifications specs{};
		specs.Size = glm::uvec3((m_Size + s_TileSize - 1u) / s_TileSize, 1u);
		specs.Usage = ImageUsage::Storage | ImageUsage::Sampled;
		specs.Format = ImageFormat::R16G16_Float;

		m_TileMin = Image::Create(specs, "MotionBlur. Tile Min");
		m_TileMax = Image::Create(specs, "MotionBlur. Tile Max");
		m_NeighborhoodMax = Image::Create(specs, "MotionBlur. Neighborhood Max");

		BufferSpecifications bufferSpecs{};
		bufferSpecs.Usage = BufferUsage::StorageBuffer | BufferUsage::TransferDst;
		bufferSpecs.Size = specs.Size.x * specs.Size.y * sizeof(uint32_t);
		bufferSpecs.Layout = BufferLayoutType::StorageBuffer;
		m_EarlyExitTiles = Buffer::Create(bufferSpecs, "MotionBlur_EarlyExitTiles");
		m_CheapTiles = Buffer::Create(bufferSpecs, "MotionBlur_CheapTiles");
		m_ExpensiveTiles = Buffer::Create(bufferSpecs, "MotionBlur_ExpensiveTiles");

		specs.Size.y = m_Size.y;
		m_TileMinHorizontal = Image::Create(specs, "MotionBlur. Tile Min Horizontal");
		m_TileMaxHorizontal = Image::Create(specs, "MotionBlur. Tile Max Horizontal");
	}
}
