#pragma once

#include "RendererTask.h"
#include "Eagle/Classes/StaticMesh.h"
#include "Eagle/Core/GUID.h"
#include "Eagle/Core/Transform.h"

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
	class SpriteComponent;
	class Material;
	class Buffer;
	class SubTexture2D;

	struct QuadVertex
	{
		glm::vec2 TexCoords;
		uint32_t BufferIndex; // Material/Transform index. Yes, they're the same
		int EntityID = -1;
	};

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

	struct MeshGeometryData
	{
		Ref<Buffer> VertexBuffer;
		Ref<Buffer> InstanceBuffer;
		Ref<Buffer> IndexBuffer;

		std::vector<Vertex> Vertices;
		std::vector<PerInstanceData> InstanceVertices;
		std::vector<Index> Indices;
	};

	struct SpriteGeometryData
	{
		Ref<Buffer> VertexBuffer;
		Ref<Buffer> IndexBuffer;

		std::vector<QuadVertex> QuadVertices;
	};

	struct MeshData
	{
		Ref<Material> Material;
		PerInstanceData InstanceData;
	};

	struct SpriteData
	{
		Ref<Material> Material;
		uint32_t EntityID = 0u;
		Ref<SubTexture2D> SubTexture;
		bool bSubTexture = false;
	};

	class GeometryManagerTask : public RendererTask
	{
	public:
		GeometryManagerTask(SceneRenderer& renderer);

		void RecordCommandBuffer(const Ref<CommandBuffer>& cmd) override;
		void InitWithOptions(const SceneRendererSettings& settings) override
		{
			if (bMotionRequired == settings.OptionalGBuffers.bMotion)
				return;

			bMotionRequired = settings.OptionalGBuffers.bMotion;
			if (!bMotionRequired)
			{
				m_MeshesPrevTransformsBuffer.reset();
				m_SpritesPrevTransformsBuffer.reset();
			}
		}

		// ------- Meshes -------
		void SetMeshes(const std::vector<const StaticMeshComponent*>& meshes, bool bDirty);
		void SetTransforms(const std::set<const StaticMeshComponent*>& meshes);
		void ProcessMeshMaterials(const Ref<CommandBuffer>& cmd);
		void ProcessMeshes();
		void UploadMeshes(const Ref<CommandBuffer>& cmd, MeshGeometryData& data, const std::unordered_map<MeshKey, std::vector<MeshData>>& meshes);
		void UploadMeshTransforms(const Ref<CommandBuffer>& cmd);

		// ------- Sprites -------
		void SetSprites(const std::vector<const SpriteComponent*>& sprites, bool bDirty);
		void SetTransforms(const std::set<const SpriteComponent*>& sprites);
		void ProcessSpriteMaterials(const Ref<CommandBuffer>& cmd);
		void ProcessSprites();
		void UploadSprites(const Ref<CommandBuffer>& cmd, SpriteGeometryData& spritesData);
		void UploadSpriteTransforms(const Ref<CommandBuffer>& cmd);
		
		// Mesh getters
		const std::unordered_map<MeshKey, std::vector<MeshData>>& GetAllMeshes() const { return m_Meshes; }
		const std::unordered_map<MeshKey, std::vector<MeshData>>& GetOpaqueMeshes() const { return m_OpaqueMeshes; }
		const std::unordered_map<MeshKey, std::vector<MeshData>>& GetTranslucentMeshes() const { return m_TranslucentMeshes; }

		const MeshGeometryData& GetOpaqueMeshesData() const { return m_OpaqueMeshesData; }
		const MeshGeometryData& GetTranslucentMeshesData() const { return m_TranslucentMeshesData; }
		const Ref<Buffer>& GetMeshesTransformBuffer() const { return m_MeshesTransformsBuffer; }
		const Ref<Buffer>& GetMeshesPrevTransformBuffer() const { return m_MeshesPrevTransformsBuffer; }
		const Ref<Buffer>& GetMeshesMaterialBuffer() const { return m_MeshesMaterialBuffer; }

		// Sprite getters
		const SpriteGeometryData& GetOpaqueSpriteData() const { return m_OpaqueSpritesData; }
		const SpriteGeometryData& GetTranslucentSpriteData() const { return m_TranslucentSpritesData; }
		const Ref<Buffer>& GetSpritesTransformBuffer() const { return m_SpritesTransformsBuffer; }
		const Ref<Buffer>& GetSpritesPrevTransformBuffer() const { return m_SpritesPrevTransformsBuffer; }
		const Ref<Buffer>& GetSpritesMaterialBuffer() const { return m_SpritesMaterialBuffer; }

	private:
		// ------- Sprites -------
		static void AddQuad(std::vector<QuadVertex>& vertices, const SpriteData& sprite, const glm::mat4& transform, uint32_t index);

		//General function that are being called
		static void AddQuad(std::vector<QuadVertex>& vertices, const glm::mat4& transform, const Ref<Material>& material, uint32_t index, int entityID = -1);
		static void AddQuad(std::vector<QuadVertex>& vertices, const glm::mat4& transform, const Ref<SubTexture2D>& subtexture, const Ref<Material>& material, uint32_t index, int entityID = -1);

	private:
		// ------- Meshes -------
		MeshGeometryData m_OpaqueMeshesData;
		MeshGeometryData m_TranslucentMeshesData;
		Ref<Buffer> m_MeshesTransformsBuffer;
		Ref<Buffer> m_MeshesPrevTransformsBuffer;
		Ref<Buffer> m_MeshesMaterialBuffer;

		std::vector<CPUMaterial> m_MeshesMaterials;
		uint32_t m_MeshesCount = 0; // Meshes to draw in total. If we have 1 mesh that will be drawn 5 times using instancing, this will be 5

		// Mesh -> array of its instances
		std::unordered_map<MeshKey, std::vector<MeshData>> m_Meshes; // All meshes
		std::unordered_map<MeshKey, std::vector<MeshData>> m_OpaqueMeshes;
		std::unordered_map<MeshKey, std::vector<MeshData>> m_TranslucentMeshes;
		std::vector<glm::mat4> m_MeshTransforms;

		std::unordered_map<uint32_t, uint64_t> m_MeshTransformIndices; // EntityID -> uint64_t (index to m_MeshTransforms)

		bool bUploadMeshTransforms = true;
		// ------- !Meshes -------

		// ------- Sprites -------
		SpriteGeometryData m_OpaqueSpritesData;
		SpriteGeometryData m_TranslucentSpritesData;
		Ref<Buffer> m_SpritesTransformsBuffer;
		Ref<Buffer> m_SpritesPrevTransformsBuffer;
		Ref<Buffer> m_SpritesMaterialBuffer;
		std::vector<CPUMaterial> m_SpritesMaterials;
		std::vector<glm::mat4> m_SpriteTransforms;

		std::vector<SpriteData> m_Sprites;

		std::unordered_map<uint32_t, uint64_t> m_SpriteTransformIndices; // EntityID -> uint64_t (index to m_SpriteTransforms)

		bool bUploadSpritesTransforms = true;

		static constexpr size_t s_DefaultQuadCount = 512; // How much quads we can render without reallocating
		static constexpr size_t s_DefaultVerticesCount = s_DefaultQuadCount * 4;
		static constexpr size_t s_BaseVertexBufferSize = s_DefaultVerticesCount * sizeof(QuadVertex); //Allocating enough space to store 2048 vertices
		static constexpr size_t s_BaseIndexBufferSize = s_DefaultQuadCount * (sizeof(Index) * 6);

		// ------- !Sprites -------

		bool bMotionRequired = false;
	};
}
