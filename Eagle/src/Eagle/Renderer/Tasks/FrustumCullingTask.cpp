#include "egpch.h"
#include "FrustumCullingTask.h"

#include "Eagle/Renderer/RenderManager.h"
#include "Eagle/Renderer/SceneRenderer.h"
#include "Eagle/Renderer/VidWrappers/RenderCommandManager.h"

#include "Eagle/Debug/CPUTimings.h"
#include "Eagle/Debug/GPUTimings.h"

namespace Eagle
{
	static void EnsureSize(const Ref<Buffer>& buffer, size_t required)
	{
		if (buffer->GetSize() < required)
			buffer->Resize((required * 12) / 10);
	}

	template<typename T>
	static void Upload(const Ref<CommandBuffer>& cmd, const Ref<Buffer>& buffer, const std::vector<T>& data)
	{
		const size_t size = data.size() * sizeof(T);
		EnsureSize(buffer, size);
		if (size > 0)
			cmd->Write(buffer, data.data(), size, 0, buffer->GetLayout(), BufferLayoutType::StorageBuffer);
	}

	// @meshes. Meshes that should be used to gather the input data from. The data will be stored on the GPU in the buffers of `data`.
	// @data. Where to store the gather data
	static void Prepare(const Ref<CommandBuffer>& cmd, const MeshDrawDataInfo& meshes, bool bSkeletalMeshes, FrustumCulledMeshes::PerSideData::Data& data)
	{
		data.MeshDatas.clear();
		data.MeshMaterialData.clear();
		data.SkeletalPushDatas.clear();

		SkeletalPushData pushData{};
		for (const auto& mesh : meshes.DrawData)
		{
			MeshData meshData{};
			meshData.MaterialsOffset = (uint32_t)data.MeshMaterialData.size();

			pushData.VertexCount = mesh.VerticesCount;
			pushData.VerticesOffset = mesh.SkinnedVertexOffset;

			for (const auto& matRenderData : mesh.PerMaterialData)
			{
				const uint32_t instanceCount = matRenderData.InstanceCount;
				if (instanceCount > 0)
				{
					data.MeshMaterialData.emplace_back(matRenderData);
					if (bSkeletalMeshes)
					{
						pushData.InstanceOffset = matRenderData.FirstInstance;
						data.SkeletalPushDatas.emplace_back(pushData);
					}
				}
			}
			meshData.MaterialsCount = (uint32_t)data.MeshMaterialData.size() - meshData.MaterialsOffset;

			if (meshData.MaterialsCount > 0)
			{
				meshData.VertexOffset = mesh.VertexOffset;
				meshData.MinAABB = mesh.MeshAABB.Min;
				meshData.MaxAABB = mesh.MeshAABB.Max;
				data.MeshDatas.emplace_back(meshData);
			}
		}

		Upload(cmd, data.MeshDatasBuffer, data.MeshDatas);
		Upload(cmd, data.MeshMaterialsDatasBuffer, data.MeshMaterialData);
		if (bSkeletalMeshes)
			Upload(cmd, data.SkeletalPushDatasBuffer, data.SkeletalPushDatas);
	}

	struct CullContext
	{
		const Ref<CommandBuffer>& Cmd;
		const Ref<PipelineCompute>& Pipeline;
		const CullingFrustumData& Frustum;
		RenderStats& Stats;

		const Ref<Buffer>& Transforms;
		const Ref<Buffer>& SourceInstances;
		const Ref<Buffer>& SkinnedAABBs; // Only used for skeletal meshes
		const MeshesDrawLists& DrawLists;
		bool bSkeletal;

		FrustumCulledMeshes& Output;
	};

