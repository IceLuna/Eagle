#pragma once

#include "RendererTask.h"
#include "Eagle/Renderer/VidWrappers/PipelineGraphics.h"

#include "Eagle/Classes/StaticMesh.h"
#include "Eagle/Core/GUID.h"

struct CPUMaterial;

namespace Eagle
{
	struct MeshKey
	{
		Ref<StaticMesh> Mesh;
		GUID GUID;
		bool bCastsShadows;

		bool operator==(const MeshKey& other) const
		{
			return GUID == other.GUID && bCastsShadows == other.bCastsShadows;
		}
		bool operator!=(const MeshKey& other) const
		{
			return !((*this) == other);
		}
	};
}

namespace std
{
	template <>
	struct hash<Eagle::MeshKey>
	{
		std::size_t operator()(const Eagle::MeshKey& v) const
		{
			size_t result = v.GUID.GetHash();
			Eagle::HashCombine(result, v.bCastsShadows);
			return result;
		}
	};
}

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

		struct PerInstanceData
		{
			union
			{
				glm::uvec3 a_PerInstanceData = glm::uvec3(0u); // .x = TransformIndex; .y = MaterialIndex; .z = ObjectID
				struct
				{
					uint32_t TransformIndex;
					uint32_t MaterialIndex;
					uint32_t ObjectID;
				};
			};
		};

		struct MeshData
		{
			Ref<Material> Material;
			PerInstanceData InstanceData;
		};

		void SetMeshes(const std::vector<const StaticMeshComponent*>& meshes, bool bDirty);
		void UpdateMeshesTransforms(const std::vector<const StaticMeshComponent*>& meshes);

		const auto& GetMeshes() const { return m_Meshes; }

		const Ref<Buffer>& GetVertexBuffer() const { return m_VertexBuffer; }
		const Ref<Buffer>& GetInstanceVertexBuffer() const { return m_InstanceVertexBuffer; }
		const Ref<Buffer>& GetIndexBuffer() const { return m_IndexBuffer; }
		const Ref<Buffer>& GetMeshTransformsBuffer() const { return m_MeshTransformsBuffer; }

		inline static const std::vector<PipelineGraphicsState::VertexInputAttribute> PerInstanceAttribs = { { 4u } }; // Locations of Per-Instance data in shader

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
		Ref<Buffer> m_InstanceVertexBuffer;
		Ref<Buffer> m_IndexBuffer;
		Ref<Buffer> m_MeshTransformsBuffer;
		Ref<Buffer> m_MaterialBuffer;

		std::vector<Vertex> m_Vertices;
		std::vector<PerInstanceData> m_InstanceVertices;
		std::vector<Index> m_Indices;

		std::vector<CPUMaterial> m_Materials;
		uint32_t m_MeshesCount = 0; // Meshes to draw in total. If we have 1 mesh that will be drawn 5 times using instancing, this will be 5

		// Mesh -> array of its instances
		std::unordered_map<MeshKey, std::vector<MeshData>> m_Meshes;
		std::vector<glm::mat4> m_MeshTransforms;

		std::unordered_map<uint32_t, uint64_t> m_MeshTransformIndices; // EntityID -> uint64_t (index to m_MeshTransforms)
		uint64_t m_TexturesUpdatedFrame = 0;
		bool bUploadMeshes = true;
		bool bUploadMeshTransforms = true;
	};
}
