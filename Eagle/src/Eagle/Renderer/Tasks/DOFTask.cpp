#include "egpch.h"
#include "DOFTask.h"

#include "Eagle/Renderer/SceneRenderer.h"
#include "Eagle/Renderer/VidWrappers/RenderCommandManager.h"

#include "Eagle/Debug/CPUTimings.h"
#include "Eagle/Debug/GPUTimings.h"

namespace Eagle
{
	DOFTask::DOFTask(SceneRenderer& renderer)
		: RendererTask(renderer)
	{
		bDebugTiles = m_Renderer.GetOptions().DOFSettings.bDebugOutput;
		InitPipeline();
		m_Size = m_Renderer.GetViewportSize();
		InitResources();

		BufferSpecifications specs{};
		specs.Usage = BufferUsage::StorageBuffer | BufferUsage::IndirectBuffer | BufferUsage::TransferDst;
		specs.Size = sizeof(PostprocessTileStatistics);
		m_DispatchArgs = Buffer::Create(specs, "DOF_DispatchArgs");
	}

	void DOFTask::RecordCommandBuffer(const Ref<CommandBuffer>& cmd)
	{
		const auto& settings = m_Renderer.GetOptions_RT().DOFSettings;
		if (settings.ApertureSize < 0.0001f)
			return;

		EG_GPU_TIMING_SCOPED(cmd, "Depth of Field");
		EG_CPU_TIMING_SCOPED("Depth of Field");

		m_PushData.ApertureShape = settings.ApertureShape;
		m_PushData.ApertureSize = settings.ApertureSize;
		m_PushData.FocalLength = settings.FocalLength;
		m_PushData.COCScale = settings.COCScale;
		m_PushData.MaxCOC = settings.MaxCOC;
		m_PushData.ZNear = m_Renderer.GetZNear();
		m_PushData.ZFar = m_Renderer.GetZFar();

		auto& depth = m_Renderer.GetGBuffer().Depth;
		const ImageLayout oldDepthLayout = depth->GetLayout();
		{
			EG_GPU_TIMING_SCOPED(cmd, "DOF. Prepare buffers");
			EG_CPU_TIMING_SCOPED("DOF. Prepare buffers");

			cmd->TransitionLayout(depth, oldDepthLayout, ImageReadAccess::PixelShaderRead);

			m_Tiles.EarlyExit.ThreadGroupCount = m_Tiles.Cheap.ThreadGroupCount = m_Tiles.Expensive.ThreadGroupCount = glm::uvec4(0, 1, 1, 0);
			cmd->Write(m_DispatchArgs, &m_Tiles, sizeof(m_Tiles), 0, BufferLayoutType::Unknown, BufferLayoutType::StorageBuffer);

			cmd->FillBuffer(m_EarlyExitTiles, 0);
			cmd->FillBuffer(m_CheapTiles, 0);
			cmd->FillBuffer(m_ExpensiveTiles, 0);

			cmd->ClearColorImage(m_Presort, glm::vec4(0.f), ImageLayoutType::Unknown, ImageLayoutType::StorageImage);
		}

		TileMinMaxPass(cmd);
		NeighborhoodMinMaxPass(cmd);
		Presort(cmd);
		MainPass(cmd);
		PostFilterPass(cmd);
		UpsamplePass(cmd);

		cmd->TransitionLayout(depth, ImageReadAccess::PixelShaderRead, oldDepthLayout);
	}
	
	void DOFTask::TileMinMaxPass(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "DOF. Tile min-max");
		EG_CPU_TIMING_SCOPED("DOF. Tile min-max");