	static void Cull_Internal(const CullContext& ctx, FrustumCulledMeshes::PerSideData::Data& data, uint32_t maxDrawCalls)
	{
		const uint32_t numMeshes = data.GetNumMeshes();
		if (numMeshes == 0)
			return;

		const auto& cmd = ctx.Cmd;
		const auto& pipeline = ctx.Pipeline;
		FrustumCullingResult* result = &data.Result;

		const size_t requiredMem = maxDrawCalls * sizeof(DrawIndexedIndirectCommand);
		EnsureSize(result->IndirectArgsBuffer, requiredMem);
		EnsureSize(result->UnculledShadowCastersIndirectArgsBuffer, requiredMem);
		result->MaxDrawCalls = maxDrawCalls;

		pipeline->SetBuffer(data.MeshDatasBuffer, 0, 0);
		pipeline->SetBuffer(data.MeshMaterialsDatasBuffer, 0, 1);
		pipeline->SetBuffer(ctx.SourceInstances, 0, 2);
		pipeline->SetBuffer(ctx.Transforms, 0, 3);
		pipeline->SetBuffer(ctx.Output.InstanceBuffer, 0, 4);
		pipeline->SetBuffer(ctx.Output.UnculledInstanceBuffer, 0, 5);
		pipeline->SetBuffer(result->IndirectArgsBuffer, 0, 6);
		pipeline->SetBuffer(result->DrawCountBuffer, 0, 7);
		pipeline->SetBuffer(result->UnculledShadowCastersIndirectArgsBuffer, 0, 8);
		pipeline->SetBuffer(result->UnculledShadowCastersDrawCountBuffer, 0, 9);

		if (ctx.bSkeletal)
		{
			pipeline->SetBuffer(data.SkeletalPushDatasBuffer, 0, 10);
			pipeline->SetBuffer(ctx.SkinnedAABBs, 0, 11);
		}

		cmd->FillBuffer(result->DrawCountBuffer, 0);
		cmd->FillBuffer(result->UnculledShadowCastersDrawCountBuffer, 0);
		cmd->TransitionLayout(result->IndirectArgsBuffer, result->IndirectArgsBuffer->GetLayout(), BufferLayoutType::StorageBuffer);
		cmd->TransitionLayout(result->UnculledShadowCastersIndirectArgsBuffer, result->UnculledShadowCastersIndirectArgsBuffer->GetLayout(), BufferLayoutType::StorageBuffer);
		cmd->TransitionLayout(result->DrawCountBuffer, result->DrawCountBuffer->GetLayout(), BufferLayoutType::StorageBuffer);
		cmd->TransitionLayout(result->UnculledShadowCastersDrawCountBuffer, result->UnculledShadowCastersDrawCountBuffer->GetLayout(), BufferLayoutType::StorageBuffer);

		{
			struct CullingPushData
			{
				glm::mat4 View = glm::mat4(1);
				CullingFrustum Frustum;
				uint32_t NumMeshes = 0;
				uint32_t MeshOffset = 0;
				uint32_t MaxDrawCalls = 0;
			};
			static_assert(sizeof(CullingPushData) <= 128);

			const auto& cullingData = ctx.Frustum;
			CullingPushData pushData{};
			pushData.View = cullingData.View;
			pushData.Frustum = cullingData.Frustum;
			pushData.NumMeshes = numMeshes;
			pushData.MaxDrawCalls = maxDrawCalls;

			// One group handles one mesh, so that all instances are processed in parallel. Reduces wave divergence
			const uint32_t meshesPerBatch = glm::min(numMeshes, 1024u);
			const uint32_t numBatches = ((numMeshes - 1) / meshesPerBatch) + 1;

			uint32_t meshOffset = 0;
			for (uint32_t i = 0; i < numBatches; ++i)
			{
				pushData.MeshOffset = meshOffset;
				cmd->Dispatch(pipeline, glm::uvec3(meshesPerBatch, 1, 1), &pushData);
				ctx.Stats.Dispatches++;

				meshOffset += meshesPerBatch;
			}

			// Required, otherwise we won't be able to cull again with different buffers
			// Because it would cause old descriptors to be overwritten while in use. So we reset them.
			// TODO: Fix when manual descriptors system is implemented
			pipeline->ResetDescriptors();
		}

		cmd->TransitionLayout(result->IndirectArgsBuffer, result->IndirectArgsBuffer->GetLayout(), BufferReadAccess::IndirectArgument);
		cmd->TransitionLayout(result->UnculledShadowCastersIndirectArgsBuffer, result->UnculledShadowCastersIndirectArgsBuffer->GetLayout(), BufferReadAccess::IndirectArgument);
		cmd->TransitionLayout(result->DrawCountBuffer, result->DrawCountBuffer->GetLayout(), BufferReadAccess::IndirectArgument);
		cmd->TransitionLayout(result->UnculledShadowCastersDrawCountBuffer, result->UnculledShadowCastersDrawCountBuffer->GetLayout(), BufferReadAccess::IndirectArgument);
	}

