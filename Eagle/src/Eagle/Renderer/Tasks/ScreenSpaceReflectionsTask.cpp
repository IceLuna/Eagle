#include "egpch.h"
#include "ScreenSpaceReflectionsTask.h"

#include "Eagle/Renderer/SceneRenderer.h"
#include "Eagle/Renderer/VidWrappers/RenderCommandManager.h"
#include "Eagle/Renderer/VidWrappers/Texture.h"
#include "Eagle/Asset/Asset.h"

#include "Eagle/Debug/CPUTimings.h"
#include "Eagle/Debug/GPUTimings.h"

namespace Eagle
{
	ScreenSpaceReflectionsTask::ScreenSpaceReflectionsTask(SceneRenderer& renderer)
		: RendererTask(renderer)
	{
		m_Size = m_Renderer.GetViewportSize();
		InitPipeline();
		InitResources();
		InitSizeDependentResources();
	}
	
	void ScreenSpaceReflectionsTask::RecordCommandBuffer(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "Screen-Space Reflections");
		EG_CPU_TIMING_SCOPED("Screen-Space Reflections");

		auto& gbuffer = m_Renderer.GetGBuffer();
		auto& color = m_Renderer.GetHDROutput();
		auto& depth = gbuffer.Depth;

		const ImageLayout oldDepthLayout = depth->GetLayout();
		const ImageLayout oldColorLayout = color->GetLayout();
		cmd->TransitionLayout(depth, oldDepthLayout, ImageReadAccess::PixelShaderRead);
		cmd->TransitionLayout(color, oldColorLayout, ImageLayoutType::StorageImage);

		{
			EG_GPU_TIMING_SCOPED(cmd, "SSSR. Update uniform data");
			EG_CPU_TIMING_SCOPED("SSSR. Update uniform data");
			
			glm::mat4 view = m_Renderer.GetViewMatrix();
			glm::mat4 proj = m_Renderer.GetProjectionMatrix();

			m_UniformData.View = view;
			m_UniformData.Proj = proj;
			m_UniformData.InvProj = glm::inverse(proj);
			m_UniformData.InvView = glm::inverse(view);
			m_UniformData.InvViewProj = glm::inverse(proj * view);
			m_UniformData.PrevViewProj = m_Renderer.GetPrevViewProjection();
			m_UniformData.RoughnessThreshold = m_Renderer.GetOptions_RT().ScreenSpaceReflections.RoughnessThreshold;

			cmd->Write(m_Uniform, &m_UniformData, sizeof(m_UniformData), 0, BufferLayoutType::Unknown, BufferReadAccess::Uniform);
		}

		ClassifyTiles(cmd);
		PrepareIndirectArgs(cmd);
		HZB(cmd);
		Intersection(cmd);
		Reproject(cmd);
		Prefilter(cmd);
		TemporalResolve(cmd);
		Composite(cmd);

		cmd->TransitionLayout(depth, ImageReadAccess::PixelShaderRead, oldDepthLayout);
		cmd->TransitionLayout(color, ImageLayoutType::StorageImage, oldColorLayout);
		cmd->CopyImage(m_Roughness, m_RoughnessHistory, ImageLayoutType::Unknown, ImageReadAccess::PixelShaderRead);