		{
			EG_GPU_TIMING_SCOPED(cmd, "DOF. Tile min-max. Horizontal");
			EG_CPU_TIMING_SCOPED("DOF. Tile min-max. Horizontal");

			cmd->TransitionLayout(m_TileMaxHorizontal, ImageLayoutType::Unknown, ImageLayoutType::StorageImage);
			cmd->TransitionLayout(m_TileMinCOCHorizontal, ImageLayoutType::Unknown, ImageLayoutType::StorageImage);
			cmd->TransitionLayout(m_TileMax, ImageLayoutType::Unknown, ImageLayoutType::StorageImage);
			cmd->TransitionLayout(m_TileMinCOC, ImageLayoutType::Unknown, ImageLayoutType::StorageImage);

			m_TileHorizontalPipeline->SetImageSampler(m_Renderer.GetGBuffer().Depth, Sampler::PointSamplerClamp, 0, 0);
			m_TileHorizontalPipeline->SetImage(m_TileMaxHorizontal, 0, 1);
			m_TileHorizontalPipeline->SetImage(m_TileMinCOCHorizontal, 0, 2);

			m_PushData.PassSize = m_TileMaxHorizontal->GetSize();
			m_PushData.Size = m_Size;
			m_PushData.TexelSize = 1.f / glm::vec2(m_PushData.Size);

			constexpr uint32_t tileSize = 8;
			const auto& size = m_PushData.PassSize;
			glm::uvec2 numGroups = { glm::ceil(size.x / float(tileSize)), glm::ceil(size.y / float(tileSize)) };
			cmd->Dispatch(m_TileHorizontalPipeline, numGroups.x, numGroups.y, 1, &m_PushData);
		}

