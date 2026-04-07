#include "egpch.h"
#include "SkinCacheTask.h"

#include "Eagle/Renderer/SceneRenderer.h"
#include "Eagle/Renderer/VidWrappers/RenderCommandManager.h"

#include "Eagle/Debug/CPUTimings.h"
#include "Eagle/Debug/GPUTimings.h"

namespace Eagle
{
	SkinCacheTask::SkinCacheTask(SceneRenderer& renderer)
		: RendererTask(renderer)
	{
		bMotionRequired = m_Renderer.GetOptions().InternalState.bMotionBuffer;
		InitPipeline();

		constexpr size_t baseVertexBufferSize = 1 * 1024 * 1024; // 1 MB
		BufferSpecifications specs{};
		specs.Size = baseVertexBufferSize;
		specs.Layout = BufferLayoutType::StorageBuffer;
		specs.Usage = BufferUsage::StorageBuffer;
		m_SkinnedVertices = Buffer::Create(specs, "SkinnedVertices");
	}

	void SkinCacheTask::RecordCommandBuffer(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "Skin Cache");
		EG_CPU_TIMING_SCOPED("Skin Cache");

		const auto& meshes = m_Renderer.GetSkeletalMeshes();
		if (meshes.empty())
			return;

		size_t totalVertices = 0;
		for (auto& [meshKey, instances] : meshes)
		{
			totalVertices += meshKey.Mesh->GetVerticesCount() * instances.size();
		}

		if (bMotionRequired)
		{
			const size_t requiredSize = totalVertices * sizeof(glm::vec3);
			if (!m_PrevSkinnedVerticesPosition)
			{
				BufferSpecifications specs{};
				specs.Size = (requiredSize * 3) / 2;
				specs.Layout = BufferLayoutType::StorageBuffer;
				specs.Usage = BufferUsage::StorageBuffer;
				m_PrevSkinnedVerticesPosition = Buffer::Create(specs, "SkinnedVerticesPositions_Prev");
			}
			else if (m_PrevSkinnedVerticesPosition->GetSize() < requiredSize)
			{
				m_PrevSkinnedVerticesPosition->Resize((requiredSize * 3) / 2);
			}
		}
		else
		{
			m_PrevSkinnedVerticesPosition.reset();
		}
		const size_t requiredSize = totalVertices * sizeof(Vertex);
		if (m_SkinnedVertices->GetSize() < requiredSize)
		{
			m_SkinnedVertices->Resize((requiredSize * 3) / 2);
			bVerticesValid = false;
		}

		auto& stats = m_Renderer.GetStats();
		const auto& buffers = m_Renderer.GetSkeletalMeshesBuffers();
		m_Pipeline->SetBuffer(buffers.VertexBuffer, 0, 0);
		m_Pipeline->SetBuffer(m_SkinnedVertices, 0, 1);
		m_Pipeline->SetBuffer(m_Renderer.GetSkeletalMeshTransformsBuffer(), 0, 2);
		if (bMotionRequired)
		{
			m_Pipeline->SetBuffer(m_PrevSkinnedVerticesPosition, 0, 3);
		}
		m_Pipeline->SetBufferArray(m_Renderer.GetAnimationTransformsBuffers(), 1, 0);

		cmd->Barrier(m_SkinnedVertices);
		cmd->TransitionLayout(buffers.VertexBuffer, BufferReadAccess::Vertex, BufferLayoutType::StorageBuffer);

		struct PushData
		{
			uint32_t VerticesCount = 0;
			uint32_t VerticesOffset = 0;
			uint32_t DstOffset = 0;
			uint32_t AnimIndex = 0;
			uint32_t PrevVerticesValid = 0;
		} pushData;
		pushData.PrevVerticesValid = bVerticesValid ? 1u : 0u;

		uint32_t dstOffset = 0;
		for (auto& [meshKey, instances] : meshes)
		{
			auto& mesh = meshKey.Mesh;
			const uint32_t verticesCount = (uint32_t)mesh->GetVerticesCount();
			const uint32_t instancesCount = (uint32_t)instances.size();
			
			pushData.VerticesCount = verticesCount;

			constexpr uint32_t groupSize = 128u;
			const uint32_t numGroups = CalcNumGroups(verticesCount, groupSize);
			for (uint32_t i = 0; i < instancesCount; ++i)
			{
				// It doesn't matter which submesh index we take, since `TransformIndex` is going to be the same
				const auto& instanceData = instances[i].SubMeshData[0];
				pushData.AnimIndex = instanceData.PackedTransformIndex & (~EG_RECEIVES_DECALS_MASK);
				pushData.DstOffset = dstOffset + i * verticesCount;

				// TODO: Is it possible/worth to do a 2D dispatch to handle all instances in a single call?
				// Atm, It's tricky to correctly determine anim index for each instance if we batch them into a single 2D dispatch
				cmd->Dispatch(m_Pipeline, numGroups, 1, 1, &pushData);
				++stats.Dispatches;
			}

			pushData.VerticesOffset += verticesCount;
			dstOffset += verticesCount * instancesCount;
		}

		cmd->TransitionLayout(buffers.VertexBuffer, BufferLayoutType::StorageBuffer, BufferReadAccess::Vertex);
		cmd->Barrier(m_SkinnedVertices);

		bVerticesValid = true;
	}
	
	void SkinCacheTask::InitPipeline()
	{
		ShaderDefines defines{};
		if (bMotionRequired)
		{
			defines["EG_OUTPUT_PREV_POSITION"] = "";
		}
		PipelineComputeState state;
		state.ComputeShader = Shader::Create("skin_cache.comp", ShaderType::Compute, defines);
		m_Pipeline = PipelineCompute::Create(state);
	}
}