		m_PingPong = (m_PingPong + 1) % 2;
	}

	void ScreenSpaceReflectionsTask::OnResize(const glm::uvec2 size)
	{
		m_Size = size;
		InitSizeDependentResources();
	}

	void ScreenSpaceReflectionsTask::ClassifyTiles(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "SSSR. Classify Tiles");
		EG_CPU_TIMING_SCOPED("SSSR. Classify Tiles");

		struct PushData
		{
			glm::uvec2 Size;
			glm::vec2 TexelSize;
			uint32_t SamplesPerQuad;
		} pushData;

		auto& gbuffer = m_Renderer.GetGBuffer();

		m_ClassifyPipeline->SetBuffer(m_Uniform, 0, 0);
		m_ClassifyPipeline->SetImage(m_Variance[1 - m_PingPong], 0, 1);
		m_ClassifyPipeline->SetBuffer(m_RayCounter, 0, 2);
		m_ClassifyPipeline->SetBuffer(m_DenoiserTileList, 0, 3);
		m_ClassifyPipeline->SetBuffer(m_RayList, 0, 4);
		m_ClassifyPipeline->SetImageSampler(gbuffer.Depth, Sampler::PointSamplerClamp, 0, 5);
		m_ClassifyPipeline->SetImageSampler(gbuffer.Geometry_Shading_Normals, Sampler::PointSamplerClamp, 0, 6);
		m_ClassifyPipeline->SetImageSampler(gbuffer.MaterialData, Sampler::PointSamplerClamp, 0, 7);
		m_ClassifyPipeline->SetImage(m_Radiance[m_PingPong], 0, 8);
		m_ClassifyPipeline->SetImage(m_Roughness, 0, 9);

		const auto& options = m_Renderer.GetOptions_RT().ScreenSpaceReflections;
		pushData.Size = m_Size;
		pushData.TexelSize = 1.f / glm::vec2(m_Size);
		pushData.SamplesPerQuad = options.SamplesPerQuad;

		cmd->TransitionLayout(m_Roughness, ImageLayoutType::Unknown, ImageLayoutType::StorageImage);
		cmd->TransitionLayout(m_Radiance[m_PingPong], ImageLayoutType::Unknown, ImageLayoutType::StorageImage);

		constexpr uint32_t tileSize = 8;
		glm::uvec2 numGroups = { glm::ceil(m_Size.x / float(tileSize)), glm::ceil(m_Size.y / float(tileSize)) };
		cmd->Dispatch(m_ClassifyPipeline, numGroups.x, numGroups.y, 1, &pushData);

		cmd->Barrier(m_RayCounter);
		cmd->Barrier(m_DenoiserTileList);
		cmd->Barrier(m_RayList);
		cmd->Barrier(m_Radiance[m_PingPong]);
		cmd->Barrier(m_Roughness);
	}

	void ScreenSpaceReflectionsTask::PrepareIndirectArgs(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "SSSR. Prepare Indirect Args");
		EG_CPU_TIMING_SCOPED("SSSR. Prepare Indirect Args");

		m_PreparePipeline->SetBuffer(m_RayCounter, 0, 0);
		m_PreparePipeline->SetBuffer(m_IntersectionPassIndirectArgs, 0, 1);

		cmd->TransitionLayout(m_IntersectionPassIndirectArgs, BufferLayoutType::Unknown, BufferLayoutType::StorageBuffer);

		cmd->Dispatch(m_PreparePipeline, 1, 1, 1);

		cmd->Barrier(m_RayCounter);
		cmd->TransitionLayout(m_IntersectionPassIndirectArgs, BufferLayoutType::StorageBuffer, BufferReadAccess::IndirectArgument);
	}

	void ScreenSpaceReflectionsTask::HZB(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "SSSR. HZB generation");
		EG_CPU_TIMING_SCOPED("SSSR. HZB generation");

		constexpr uint32_t tileSize = 8;
		const uint32_t mipCount = m_HZB->GetMipsCount();
		const glm::uvec2 inputSize = m_HZB->GetSize();
		glm::uvec2 mipSize = inputSize;

		struct PushConstants
		{
			glm::uvec2 Size;
			glm::uvec2 PrevSize;
			int PrevMipLevel;
		} pushData;

		{
			auto& depth = m_Renderer.GetGBuffer().Depth;
			std::vector<BufferImageCopy> copyRegion(1);
			copyRegion[0].BufferRowLength = m_Size.x;
			copyRegion[0].BufferImageHeight = m_Size.y;
			copyRegion[0].ImageExtent = glm::uvec3(m_Size, 1u);

			const ImageLayout srcOldLayout = depth->GetLayout();

			cmd->TransitionLayout(depth, srcOldLayout, ImageReadAccess::CopySource);
			cmd->TransitionLayout(m_TempDepthCopy, BufferLayoutType::Unknown, BufferLayoutType::CopyDest);
			cmd->CopyImageToBuffer(depth, m_TempDepthCopy, copyRegion);
			cmd->TransitionLayout(m_TempDepthCopy, BufferLayoutType::CopyDest, BufferReadAccess::CopySource);
			cmd->TransitionLayout(depth, ImageReadAccess::CopySource, srcOldLayout);

			cmd->TransitionLayout(m_HZB, ImageLayoutType::Unknown, ImageLayoutType::CopyDest);
			cmd->CopyBufferToImage(m_TempDepthCopy, m_HZB, copyRegion);
			cmd->TransitionLayout(m_HZB, ImageLayoutType::CopyDest, ImageLayoutType::StorageImage);
		}

		m_HZBPipeline->SetImageSampler(m_HZB, m_HZBSampler, 0, 0);
		m_HZBPipeline->SetImageArray(m_HZB, m_HZBMipViews, 0, 1);

		for (uint32_t mip = 1; mip < mipCount - 1; ++mip)
		{
			pushData.PrevMipLevel = mip - 1;
			pushData.PrevSize = mipSize;
			mipSize >>= 1u;
			pushData.Size = mipSize;

			glm::uvec2 numGroups = { glm::ceil(mipSize.x / float(tileSize)), glm::ceil(mipSize.y / float(tileSize)) };
			if (glm::min(numGroups.x, numGroups.y) == 0)
				break;

			cmd->Dispatch(m_HZBPipeline, numGroups.x, numGroups.y, 1, &pushData);
			cmd->TransitionLayout(m_HZB, m_HZBMipViews[mip - 1], ImageLayoutType::StorageImage, ImageLayoutType::StorageImage);
			cmd->TransitionLayout(m_HZB, m_HZBMipViews[mip], ImageLayoutType::StorageImage, ImageLayoutType::StorageImage);
		}

		cmd->TransitionLayout(m_HZB, ImageLayoutType::StorageImage, ImageReadAccess::PixelShaderRead);
	}

	void ScreenSpaceReflectionsTask::Intersection(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "SSSR. Intersection");
		EG_CPU_TIMING_SCOPED("SSSR. Intersection");

		struct PushData
		{
			glm::uvec2 Size;
			glm::vec2 TexelSize;
			float BlueNoisePhase;
			uint32_t MaxTraversalIntersections;
		} pushData;

		auto& gbuffer = m_Renderer.GetGBuffer();
		m_IntersectionPipeline->SetBuffer(m_Uniform, 0, 0);
		m_IntersectionPipeline->SetImageSampler(gbuffer.Geometry_Shading_Normals, Sampler::PointSamplerClamp, 0, 1);
		m_IntersectionPipeline->SetImageSampler(RenderManager::GetBlueNoise()->GetImage(), Sampler::PointSamplerClamp, 0, 2);
		m_IntersectionPipeline->SetImage(m_Renderer.GetHDROutput(), 0, 3);
		m_IntersectionPipeline->SetImage(m_Radiance[m_PingPong], 0, 4);
		m_IntersectionPipeline->SetImageSampler(m_HZB, m_HZBSampler, 0, 5);
		m_IntersectionPipeline->SetBuffer(m_RayCounter, 0, 6);
		m_IntersectionPipeline->SetBuffer(m_RayList, 0, 7);
		m_IntersectionPipeline->SetImage(m_Roughness, 0, 8);

		const auto& options = m_Renderer.GetOptions_RT().ScreenSpaceReflections;
		pushData.Size = m_Size;
		pushData.TexelSize = 1.f / glm::vec2(m_Size);
		pushData.BlueNoisePhase = RenderManager::GetBlueNoisePhase();
		pushData.MaxTraversalIntersections = options.MaxTraversalIterations;

		cmd->DispatchIndirect(m_IntersectionPipeline, m_IntersectionPassIndirectArgs, 0, &pushData);
		cmd->TransitionLayout(m_Radiance[m_PingPong], ImageLayoutType::StorageImage, ImageReadAccess::PixelShaderRead);
	}

	void ScreenSpaceReflectionsTask::Reproject(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "SSSR. Reproject");
		EG_CPU_TIMING_SCOPED("SSSR. Reproject");

		struct PushData
		{
			glm::uvec2 Size;
		} pushData;

		auto& gbuffer = m_Renderer.GetGBuffer();
		m_ReprojectPipeline->SetBuffer(m_Uniform, 0, 0);
		m_ReprojectPipeline->SetImageSampler(gbuffer.Depth, Sampler::PointSamplerClamp, 0, 1);
		m_ReprojectPipeline->SetImageSampler(gbuffer.DepthHistory, Sampler::PointSamplerClamp, 0, 2);
		m_ReprojectPipeline->SetImageSampler(m_Radiance[m_PingPong], Sampler::PointSamplerClamp, 0, 3);
		m_ReprojectPipeline->SetImageSampler(m_Radiance[1 - m_PingPong], Sampler::PointSamplerClamp, 0, 4);
		m_ReprojectPipeline->SetImageSampler(m_SampleCount[1 - m_PingPong], Sampler::PointSamplerClamp, 0, 5);
		m_ReprojectPipeline->SetImageSampler(gbuffer.Geometry_Shading_Normals, Sampler::PointSamplerClamp, 0, 6);
		m_ReprojectPipeline->SetImageSampler(gbuffer.NormalsHistory, Sampler::PointSamplerClamp, 0, 7);
		m_ReprojectPipeline->SetImageSampler(m_Roughness, Sampler::PointSamplerClamp, 0, 8);
		m_ReprojectPipeline->SetImageSampler(m_RoughnessHistory, Sampler::PointSamplerClamp, 0, 9);
		m_ReprojectPipeline->SetImageSampler(m_AverageRadiance[1 - m_PingPong], Sampler::PointSamplerClamp, 0, 10);
		m_ReprojectPipeline->SetImageSampler(m_Variance[1 - m_PingPong], Sampler::PointSamplerClamp, 0, 11);
		m_ReprojectPipeline->SetImageSampler(gbuffer.Motion, Sampler::PointSamplerClamp, 0, 12);
		m_ReprojectPipeline->SetImage(m_ReprojectedRadiance, 0, 13);
		m_ReprojectPipeline->SetImage(m_AverageRadiance[m_PingPong], 0, 14);
		m_ReprojectPipeline->SetImage(m_Variance[m_PingPong], 0, 15);
		m_ReprojectPipeline->SetImage(m_SampleCount[m_PingPong], 0, 16);
		m_ReprojectPipeline->SetBuffer(m_DenoiserTileList, 0, 17);

		const auto& options = m_Renderer.GetOptions_RT().ScreenSpaceReflections;
		pushData.Size = m_Size;

		//cmd->ClearColorImage(m_ReprojectedRadiance, glm::vec4(0), ImageLayoutType::Unknown, ImageLayoutType::StorageImage);
		//cmd->ClearColorImage(m_AverageRadiance[m_PingPong], glm::vec4(0), ImageLayoutType::Unknown, ImageLayoutType::StorageImage);
		//cmd->ClearColorImage(m_Variance[m_PingPong], glm::vec4(0), ImageLayoutType::Unknown, ImageLayoutType::StorageImage);
		//cmd->ClearColorImage(m_SampleCount[m_PingPong], glm::vec4(0), ImageLayoutType::Unknown, ImageLayoutType::StorageImage);

		cmd->TransitionLayout(m_ReprojectedRadiance, m_ReprojectedRadiance->GetLayout(), ImageLayoutType::StorageImage);
		cmd->TransitionLayout(m_SampleCount[m_PingPong], m_SampleCount[m_PingPong]->GetLayout(), ImageLayoutType::StorageImage);
		cmd->TransitionLayout(m_Variance[m_PingPong], m_Variance[m_PingPong]->GetLayout(), ImageLayoutType::StorageImage);
		cmd->TransitionLayout(m_AverageRadiance[m_PingPong], m_AverageRadiance[m_PingPong]->GetLayout(), ImageLayoutType::StorageImage);

		cmd->TransitionLayout(m_Radiance[1 - m_PingPong], ImageLayoutType::StorageImage, ImageReadAccess::PixelShaderRead);
		cmd->TransitionLayout(m_Variance[1 - m_PingPong], m_Variance[1 - m_PingPong]->GetLayout(), ImageReadAccess::PixelShaderRead);
		cmd->TransitionLayout(m_SampleCount[1 - m_PingPong], m_SampleCount[1 - m_PingPong]->GetLayout(), ImageReadAccess::PixelShaderRead);
		cmd->TransitionLayout(m_AverageRadiance[1 - m_PingPong], m_AverageRadiance[1 - m_PingPong]->GetLayout(), ImageReadAccess::PixelShaderRead);
		cmd->TransitionLayout(m_Roughness, ImageLayoutType::StorageImage, ImageReadAccess::PixelShaderRead);

		cmd->DispatchIndirect(m_ReprojectPipeline, m_IntersectionPassIndirectArgs, 12, &pushData);

		cmd->TransitionLayout(m_ReprojectedRadiance, ImageLayoutType::StorageImage, ImageReadAccess::PixelShaderRead);
		cmd->Barrier(m_SampleCount[m_PingPong]);
		cmd->TransitionLayout(m_AverageRadiance[m_PingPong], ImageLayoutType::StorageImage, ImageReadAccess::PixelShaderRead);
		cmd->TransitionLayout(m_Variance[m_PingPong], ImageLayoutType::StorageImage, ImageReadAccess::PixelShaderRead);
		cmd->TransitionLayout(m_Radiance[1 - m_PingPong], ImageReadAccess::PixelShaderRead, ImageLayoutType::StorageImage);
	}

	void ScreenSpaceReflectionsTask::Prefilter(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "SSSR. Prefilter");
		EG_CPU_TIMING_SCOPED("SSSR. Prefilter");

		struct PushData
		{
			glm::uvec2 Size;
			glm::vec2 TexelSize;
		} pushData;

		auto& gbuffer = m_Renderer.GetGBuffer();
		m_PrefilterPipeline->SetBuffer(m_Uniform, 0, 0);
		m_PrefilterPipeline->SetImageSampler(m_Radiance[m_PingPong], Sampler::PointSamplerClamp, 0, 1);
		m_PrefilterPipeline->SetImageSampler(m_Variance[m_PingPong], Sampler::PointSamplerClamp, 0, 2);
		m_PrefilterPipeline->SetImageSampler(m_AverageRadiance[m_PingPong], Sampler::PointSamplerClamp, 0, 3);
		m_PrefilterPipeline->SetImageSampler(m_Roughness, Sampler::PointSamplerClamp, 0, 4);
		m_PrefilterPipeline->SetImageSampler(gbuffer.Geometry_Shading_Normals, Sampler::PointSamplerClamp, 0, 5);
		m_PrefilterPipeline->SetImageSampler(gbuffer.Depth, Sampler::PointSamplerClamp, 0, 6);
		m_PrefilterPipeline->SetBuffer(m_DenoiserTileList, 0, 7);
		m_PrefilterPipeline->SetImage(m_Radiance[1 - m_PingPong], 0, 8);
		m_PrefilterPipeline->SetImage(m_Variance[1 - m_PingPong], 0, 9);

		pushData.Size = m_Size;
		pushData.TexelSize = 1.f / glm::vec2(m_Size);

		//cmd->ClearColorImage(m_Variance[1 - m_PingPong], glm::vec4(0), ImageLayoutType::Unknown, ImageLayoutType::StorageImage);

		cmd->TransitionLayout(m_Variance[1 - m_PingPong], ImageReadAccess::PixelShaderRead, ImageLayoutType::StorageImage);

		cmd->DispatchIndirect(m_PrefilterPipeline, m_IntersectionPassIndirectArgs, 12, &pushData);

		cmd->TransitionLayout(m_Radiance[1 - m_PingPong], ImageLayoutType::StorageImage, ImageReadAccess::PixelShaderRead);
		cmd->TransitionLayout(m_Variance[1 - m_PingPong], ImageLayoutType::StorageImage, ImageReadAccess::PixelShaderRead);
	}

	void ScreenSpaceReflectionsTask::TemporalResolve(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "SSSR. Temporal Resolve");
		EG_CPU_TIMING_SCOPED("SSSR. Temporal Resolve");

		struct PushData
		{
			glm::uvec2 Size;
			glm::vec2 TexelSize;
		} pushData;

		m_TemporalPipeline->SetBuffer(m_Uniform, 0, 0);
		m_TemporalPipeline->SetImageSampler(m_AverageRadiance[m_PingPong], Sampler::PointSamplerClamp, 0, 1);
		m_TemporalPipeline->SetImageSampler(m_ReprojectedRadiance, Sampler::PointSamplerClamp, 0, 2);
		m_TemporalPipeline->SetImageSampler(m_Radiance[1 - m_PingPong], Sampler::PointSamplerClamp, 0, 3);
		m_TemporalPipeline->SetImageSampler(m_Roughness, Sampler::PointSamplerClamp, 0, 4);
		m_TemporalPipeline->SetImageSampler(m_Variance[1 - m_PingPong], Sampler::PointSamplerClamp, 0, 5);
		m_TemporalPipeline->SetImageSampler(m_SampleCount[1 - m_PingPong], Sampler::PointSamplerClamp, 0, 6);
		m_TemporalPipeline->SetBuffer(m_DenoiserTileList, 0, 7);
		m_TemporalPipeline->SetImage(m_Radiance[m_PingPong], 0, 8);
		m_TemporalPipeline->SetImage(m_Variance[m_PingPong], 0, 9);

		pushData.Size = m_Size;
		pushData.TexelSize = 1.f / glm::vec2(m_Size);

		cmd->TransitionLayout(m_Radiance[m_PingPong], ImageReadAccess::PixelShaderRead, ImageLayoutType::StorageImage);
		cmd->TransitionLayout(m_Variance[m_PingPong], m_Variance[m_PingPong]->GetLayout(), ImageLayoutType::StorageImage);

		cmd->DispatchIndirect(m_TemporalPipeline, m_IntersectionPassIndirectArgs, 12, &pushData);

		cmd->Barrier(m_Radiance[m_PingPong]);
	}

	void ScreenSpaceReflectionsTask::Composite(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "SSSR. Composite");
		EG_CPU_TIMING_SCOPED("SSSR. Composite");

		struct PushData
		{
			glm::uvec2 Size;
			glm::vec2 TexelSize;
			glm::vec3 CameraDir;
		} pushData;

		auto& gbuffer = m_Renderer.GetGBuffer();
		m_CompositePipeline->SetImage(m_Radiance[m_PingPong], 0, 0);
		m_CompositePipeline->SetImageSampler(gbuffer.Geometry_Shading_Normals, Sampler::PointSamplerClamp, 0, 1);
		m_CompositePipeline->SetImageSampler(gbuffer.Albedo, Sampler::PointSamplerClamp, 0, 2);
		m_CompositePipeline->SetImageSampler(gbuffer.MaterialData, Sampler::PointSamplerClamp, 0, 3);
		m_CompositePipeline->SetImageSampler(RenderManager::GetBRDFLUTImage(), Sampler::PointSamplerClamp, 0, 4);
		m_CompositePipeline->SetImage(m_Renderer.GetHDROutput(), 0, 5);

		pushData.Size = m_Size;
		pushData.TexelSize = 1.f / glm::vec2(m_Size);
		pushData.CameraDir = m_Renderer.GetViewDirection();
		//cmd->TransitionLayout(m_Radiance[m_PingPong], ImageReadAccess::PixelShaderRead, ImageLayoutType::StorageImage);

		constexpr uint32_t tileSize = 8;
		glm::uvec2 numGroups = { glm::ceil(m_Size.x / float(tileSize)), glm::ceil(m_Size.y / float(tileSize)) };
		cmd->Dispatch(m_CompositePipeline, numGroups.x, numGroups.y, 1, &pushData);
	}
	
	void ScreenSpaceReflectionsTask::InitPipeline()
	{
		PipelineComputeState state{};

		state.ComputeShader = Shader::Create("screen_space_reflections/classify_tiles.comp", ShaderType::Compute);
		m_ClassifyPipeline = PipelineCompute::Create(state);

		state.ComputeShader = Shader::Create("screen_space_reflections/prepare_indirect_args.comp", ShaderType::Compute);
		m_PreparePipeline = PipelineCompute::Create(state);

		state.ComputeShader = Shader::Create("screen_space_reflections/intersection.comp", ShaderType::Compute);
		m_IntersectionPipeline = PipelineCompute::Create(state);

		state.ComputeShader = Shader::Create("screen_space_reflections/reproject.comp", ShaderType::Compute);
		m_ReprojectPipeline = PipelineCompute::Create(state);

		state.ComputeShader = Shader::Create("screen_space_reflections/prefilter.comp", ShaderType::Compute);
		m_PrefilterPipeline = PipelineCompute::Create(state);

		state.ComputeShader = Shader::Create("screen_space_reflections/temporal.comp", ShaderType::Compute);
		m_TemporalPipeline = PipelineCompute::Create(state);

		state.ComputeShader = Shader::Create("hzb.comp", ShaderType::Compute);
		m_HZBPipeline = PipelineCompute::Create(state);

		state.ComputeShader = Shader::Create("screen_space_reflections/composite.comp", ShaderType::Compute);
		m_CompositePipeline = PipelineCompute::Create(state);
	}

	void ScreenSpaceReflectionsTask::InitResources()
	{
		//==============================Create Tile Classification-related buffers============================================
		{
			constexpr uint32_t rayCounterElementCount = 4;

			BufferSpecifications specs{};
			specs.Layout = BufferLayoutType::StorageBuffer;
			specs.Format = ImageFormat::R32_UInt;
			specs.Usage = BufferUsage::StorageTexelBuffer | BufferUsage::UniformTexelBuffer | BufferUsage::TransferDst | BufferUsage::StorageBuffer;
			specs.Size = rayCounterElementCount * sizeof(uint32_t);
			m_RayCounter = Buffer::Create(specs, "SSSR. Ray Counter");
		}

		//==============================Create PrepareIndirectArgs-related buffers============================================
		{
			uint32_t intersectionPassIndirectArgsElementCount = 6;
			BufferSpecifications specs{};
			specs.Format = ImageFormat::R32_UInt;
			specs.Usage = BufferUsage::StorageTexelBuffer | BufferUsage::IndirectBuffer | BufferUsage::StorageBuffer;
			specs.Size = intersectionPassIndirectArgsElementCount * sizeof(uint32_t);
			m_IntersectionPassIndirectArgs = Buffer::Create(specs, "SSSR. Intersect Indirect Args");
		}

		// Uniform buffer
		{
			BufferSpecifications specs{};
			specs.Usage = BufferUsage::UniformBuffer | BufferUsage::TransferDst;
			specs.Size = sizeof(UniformData);
			m_Uniform = Buffer::Create(specs, "SSSR. Uniform data");
		}
	}

	void ScreenSpaceReflectionsTask::InitSizeDependentResources()
	{
		m_PingPong = 0;

		auto DivideRoundingUp = [](uint32_t a, uint32_t b)
		{
			return (a + b - 1u) / b;
		};

		//==============================Create Tile Classification-related buffers============================================
		{
			uint32_t numTiles = DivideRoundingUp(m_Size.x, 8u) * DivideRoundingUp(m_Size.y, 8u);
			uint32_t numPixels = m_Size.x * m_Size.y;

			uint32_t rayListElementCount = numPixels;
			uint32_t rayCounterElementCount = 1;

			BufferSpecifications specs{};
			specs.Format = ImageFormat::R32_UInt;
			specs.Usage = BufferUsage::StorageTexelBuffer | BufferUsage::UniformTexelBuffer | BufferUsage::StorageBuffer;
			specs.Size = sizeof(uint32_t) * rayListElementCount;
			m_RayList = Buffer::Create(specs, "SSSR. Ray List");
		}
		{
			uint32_t numTiles = DivideRoundingUp(m_Size.x, 8u) * DivideRoundingUp(m_Size.y, 8u);
			uint32_t numPixels = m_Size.x * m_Size.y;

			uint32_t denoiserTileListElementCount = numPixels;
			uint32_t rayCounterElementCount = 1;

			BufferSpecifications specs{};
			specs.Format = ImageFormat::R32_UInt;
			specs.Usage = BufferUsage::StorageTexelBuffer | BufferUsage::UniformTexelBuffer | BufferUsage::StorageBuffer;
			specs.Size = sizeof(uint32_t) * denoiserTileListElementCount;
			m_DenoiserTileList = Buffer::Create(specs, "SSSR. Denoiser Tile List");
		}

		//==============================Create denoising-related resources==============================
		{
			ImageSpecifications radianceSpecs{};
			radianceSpecs.Format = ImageFormat::R16G16B16A16_Float;
			radianceSpecs.Size = glm::uvec3(m_Size, 1u);
			radianceSpecs.Usage = ImageUsage::Sampled | ImageUsage::Storage | ImageUsage::TransferDst;
			m_Radiance[0] = Image::Create(radianceSpecs, "SSSR. Reflection Denoiser - Radiance 0");
			m_Radiance[1] = Image::Create(radianceSpecs, "SSSR. Reflection Denoiser - Radiance 1");
			m_ReprojectedRadiance = Image::Create(radianceSpecs, "SSSR. Reflection Denoiser - Reprojected Radiance");

			ImageSpecifications averageRadianceSpecs{};
			averageRadianceSpecs.Format = ImageFormat::R11G11B10_Float;
			averageRadianceSpecs.Size = glm::uvec3(DivideRoundingUp(m_Size.x, 8u), DivideRoundingUp(m_Size.y, 8u), 1u);
			averageRadianceSpecs.Usage = ImageUsage::Sampled | ImageUsage::Storage | ImageUsage::TransferDst;
			m_AverageRadiance[0] = Image::Create(averageRadianceSpecs, "SSSR. Reflection Denoiser - Average Radiance 0");
			m_AverageRadiance[1] = Image::Create(averageRadianceSpecs, "SSSR. Reflection Denoiser - Average Radiance 1");

			ImageSpecifications varianceSpecs{};
			varianceSpecs.Format = ImageFormat::R16_Float;
			varianceSpecs.Size = glm::uvec3(m_Size, 1u);
			varianceSpecs.Usage = ImageUsage::Sampled | ImageUsage::Storage | ImageUsage::TransferDst;
			m_Variance[0] = Image::Create(varianceSpecs, "SSSR. Reflection Denoiser - Variance 0");
			m_Variance[1] = Image::Create(varianceSpecs, "SSSR. Reflection Denoiser - Variance 1");

			ImageSpecifications sampleCountSpecs = varianceSpecs;
			m_SampleCount[0] = Image::Create(sampleCountSpecs, "SSSR. Reflection Denoiser - Sample Count 0");
			m_SampleCount[1] = Image::Create(sampleCountSpecs, "SSSR. Reflection Denoiser - Sample Count 1");

			ImageSpecifications imgSpecs{};
			imgSpecs.Size = glm::uvec3(m_Size, 1u);
			imgSpecs.Format = ImageFormat::R8_UNorm;
			imgSpecs.Usage = ImageUsage::Sampled | ImageUsage::Storage | ImageUsage::TransferDst | ImageUsage::TransferSrc;
			m_Roughness = Image::Create(imgSpecs, "SSSR. Reflection Denoiser - Extracted Roughness");
			m_RoughnessHistory = Image::Create(imgSpecs, "SSSR. Reflection Denoiser - Extracted Roughness History");
		}

		// HZB
		{
			BufferSpecifications bufferSpecs{};
			bufferSpecs.Usage = BufferUsage::TransferDst | BufferUsage::TransferSrc;
			bufferSpecs.Size = CalculateImageMemorySize(m_Renderer.GetGBuffer().Depth->GetFormat(), m_Size.x, m_Size.y);
			m_TempDepthCopy = Buffer::Create(bufferSpecs, "SSSR. Temp Depth Copy");

			ImageSpecifications specs{};
			specs.Format = ImageFormat::R32_Float;
			specs.Size = glm::uvec3(m_Size, 1u);
			specs.MipsCount = UINT_MAX;
			specs.Usage = ImageUsage::Storage | ImageUsage::Sampled | ImageUsage::TransferSrc | ImageUsage::TransferDst;
			specs.Layout = ImageLayoutType::StorageImage;
			m_HZB = Image::Create(specs, "SSSR. HZB");

			m_HZBMipViews.resize(16);
			std::fill(m_HZBMipViews.begin(), m_HZBMipViews.end(), ImageView{});
			const uint32_t mipsCount = m_HZB->GetMipsCount();
			m_HZBSampler = Sampler::Create(FilterMode::Point, AddressMode::Clamp, CompareOperation::Never, 0.f, float(mipsCount - 1), 1.f);

			for (uint32_t mip = 0; mip < mipsCount; ++mip)
				m_HZBMipViews[mip] = ImageView{ mip };
		}

		RenderManager::Submit([rayCounter = m_RayCounter, radiance = m_Radiance, reprojectionRadiance = m_ReprojectedRadiance, averageRadiance = m_AverageRadiance,
							   variance = m_Variance, sampleCount = m_SampleCount, roughness = m_Roughness, roughnessHistory = m_RoughnessHistory](const Ref<CommandBuffer>& cmd) mutable
		{
			constexpr glm::vec4 clearColor = glm::vec4(0.f);
			cmd->FillBuffer(rayCounter, 0);
			cmd->ClearColorImage(radiance[0], clearColor, ImageLayoutType::Unknown, ImageLayoutType::StorageImage);
			cmd->ClearColorImage(radiance[1], clearColor, ImageLayoutType::Unknown, ImageLayoutType::StorageImage);
			cmd->ClearColorImage(reprojectionRadiance, clearColor, ImageLayoutType::Unknown, ImageLayoutType::StorageImage);
			cmd->ClearColorImage(averageRadiance[0], clearColor, ImageLayoutType::Unknown, ImageLayoutType::StorageImage);
			cmd->ClearColorImage(averageRadiance[1], clearColor, ImageLayoutType::Unknown, ImageLayoutType::StorageImage);
			cmd->ClearColorImage(variance[0], clearColor, ImageLayoutType::Unknown, ImageLayoutType::StorageImage);
			cmd->ClearColorImage(variance[1], clearColor, ImageLayoutType::Unknown, ImageLayoutType::StorageImage);
			cmd->ClearColorImage(sampleCount[0], clearColor, ImageLayoutType::Unknown, ImageLayoutType::StorageImage);
			cmd->ClearColorImage(sampleCount[1], clearColor, ImageLayoutType::Unknown, ImageLayoutType::StorageImage);
			cmd->ClearColorImage(roughness, clearColor, ImageLayoutType::Unknown, ImageLayoutType::StorageImage);
			cmd->ClearColorImage(roughnessHistory, clearColor, ImageLayoutType::Unknown, ImageReadAccess::PixelShaderRead);
		});
	}
}