		{
			EG_GPU_TIMING_SCOPED(cmd, "DOF. Tile min-max. Vertical");
			EG_CPU_TIMING_SCOPED("DOF. Tile min-max. Vertical");

			m_TileVerticalPipeline->SetImage(m_TileMaxHorizontal, 0, 0);
			m_TileVerticalPipeline->SetImage(m_TileMax, 0, 1);
			m_TileVerticalPipeline->SetImage(m_TileMinCOCHorizontal, 0, 2);
			m_TileVerticalPipeline->SetImage(m_TileMinCOC, 0, 3);

			m_PushData.PassSize = m_TileMax->GetSize();
			m_PushData.Size = m_TileMaxHorizontal->GetSize();
			m_PushData.TexelSize = 1.f / glm::vec2(m_PushData.Size);

			cmd->Barrier(m_TileMinCOCHorizontal);
			cmd->Barrier(m_TileMaxHorizontal);

			constexpr uint32_t tileSize = 8;
			const auto& size = m_PushData.PassSize;
			glm::uvec2 numGroups = { glm::ceil(size.x / float(tileSize)), glm::ceil(size.y / float(tileSize)) };
			cmd->Dispatch(m_TileVerticalPipeline, numGroups.x, numGroups.y, 1, &m_PushData);
		}
	}

	void DOFTask::NeighborhoodMinMaxPass(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "DOF. Neighborhood min-max");
		EG_CPU_TIMING_SCOPED("DOF. Neighborhood min-max");
		
		m_NeighborhoodPipeline->SetImage(m_TileMax, 0, 0);
		m_NeighborhoodPipeline->SetImage(m_TileMinCOC, 0, 1);
		m_NeighborhoodPipeline->SetImage(m_NeighborhoodMax, 0, 2);
		m_NeighborhoodPipeline->SetBuffer(m_DispatchArgs, 0, 3);
		m_NeighborhoodPipeline->SetBuffer(m_EarlyExitTiles, 0, 4);
		m_NeighborhoodPipeline->SetBuffer(m_CheapTiles, 0, 5);
		m_NeighborhoodPipeline->SetBuffer(m_ExpensiveTiles, 0, 6);

		m_PushData.PassSize = m_NeighborhoodMax->GetSize();
		m_PushData.Size = m_NeighborhoodMax->GetSize();
		m_PushData.TexelSize = 1.f / glm::vec2(m_PushData.Size);

		cmd->TransitionLayout(m_NeighborhoodMax, ImageLayoutType::Unknown, ImageLayoutType::StorageImage);
		cmd->Barrier(m_TileMax);
		cmd->Barrier(m_TileMinCOC);

		constexpr uint32_t tileSize = 8;
		const auto& size = m_PushData.PassSize;
		glm::uvec2 numGroups = { glm::ceil(size.x / float(tileSize)), glm::ceil(size.y / float(tileSize)) };
		cmd->Dispatch(m_NeighborhoodPipeline, numGroups.x, numGroups.y, 1, &m_PushData);

		cmd->Barrier(m_EarlyExitTiles);
		cmd->Barrier(m_CheapTiles);
		cmd->Barrier(m_ExpensiveTiles);

		cmd->TransitionLayout(m_DispatchArgs, BufferLayoutType::StorageBuffer, BufferReadAccess::IndirectArgument);
	}

	void DOFTask::Presort(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "DOF. Presort Pass");
		EG_CPU_TIMING_SCOPED("DOF. Presort Pass");

		// Half res pass
		m_PushData.PassSize = m_Size / 2u;
		m_PushData.Size = m_PushData.PassSize;
		m_PushData.TexelSize = 1.f / glm::vec2(m_PushData.Size);
		
		auto& color = m_Renderer.GetHDROutput();
		const ImageLayout inputOldLayout = color->GetLayout();
		cmd->TransitionLayout(color, inputOldLayout, ImageReadAccess::PixelShaderRead);

		auto setDescriptors = [this, &color](Ref<PipelineCompute>& pipeline, const Ref<Buffer>& tiles)
		{
			pipeline->SetImageSampler(color, Sampler::PointSamplerClamp, 0, 0);
			pipeline->SetImageSampler(m_Renderer.GetGBuffer().Depth, Sampler::PointSamplerClamp, 0, 1);
			pipeline->SetImage(m_NeighborhoodMax, 0, 2);
			pipeline->SetImage(m_Presort, 0, 3);
			pipeline->SetImage(m_Prefilter, 0, 4);
			pipeline->SetBuffer(tiles, 0, 5);
		};

		setDescriptors(m_PresortEarlyPipeline, m_EarlyExitTiles);
		setDescriptors(m_PresortCheapPipeline, m_CheapTiles);
		setDescriptors(m_PresortExpensivePipeline, m_ExpensiveTiles);

		cmd->TransitionLayout(m_Prefilter, ImageLayoutType::Unknown, ImageLayoutType::StorageImage);
		cmd->Barrier(m_NeighborhoodMax);

		cmd->DispatchIndirect(m_PresortEarlyPipeline, m_DispatchArgs, offsetof(PostprocessTileStatistics, EarlyExit), &m_PushData);
		cmd->DispatchIndirect(m_PresortCheapPipeline, m_DispatchArgs, offsetof(PostprocessTileStatistics, Cheap), &m_PushData);
		cmd->DispatchIndirect(m_PresortExpensivePipeline, m_DispatchArgs, offsetof(PostprocessTileStatistics, Expensive), &m_PushData);

		cmd->TransitionLayout(color, ImageReadAccess::PixelShaderRead, inputOldLayout);
	}

	void DOFTask::MainPass(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "DOF. Main Pass");
		EG_CPU_TIMING_SCOPED("DOF. Main Pass");

		// Half res pass

		auto setDescriptors = [this](Ref<PipelineCompute>& pipeline, const Ref<Buffer>& tiles)
		{
			pipeline->SetImageSampler(m_Presort, Sampler::PointSamplerClamp, 0, 0);
			pipeline->SetImageSampler(m_Prefilter, Sampler::BilinearSamplerClamp, 0, 1);
			pipeline->SetImage(m_NeighborhoodMax, 0, 2);
			pipeline->SetImage(m_Main, 0, 3);
			pipeline->SetImage(m_AlphaTemp, 0, 4);
			pipeline->SetBuffer(tiles, 0, 5);
		};

		setDescriptors(m_MainEarlyPipeline, m_EarlyExitTiles);
		setDescriptors(m_MainCheapPipeline, m_CheapTiles);
		setDescriptors(m_MainExpensivePipeline, m_ExpensiveTiles);

		cmd->TransitionLayout(m_Presort, ImageLayoutType::StorageImage, ImageReadAccess::PixelShaderRead);
		cmd->TransitionLayout(m_Prefilter, ImageLayoutType::StorageImage, ImageReadAccess::PixelShaderRead);
		cmd->TransitionLayout(m_Main, ImageLayoutType::Unknown, ImageLayoutType::StorageImage);
		cmd->TransitionLayout(m_AlphaTemp, ImageLayoutType::Unknown, ImageLayoutType::StorageImage);

		cmd->DispatchIndirect(m_MainEarlyPipeline, m_DispatchArgs, offsetof(PostprocessTileStatistics, EarlyExit), &m_PushData);
		cmd->DispatchIndirect(m_MainCheapPipeline, m_DispatchArgs, offsetof(PostprocessTileStatistics, Cheap), &m_PushData);
		cmd->DispatchIndirect(m_MainExpensivePipeline, m_DispatchArgs, offsetof(PostprocessTileStatistics, Expensive), &m_PushData);
	}

	void DOFTask::PostFilterPass(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "DOF. Post filter Pass");
		EG_CPU_TIMING_SCOPED("DOF. Post filter Pass");
		
		// Half res pass
		m_PostFilterPipeline->SetImageSampler(m_Main, Sampler::PointSamplerClamp, 0, 0);
		m_PostFilterPipeline->SetImageSampler(m_AlphaTemp, Sampler::PointSamplerClamp, 0, 1);

		m_PostFilterPipeline->SetImage(m_Postfilter, 0, 2);
		m_PostFilterPipeline->SetImage(m_AlphaResult, 0, 3);

		cmd->TransitionLayout(m_Main, ImageLayoutType::StorageImage, ImageReadAccess::PixelShaderRead);
		cmd->TransitionLayout(m_AlphaTemp, ImageLayoutType::StorageImage, ImageReadAccess::PixelShaderRead);
		cmd->TransitionLayout(m_Postfilter, ImageLayoutType::Unknown, ImageLayoutType::StorageImage);
		cmd->TransitionLayout(m_AlphaResult, ImageLayoutType::Unknown, ImageLayoutType::StorageImage);

		constexpr uint32_t tileSize = 8;
		const auto& size = m_PushData.PassSize;
		glm::uvec2 numGroups = { glm::ceil(size.x / float(tileSize)), glm::ceil(size.y / float(tileSize)) };
		cmd->Dispatch(m_PostFilterPipeline, numGroups.x, numGroups.y, 1, &m_PushData);
	}

	void DOFTask::UpsamplePass(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "DOF. Upsample Pass");
		EG_CPU_TIMING_SCOPED("DOF. Upsample Pass");

		// Full res pass
		m_PushData.PassSize = m_Size;
		m_PushData.Size = m_PushData.PassSize;
		m_PushData.TexelSize = 1.f / glm::vec2(m_PushData.Size);

		auto& color = m_Renderer.GetHDROutput();
		const ImageLayout inputOldLayout = color->GetLayout();
		cmd->TransitionLayout(color, inputOldLayout, ImageLayoutType::StorageImage);

		m_UpsamplePipeline->SetImageSampler(m_Postfilter, Sampler::BilinearSamplerClamp, 0, 0);
		m_UpsamplePipeline->SetImageSampler(m_AlphaResult, Sampler::BilinearSamplerClamp, 0, 1);
		m_UpsamplePipeline->SetImage(m_NeighborhoodMax, 0, 2);
		m_UpsamplePipeline->SetImageSampler(m_Renderer.GetGBuffer().Depth, Sampler::PointSamplerClamp, 0, 3);
		m_UpsamplePipeline->SetImage(m_Renderer.GetHDROutput(), 0, 4);

		cmd->TransitionLayout(m_Postfilter, ImageLayoutType::StorageImage, ImageReadAccess::PixelShaderRead);
		cmd->TransitionLayout(m_AlphaResult, ImageLayoutType::StorageImage, ImageReadAccess::PixelShaderRead);

		constexpr uint32_t tileSize = 8;
		const auto& size = m_PushData.PassSize;
		glm::uvec2 numGroups = { glm::ceil(size.x / float(tileSize)), glm::ceil(size.y / float(tileSize)) };
		cmd->Dispatch(m_UpsamplePipeline, numGroups.x, numGroups.y, 1, &m_PushData);

		cmd->TransitionLayout(color, ImageLayoutType::StorageImage, inputOldLayout);
	}

	void DOFTask::OnResize(glm::uvec2 size)
	{
		m_Size = size;
		InitResources();
	}

	void DOFTask::InitWithOptions(const SceneRendererSettings& settings)
	{
		if (settings.DOFSettings.bDebugOutput == bDebugTiles)
			return;

		bDebugTiles = settings.DOFSettings.bDebugOutput;
		InitMainPipeline();
	}

	void DOFTask::InitPipeline()
	{
		PipelineComputeState state{};

		state.ComputeShader = Shader::Create("dof/tile_min_max_horizontal.comp", ShaderType::Compute);
		m_TileHorizontalPipeline = PipelineCompute::Create(state);

		state.ComputeShader = Shader::Create("dof/tile_min_max_vertical.comp", ShaderType::Compute);
		m_TileVerticalPipeline = PipelineCompute::Create(state);

		state.ComputeShader = Shader::Create("dof/neighborhood_min_max.comp", ShaderType::Compute);
		m_NeighborhoodPipeline = PipelineCompute::Create(state);

		// Presort
		{
			state.ComputeShader = Shader::Create("dof/presort.comp", ShaderType::Compute, { {"DOF_EARLYEXIT", ""} });
			m_PresortEarlyPipeline = PipelineCompute::Create(state);

			state.ComputeShader = Shader::Create("dof/presort.comp", ShaderType::Compute);
			m_PresortCheapPipeline = PipelineCompute::Create(state);

			state.ComputeShader = Shader::Create("dof/presort.comp", ShaderType::Compute);
			m_PresortExpensivePipeline = PipelineCompute::Create(state);
		}

		InitMainPipeline();

		state.ComputeShader = Shader::Create("dof/postfilter.comp", ShaderType::Compute);
		m_PostFilterPipeline = PipelineCompute::Create(state);

		state.ComputeShader = Shader::Create("dof/upsample.comp", ShaderType::Compute);
		m_UpsamplePipeline = PipelineCompute::Create(state);
	}

	void DOFTask::InitMainPipeline()
	{
		PipelineComputeState state{};

		ShaderDefines defines;
		if (bDebugTiles)
			defines["DOF_DEBUG_TILING"] = "";

		{
			auto localDefines = defines;
			localDefines["DOF_EARLYEXIT"] = "";
			state.ComputeShader = Shader::Create("dof/main.comp", ShaderType::Compute, localDefines);
			m_MainEarlyPipeline = PipelineCompute::Create(state);
		}

		{
			auto localDefines = defines;
			localDefines["DOF_CHEAP"] = "";
			state.ComputeShader = Shader::Create("dof/main.comp", ShaderType::Compute, localDefines);
			m_MainCheapPipeline = PipelineCompute::Create(state);
		}

		state.ComputeShader = Shader::Create("dof/main.comp", ShaderType::Compute, defines);
		m_MainExpensivePipeline = PipelineCompute::Create(state);
	}
	
	void DOFTask::InitResources()
	{
		// Tile and Neighborhood
		{
			ImageSpecifications specs{};
			specs.Size = glm::uvec3((m_Size + s_DOFTileSize - 1u) / s_DOFTileSize, 1u);
			specs.Usage = ImageUsage::Storage | ImageUsage::Sampled;

			specs.Format = ImageFormat::R16G16_Float; // Min depth, max COC
			m_TileMax = Image::Create(specs, "DOF. Tile Max");
			m_NeighborhoodMax = Image::Create(specs, "DOF. Neighborhood Max");

			specs.Format = ImageFormat::R16_Float; // Min COC
			m_TileMinCOC = Image::Create(specs, "DOF. Tile Min COC");

			BufferSpecifications bufferSpecs{};
			bufferSpecs.Usage = BufferUsage::StorageBuffer | BufferUsage::TransferDst;
			bufferSpecs.Size = specs.Size.x * specs.Size.y * sizeof(uint32_t);
			bufferSpecs.Layout = BufferLayoutType::StorageBuffer;
			m_EarlyExitTiles = Buffer::Create(bufferSpecs, "DOF_EarlyExitTiles");
			m_CheapTiles = Buffer::Create(bufferSpecs, "DOF_CheapTiles");
			m_ExpensiveTiles = Buffer::Create(bufferSpecs, "DOF_ExpensiveTiles");

			specs.Size.y = m_Size.y;
			m_TileMinCOCHorizontal = Image::Create(specs, "DOF. Tile Min COC Horizontal");
			specs.Format = ImageFormat::R16G16_Float; // Min depth, max COC
			m_TileMaxHorizontal = Image::Create(specs, "DOF. Tile Max Horizontal");
		}

		// Presort, Prefilter, Main, Postfilter, Alpha
		{
			ImageSpecifications specs{};
			specs.Size = glm::uvec3(glm::max(glm::uvec2(1), m_Size / 2u), 1u);
			specs.Usage = ImageUsage::Storage | ImageUsage::Sampled;
			specs.Format = ImageFormat::R11G11B10_Float;

			{
				auto temp = specs;
				temp.Usage |= ImageUsage::TransferDst;
				m_Presort = Image::Create(temp, "DOF. Presort");
			}
			m_Prefilter = Image::Create(specs, "DOF. Prefilter");
			m_Main = Image::Create(specs, "DOF. Main");
			m_Postfilter = Image::Create(specs, "DOF. Postfilter");

			specs.Format = ImageFormat::R8_UNorm;
			m_AlphaTemp = Image::Create(specs, "DOF. Alpha Temp");
			m_AlphaResult = Image::Create(specs, "DOF. Alpha Result");
		}
	}
}
