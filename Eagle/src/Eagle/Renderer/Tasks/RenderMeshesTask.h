#pragma once

#include "RendererTask.h"
#include "Eagle/Renderer/VidWrappers/PipelineGraphics.h"

struct CPUMaterial;

namespace Eagle
{
	class StaticMeshComponent;
	class StaticMesh;
	class Material;
	class Buffer;

	class RenderMeshesTask : public RendererTask
	{
	public:
		RenderMeshesTask(SceneRenderer& renderer);

		void RecordCommandBuffer(const Ref<CommandBuffer>& cmd) override;
		void OnResize(const glm::uvec2 size) override { m_Pipeline->Resize(size.x, size.y); }

		struct MeshData
		{
			Ref<StaticMesh> Mesh;
			Ref<Material> Material;
			uint32_t ID = 0;
		};

		void SetMeshes(const std::vector<const StaticMeshComponent*>& meshes, bool bDirty);
		void UpdateMeshesTransforms(const std::vector<const StaticMeshComponent*>& meshes);

		const std::vector<MeshData>& GetMeshes() const { return m_Meshes; }

		const Ref<Buffer>& GetVertexBuffer() const { return m_VertexBuffer; }
		const Ref<Buffer>& GetIndexBuffer() const { return m_IndexBuffer; }
		const Ref<Buffer>& GetMeshTransformsBuffer() const { return m_MeshTransformsBuffer; }

	private:
		void InitPipeline();
		void Render(const Ref<CommandBuffer>& cmd);
		void UpdateMaterials();
		void UploadMaterials(const Ref<CommandBuffer>& cmd);
		void UploadMeshes(const Ref<CommandBuffer>& cmd);
		void UploadTransforms(const Ref<CommandBuffer>& cmd);

	private:
		Ref<PipelineGraphics> m_Pipeline;
		Ref<Buffer> m_VertexBuffer;
		Ref<Buffer> m_IndexBuffer;
		Ref<Buffer> m_MeshTransformsBuffer;
		Ref<Buffer> m_MaterialBuffer;

		std::vector<CPUMaterial> m_Materials;
		std::vector<MeshData> m_Meshes;
		std::vector<glm::mat4> m_MeshTransforms;
		std::unordered_map<uint32_t, uint64_t> m_MeshTransformIndices; // EntityID -> uint64_t (index to m_MeshTransforms)
		uint64_t m_TexturesUpdatedFrame = 0;
		bool bUploadMeshes = true;
		bool bUploadMeshTransforms = true;
	};
}