	static void HandleInstanceBufferAllocation(Ref<Buffer>& buffer, const Ref<Buffer>& srcBuffer)
	{
		if (!buffer)
		{
			BufferSpecifications specs{};
			specs.Usage = BufferUsage::StorageBuffer | BufferUsage::VertexBuffer;
			specs.Size = srcBuffer->GetSize();
			specs.Layout = BufferLayoutType::StorageBuffer;
			buffer = Buffer::Create(specs, "FrustumCulling_CulledInstanceBuffer");
		}
		else
		{
			EnsureSize(buffer, srcBuffer->GetSize());
		}
	}

	static void CullMeshes(const CullContext& ctx)
	{
		const auto& cmd = ctx.Cmd;
		const auto& origInstanceBuffer = ctx.SourceInstances;
		const auto& drawData = ctx.DrawLists;
		FrustumCulledMeshes* output = &ctx.Output;

		HandleInstanceBufferAllocation(output->InstanceBuffer, origInstanceBuffer);
		HandleInstanceBufferAllocation(output->UnculledInstanceBuffer, origInstanceBuffer);

		const BufferLayout origIvbLayout = origInstanceBuffer->GetLayout();
		const BufferLayout ivbLayout = output->InstanceBuffer->GetLayout();
		const BufferLayout unculledIvbLayout = output->UnculledInstanceBuffer->GetLayout();
		cmd->TransitionLayout(origInstanceBuffer, origIvbLayout, BufferLayoutType::StorageBuffer);
		cmd->TransitionLayout(output->InstanceBuffer, ivbLayout, BufferLayoutType::StorageBuffer);
		cmd->TransitionLayout(output->UnculledInstanceBuffer, unculledIvbLayout, BufferLayoutType::StorageBuffer);

		const std::pair<const MeshesDrawLists::SideDrawLists*, FrustumCulledMeshes::PerSideData*> sides[] = {
			{ &drawData.SingleSided, &output->SingleSided },
			{ &drawData.DoubleSided, &output->DoubleSided },
		};

		for (auto [src, dst] : sides)
		{
			for (auto blendMode : magic_enum::enum_values<MaterialBlendMode>())
			{
				const auto& meshes = src->Get(blendMode);
				auto& datas = dst->BlendModes[uint32_t(blendMode)];

				Prepare(cmd, meshes, ctx.bSkeletal, datas);
				Cull_Internal(ctx, datas, meshes.DrawCallsCount);
			}
		}

		cmd->TransitionLayout(origInstanceBuffer, BufferLayoutType::StorageBuffer, origIvbLayout);
		cmd->TransitionLayout(output->InstanceBuffer, BufferLayoutType::StorageBuffer, ivbLayout);
		cmd->TransitionLayout(output->UnculledInstanceBuffer, BufferLayoutType::StorageBuffer, unculledIvbLayout);
	}

	FrustumCullingTask::FrustumCullingTask(SceneRenderer& renderer)
		: RendererTask(renderer)
	{
		PipelineComputeState state{};
		state.ComputeShader = Shader::Create("frustum_culling.comp", ShaderType::Compute);
		m_StaticFrustumCulling = PipelineCompute::Create(state);

		state.ComputeShader = Shader::Create("frustum_culling.comp", ShaderType::Compute, { {"EG_SKELETAL", {}} });
		m_SkeletalFrustumCulling = PipelineCompute::Create(state);

		m_CulledStaticMeshes.Init(false);
		m_CulledSkeletalMeshes.Init(true);
	}

	void FrustumCullingTask::RecordCommandBuffer(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "Frustum Culling Meshes");
		EG_CPU_TIMING_SCOPED("Frustum Culling Meshes");

