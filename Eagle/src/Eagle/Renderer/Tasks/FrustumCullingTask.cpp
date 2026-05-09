#include "egpch.h"
#include "FrustumCullingTask.h"

#include "Eagle/Renderer/RenderManager.h"
#include "Eagle/Renderer/SceneRenderer.h"
#include "Eagle/Renderer/VidWrappers/RenderCommandManager.h"

#include "Eagle/Debug/CPUTimings.h"
#include "Eagle/Debug/GPUTimings.h"

namespace Eagle
{
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

		const size_t requiredMeshesSize = data.MeshDatas.size() * sizeof(MeshData);
		const size_t requiredMaterialsSize = data.MeshMaterialData.size() * sizeof(MeshDrawData::MaterialData);
		if (data.MeshDatasBuffer->GetSize() < requiredMeshesSize)
			data.MeshDatasBuffer->Resize((requiredMeshesSize * 12) / 10);
		if (data.MeshMaterialsDatasBuffer->GetSize() < requiredMaterialsSize)
			data.MeshMaterialsDatasBuffer->Resize((requiredMaterialsSize * 12) / 10);

		if (requiredMeshesSize > 0)
			cmd->Write(data.MeshDatasBuffer, data.MeshDatas.data(), requiredMeshesSize, 0, data.MeshDatasBuffer->GetLayout(), BufferLayoutType::StorageBuffer);
		if (requiredMaterialsSize > 0)
			cmd->Write(data.MeshMaterialsDatasBuffer, data.MeshMaterialData.data(), requiredMaterialsSize, 0, data.MeshMaterialsDatasBuffer->GetLayout(), BufferLayoutType::StorageBuffer);

