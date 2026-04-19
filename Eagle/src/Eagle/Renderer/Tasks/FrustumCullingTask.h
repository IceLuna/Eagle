#pragma once

#include "RendererTask.h"
#include "Eagle/Renderer/VidWrappers/PipelineCompute.h"

#include "GeometryManagerTask.h"

namespace Eagle
{
	struct FrustumCullingResult
	{
		Ref<Buffer> IndirectArgsBuffer;
		Ref<Buffer> DrawCountBuffer;
		uint32_t MaxDrawCalls = 0;

		void Init();
	};

	struct MeshData
	{
		glm::vec3 MinAABB = glm::vec3(0);
		uint32_t MaterialsCount = 0;
		glm::vec3 MaxAABB = glm::vec3(0);
		uint32_t MaterialsOffset = 0;
		uint32_t VertexOffset = 0;
	};

	struct SkeletalPushData
	{
		uint32_t VertexCount = 0;
		uint32_t InstanceOffset = 0;
		uint32_t VerticesOffset = 0;
	};

	struct FrustumCulledMeshes
	{
		struct PerSideData
		{
			struct Data
			{
				FrustumCullingResult Result;

				// These hold input data that's required for the frustum culling pass.
				// They're prepared by `FrustumCulling` pass once at the start of the frame, no need to modify them.
				// Just pass them to the frustum culling and get results into the "Result"
				Ref<Buffer> MeshDatasBuffer;
				Ref<Buffer> MeshMaterialsDatasBuffer;
				Ref<Buffer> SkeletalPushDatasBuffer;
				std::vector<MeshData> MeshDatas;
				std::vector<MeshDrawData::MaterialData> MeshMaterialData;
				std::vector<SkeletalPushData> SkeletalPushDatas;

				void Init(bool bSkeletalMeshes);
				uint32_t GetNumMeshes() const { return (uint32_t)MeshDatas.size(); }
			} Opaque, Masked, Translucent;

			void Init(bool bSkeletalMeshes)
			{
				Opaque.Init(bSkeletalMeshes);
				Masked.Init(bSkeletalMeshes);
				Translucent.Init(bSkeletalMeshes);
			}
		} SingleSided, DoubleSided;

		Ref<Buffer> InstanceBuffer;

		void Init(bool bSkeletalMeshes)
		{
			SingleSided.Init(bSkeletalMeshes);
			DoubleSided.Init(bSkeletalMeshes);
		}
	};

	// Performs frustum culling of static/skeletal meshes. By default, it uses the main camera's frustum.
	// In order to run it with a custom frustum, call a separate "Run()" function
	class FrustumCullingTask : public RendererTask
	{
	public:
		FrustumCullingTask(SceneRenderer& renderer);
		void RecordCommandBuffer(const Ref<CommandBuffer>& cmd) override;

		// Culled for the main camera
		const FrustumCulledMeshes& GetCulledStaticMeshes() const { return m_CulledStaticMeshes; }
		const FrustumCulledMeshes& GetCulledSkeletalMeshes() const { return m_CulledSkeletalMeshes; }

	private:
		void CullStaticMeshes(const Ref<CommandBuffer>& cmd);
		void CullSkeletalMeshes(const Ref<CommandBuffer>& cmd);

	private:
		FrustumCulledMeshes m_CulledStaticMeshes;
		FrustumCulledMeshes m_CulledSkeletalMeshes;
		Ref<PipelineCompute> m_StaticFrustumCulling;
		Ref<PipelineCompute> m_SkeletalFrustumCulling;
	};
}