		{
			EG_GPU_TIMING_SCOPED(cmd, "Frustum Culling Meshes. Static Meshes");
			EG_CPU_TIMING_SCOPED("Frustum Culling Meshes. Static Meshes");
			CullStaticMeshes(cmd);
		}
		{
			EG_GPU_TIMING_SCOPED(cmd, "Frustum Culling Meshes. Skeletal Meshes");
			EG_CPU_TIMING_SCOPED("Frustum Culling Meshes. Skeletal Meshes");
			CullSkeletalMeshes(cmd);
		}
	}

	void FrustumCullingTask::CullStaticMeshes(const Ref<CommandBuffer>& cmd)
	{
		static Ref<Buffer> s_NullBuffer;

		const CullContext ctx{
			.Cmd = cmd,
			.Pipeline = m_StaticFrustumCulling,
			.Frustum = m_Renderer.GetCullingFrustumData(),
			.Stats = m_Renderer.GetStats(),
			.Transforms = m_Renderer.GetMeshTransformsBuffer(),
			.SourceInstances = m_Renderer.GetStaticMeshesBuffers().InstanceBuffer,
			.SkinnedAABBs = s_NullBuffer,
			.DrawLists = m_Renderer.GetStaticMeshesDrawData(),
			.bSkeletal = false,
			.Output = m_CulledStaticMeshes,
		};
		CullMeshes(ctx);
	}

	void FrustumCullingTask::CullSkeletalMeshes(const Ref<CommandBuffer>& cmd)
	{
		const CullContext ctx{
			.Cmd = cmd,
			.Pipeline = m_SkeletalFrustumCulling,
			.Frustum = m_Renderer.GetCullingFrustumData(),
			.Stats = m_Renderer.GetStats(),
			.Transforms = m_Renderer.GetSkeletalMeshTransformsBuffer(),
			.SourceInstances = m_Renderer.GetSkeletalMeshesBuffers().InstanceBuffer,
			.SkinnedAABBs = m_Renderer.GetSkinnedAABBs(),
			.DrawLists = m_Renderer.GetSkeletalMeshesDrawData(),
			.bSkeletal = true,
			.Output = m_CulledSkeletalMeshes,
		};
		CullMeshes(ctx);
	}

	void FrustumCulledMeshes::PerSideData::Data::Init(bool bSkeletalMeshes)
	{
		Result.Init();

		constexpr uint32_t reserveCount = 100;

		BufferSpecifications specs{};
		specs.Usage = BufferUsage::StorageBuffer | BufferUsage::TransferDst;
		specs.Size = reserveCount * sizeof(MeshData);
		specs.Layout = BufferLayoutType::StorageBuffer;
		MeshDatasBuffer = Buffer::Create(specs, "FrustumCulling_MeshesData");

		specs.Size = reserveCount * sizeof(MeshDrawData::MaterialData);
		MeshMaterialsDatasBuffer = Buffer::Create(specs, "FrustumCulling_MaterialsData");

		MeshDatas.reserve(reserveCount);
		MeshMaterialData.reserve(reserveCount);

		if (bSkeletalMeshes)
		{
			specs.Size = reserveCount * sizeof(SkeletalPushData);
			SkeletalPushDatasBuffer = Buffer::Create(specs, "FrustumCulling_SkeletalPushData");

			SkeletalPushDatas.reserve(reserveCount);
		}
	}

	void FrustumCullingResult::Init()
	{
		BufferSpecifications specs{};
		specs.Usage = BufferUsage::StorageBuffer | BufferUsage::IndirectBuffer | BufferUsage::TransferDst;
		specs.Size = sizeof(uint32_t);
		specs.Layout = BufferLayoutType::StorageBuffer;
		DrawCountBuffer = Buffer::Create(specs, "FrustumCulling_DrawCount");
		UnculledShadowCastersDrawCountBuffer = Buffer::Create(specs, "FrustumCulling_DrawCount_ShadowCasters");

		specs.Size = 100 * sizeof(DrawIndexedIndirectCommand);
		IndirectArgsBuffer = Buffer::Create(specs, "FrustumCulling_IndirectArgs");
		UnculledShadowCastersIndirectArgsBuffer = Buffer::Create(specs, "FrustumCulling_IndirectArgs_ShadowCasters");
	}
}