		if (bSkeletalMeshes)
		{
			const size_t requiredPushDataSize = data.SkeletalPushDatas.size() * sizeof(SkeletalPushData);
			if (data.SkeletalPushDatasBuffer->GetSize() < requiredPushDataSize)
				data.SkeletalPushDatasBuffer->Resize((requiredPushDataSize * 12) / 10);
			if (requiredPushDataSize > 0)
				cmd->Write(data.SkeletalPushDatasBuffer, data.SkeletalPushDatas.data(), requiredPushDataSize, 0, data.SkeletalPushDatasBuffer->GetLayout(), BufferLayoutType::StorageBuffer);
		}
	}

	static void Cull_Internal(const Ref<CommandBuffer>& cmd, const Ref<PipelineCompute>& pipeline, const CullingFrustumData& frustum, RenderStats& stats,
		const Ref<Buffer>& transforms, const FrustumCulledMeshes::PerSideData::Data& data, const Ref<Buffer>& origIVB, const Ref<Buffer>& culledInstanceBuffer, const Ref<Buffer>& unculledInstanceBuffer,
		uint32_t maxDrawCalls, bool bSkeletal, FrustumCullingResult* result)
	{
		const uint32_t numMeshes = data.GetNumMeshes();
		if (numMeshes == 0)
			return;

		const size_t requiredMem = maxDrawCalls * sizeof(DrawIndexedIndirectCommand);
		if (result->IndirectArgsBuffer->GetSize() < requiredMem)
		{
			result->IndirectArgsBuffer->Resize((requiredMem * 12) / 10);
		}
		if (result->UnculledShadowCastersIndirectArgsBuffer->GetSize() < requiredMem)
		{
			result->UnculledShadowCastersIndirectArgsBuffer->Resize((requiredMem * 12) / 10);
		}
		result->MaxDrawCalls = maxDrawCalls;

		pipeline->SetBuffer(data.MeshDatasBuffer, 0, 0);
		pipeline->SetBuffer(data.MeshMaterialsDatasBuffer, 0, 1);
		pipeline->SetBuffer(origIVB, 0, 2);
		pipeline->SetBuffer(transforms, 0, 3);
		pipeline->SetBuffer(culledInstanceBuffer, 0, 4);
		pipeline->SetBuffer(unculledInstanceBuffer, 0, 5);
		pipeline->SetBuffer(result->IndirectArgsBuffer, 0, 6);
		pipeline->SetBuffer(result->DrawCountBuffer, 0, 7);
		pipeline->SetBuffer(result->UnculledShadowCastersIndirectArgsBuffer, 0, 8);
		pipeline->SetBuffer(result->UnculledShadowCastersDrawCountBuffer, 0, 9);

		if (bSkeletal)
		{
			pipeline->SetBuffer(data.SkeletalPushDatasBuffer, 0, 10);
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

			const auto& cullingData = frustum;
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
				stats.Dispatches++;

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
			if (buffer->GetSize() < srcBuffer->GetSize())
				buffer->Resize(srcBuffer->GetSize());
		}
	}

	static void CullMeshes(const Ref<CommandBuffer>& cmd, const Ref<PipelineCompute>& pipeline, const CullingFrustumData& frustum, RenderStats& stats,
		const Ref<Buffer>& transforms, const Ref<Buffer>& origInstanceBuffer, const MeshesDrawLists& drawData, bool bSkeletal, FrustumCulledMeshes* output)
	{
		HandleInstanceBufferAllocation(output->InstanceBuffer, origInstanceBuffer);
		HandleInstanceBufferAllocation(output->UnculledInstanceBuffer, origInstanceBuffer);

		const BufferLayout origIvbLayout = origInstanceBuffer->GetLayout();
		const BufferLayout ivbLayout = output->InstanceBuffer->GetLayout();
		const BufferLayout unculledIvbLayout = output->UnculledInstanceBuffer->GetLayout();
		cmd->TransitionLayout(origInstanceBuffer, origIvbLayout, BufferLayoutType::StorageBuffer);
		cmd->TransitionLayout(output->InstanceBuffer, ivbLayout, BufferLayoutType::StorageBuffer);
		cmd->TransitionLayout(output->UnculledInstanceBuffer, unculledIvbLayout, BufferLayoutType::StorageBuffer);

		{
			const auto& meshes = drawData.SingleSided.Opaque;
			auto& datas = output->SingleSided.BlendModes[uint32_t(MaterialBlendMode::Opaque)];
			Prepare(cmd, meshes, bSkeletal, datas);
			Cull_Internal(cmd, pipeline, frustum, stats, transforms, datas, origInstanceBuffer, output->InstanceBuffer, output->UnculledInstanceBuffer, meshes.DrawCallsCount, bSkeletal, &datas.Result);
		}
		{
			const auto& meshes = drawData.SingleSided.Masked;
			auto& datas = output->SingleSided.BlendModes[uint32_t(MaterialBlendMode::Masked)];
			Prepare(cmd, meshes, bSkeletal, datas);
			Cull_Internal(cmd, pipeline, frustum, stats, transforms, datas, origInstanceBuffer, output->InstanceBuffer, output->UnculledInstanceBuffer, meshes.DrawCallsCount, bSkeletal, &datas.Result);
		}
		{
			const auto& meshes = drawData.SingleSided.Translucent;
			auto& datas = output->SingleSided.BlendModes[uint32_t(MaterialBlendMode::Translucent)];
			Prepare(cmd, meshes, bSkeletal, datas);
			Cull_Internal(cmd, pipeline, frustum, stats, transforms, datas, origInstanceBuffer, output->InstanceBuffer, output->UnculledInstanceBuffer, meshes.DrawCallsCount, bSkeletal, &datas.Result);
		}
		{
			const auto& meshes = drawData.DoubleSided.Opaque;
			auto& datas = output->DoubleSided.BlendModes[uint32_t(MaterialBlendMode::Opaque)];
			Prepare(cmd, meshes, bSkeletal, datas);
			Cull_Internal(cmd, pipeline, frustum, stats, transforms, datas, origInstanceBuffer, output->InstanceBuffer, output->UnculledInstanceBuffer, meshes.DrawCallsCount, bSkeletal, &datas.Result);
		}
		{
			const auto& meshes = drawData.DoubleSided.Masked;
			auto& datas = output->DoubleSided.BlendModes[uint32_t(MaterialBlendMode::Masked)];
			Prepare(cmd, meshes, bSkeletal, datas);
			Cull_Internal(cmd, pipeline, frustum, stats, transforms, datas, origInstanceBuffer, output->InstanceBuffer, output->UnculledInstanceBuffer, meshes.DrawCallsCount, bSkeletal, &datas.Result);
		}
		{
			const auto& meshes = drawData.DoubleSided.Translucent;
			auto& datas = output->DoubleSided.BlendModes[uint32_t(MaterialBlendMode::Translucent)];
			Prepare(cmd, meshes, bSkeletal, datas);
			Cull_Internal(cmd, pipeline, frustum, stats, transforms, datas, origInstanceBuffer, output->InstanceBuffer, output->UnculledInstanceBuffer, meshes.DrawCallsCount, bSkeletal, &datas.Result);
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
		const auto& buffers = m_Renderer.GetStaticMeshesBuffers();
		const auto& ivb = buffers.InstanceBuffer;
		const auto& drawData = m_Renderer.GetStaticMeshesDrawData();
		const auto& transforms = m_Renderer.GetMeshTransformsBuffer();
		const auto& frustum = m_Renderer.GetCullingFrustumData();
		const auto& pipeline = m_StaticFrustumCulling;
		constexpr bool bSkeletal = false;
		auto& stats = m_Renderer.GetStats();

		CullMeshes(cmd, pipeline, frustum, stats, transforms, ivb, drawData, bSkeletal, &m_CulledStaticMeshes);
	}

	void FrustumCullingTask::CullSkeletalMeshes(const Ref<CommandBuffer>& cmd)
	{
		const auto& buffers = m_Renderer.GetSkeletalMeshesBuffers();
		const auto& ivb = buffers.InstanceBuffer;
		const auto& drawData = m_Renderer.GetSkeletalMeshesDrawData();
		const auto& transforms = m_Renderer.GetSkeletalMeshTransformsBuffer();
		const auto& frustum = m_Renderer.GetCullingFrustumData();
		const auto& pipeline = m_SkeletalFrustumCulling;
		constexpr bool bSkeletal = true;
		auto& stats = m_Renderer.GetStats();
		CullMeshes(cmd, pipeline, frustum, stats, transforms, ivb, drawData, bSkeletal, &m_CulledSkeletalMeshes);
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
