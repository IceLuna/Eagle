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
	class TextComponent;
	class Material;
	class Buffer;
	class Texture2D;
	class SubTexture2D;

	struct QuadVertex
	{
		glm::vec2 TexCoords;
		uint32_t TransformIndex;
		uint32_t MaterialIndex;
		int EntityID = -1;
	};

	struct LitTextQuadVertex
	{
		glm::vec4 AlbedoRoughness = glm::vec4(1.f);
		glm::vec4 EmissiveMetallness = glm::vec4(1.f);
		glm::vec3 Position = glm::vec3{ 0.f };
		int EntityID = -1;
		glm::vec2 TexCoord = glm::vec2{ 0.f };
		uint32_t AtlasIndex = 0;
		float AO = 1.f;
		float Opacity = 1.f;
		uint32_t TransformIndex = 0;
	};

	struct UnlitTextQuadVertex
	{
		glm::vec3 Position = glm::vec3{ 0.f };
		glm::vec3 Color = glm::vec3(1.f);
		glm::vec2 TexCoord = glm::vec2{ 0.f };
		int EntityID = -1;
		uint32_t AtlasIndex = 0;
		uint32_t TransformIndex = 0;
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

	struct LitTextGeometryData
	{
		Ref<Buffer> VertexBuffer;
		Ref<Buffer> IndexBuffer;
		std::vector<LitTextQuadVertex> QuadVertices;
	};

	struct UnlitTextGeometryData
	{
		Ref<Buffer> VertexBuffer;
		Ref<Buffer> IndexBuffer;
		std::vector<UnlitTextQuadVertex> QuadVertices;
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
		bool bCastsShadows = true;
	};

	class GeometryManagerTask : public RendererTask
	{
	public:
		GeometryManagerTask(SceneRenderer& renderer);

		void RecordCommandBuffer(const Ref<CommandBuffer>& cmd) override;
		void InitWithOptions(const SceneRendererSettings& settings) override;

		// ------- Meshes -------
		void SetMeshes(const std::vector<const StaticMeshComponent*>& meshes, bool bDirty);
		void SetTransforms(const std::set<const StaticMeshComponent*>& meshes);
		void SortMeshes();
		void UploadMeshes(const Ref<CommandBuffer>& cmd, MeshGeometryData& data, const std::unordered_map<MeshKey, std::vector<MeshData>>& meshes);

		// ------- Sprites -------
		void SetSprites(const std::vector<const SpriteComponent*>& sprites, bool bDirty);
		void SetTransforms(const std::set<const SpriteComponent*>& sprites);
		void SortSprites();
		void UploadSprites(const Ref<CommandBuffer>& cmd, SpriteGeometryData& spritesData);

		// ------- Texts -------
		void SetTexts(const std::vector<const TextComponent*>& texts, bool bDirty);
		void SetTransforms(const std::set<const TextComponent*>& texts);
		void UploadTexts(const Ref<CommandBuffer>& cmd, LitTextGeometryData& textsData);
		void UploadTexts(const Ref<CommandBuffer>& cmd, UnlitTextGeometryData& textsData);

		// Mesh getters
		const std::unordered_map<MeshKey, std::vector<MeshData>>& GetAllMeshes() const { return m_Meshes; }
		const std::unordered_map<MeshKey, std::vector<MeshData>>& GetOpaqueMeshes() const { return m_OpaqueMeshes; }
		const std::unordered_map<MeshKey, std::vector<MeshData>>& GetTranslucentMeshes() const { return m_TranslucentMeshes; }

		const MeshGeometryData& GetOpaqueMeshesData() const { return m_OpaqueMeshesData; }
		const MeshGeometryData& GetTranslucentMeshesData() const { return m_TranslucentMeshesData; }
		const Ref<Buffer>& GetMeshesTransformBuffer() const { return m_MeshesTransformsBuffer; }
		const Ref<Buffer>& GetMeshesPrevTransformBuffer() const { return m_MeshesPrevTransformsBuffer; }

		// Sprite getters
		const SpriteGeometryData& GetOpaqueSpriteData() const { return m_OpaqueSpritesData; }
		const SpriteGeometryData& GetOpaqueNotCastingShadowSpriteData() const { return m_OpaqueNonShadowSpritesData; }
		const SpriteGeometryData& GetTranslucentSpriteData() const { return m_TranslucentSpritesData; }
		const Ref<Buffer>& GetSpritesTransformBuffer() const { return m_SpritesTransformsBuffer; }
		const Ref<Buffer>& GetSpritesPrevTransformBuffer() const { return m_SpritesPrevTransformsBuffer; }

		// Text getters
		const LitTextGeometryData& GetOpaqueLitTextData() const { return m_OpaqueLitTextData; }
		const LitTextGeometryData& GetOpaqueLitNotCastingShadowTextData() const { return m_OpaqueLitNonShadowTextData; }
		const LitTextGeometryData& GetTranslucentLitTextData() const { return m_TranslucentLitTextData; }
		const UnlitTextGeometryData& GetUnlitTextData() const { return m_UnlitTextData; }
		const UnlitTextGeometryData& GetUnlitNotCastingShadowTextData() const { return m_UnlitNonShadowTextData; }
		const Ref<Buffer>& GetTextsTransformBuffer() const { return m_TextTransformsBuffer; }
		const Ref<Buffer>& GetTextsPrevTransformBuffer() const { return m_TextPrevTransformsBuffer; }
		const std::vector<Ref<Texture2D>>& GetAtlases() const { return m_Atlases; }

	private:
		// ------- Sprites -------
		static void AddQuad(std::vector<QuadVertex>& vertices, const SpriteData& sprite, const glm::mat4& transform, uint32_t transformIndex);

		//General function that are being called
		static void AddQuad(std::vector<QuadVertex>& vertices, const glm::mat4& transform, const Ref<Material>& material, uint32_t transformIndex, int entityID = -1);
		static void AddQuad(std::vector<QuadVertex>& vertices, const glm::mat4& transform, const Ref<SubTexture2D>& subtexture, const Ref<Material>& material, uint32_t transformIndex, int entityID = -1);

	private:
		// ------- Meshes -------
		MeshGeometryData m_OpaqueMeshesData;
		MeshGeometryData m_TranslucentMeshesData;
		Ref<Buffer> m_MeshesTransformsBuffer;
		Ref<Buffer> m_MeshesPrevTransformsBuffer;

		// Mesh -> array of its instances
		std::unordered_map<MeshKey, std::vector<MeshData>> m_Meshes; // All meshes
		std::unordered_map<MeshKey, std::vector<MeshData>> m_OpaqueMeshes;
		std::unordered_map<MeshKey, std::vector<MeshData>> m_TranslucentMeshes;
		std::vector<glm::mat4> m_MeshTransforms;
		std::vector<uint64_t> m_MeshUploadSpecificTransforms; // Instead of uploading all transforms, upload just required transforms. uint - index to "std::vector<glm::mat4> transforms"

		std::unordered_map<uint32_t, uint64_t> m_MeshTransformIndices; // EntityID -> uint64_t (index to m_MeshTransforms)

		bool bUploadMeshTransforms = true;
		bool bUploadMeshSpecificTransforms = false;
		bool bUploadMeshes = true;

		static constexpr size_t s_MeshesBaseVertexBufferSize = 1 * 1024 * 1024; // 1 MB
		static constexpr size_t s_MeshesBaseIndexBufferSize = 1 * 1024 * 1024; // 1 MB
		// ------- !Meshes -------

		// ------- Sprites -------
		SpriteGeometryData m_OpaqueSpritesData;
		SpriteGeometryData m_OpaqueNonShadowSpritesData; // Quads that don't cast shadows
		SpriteGeometryData m_TranslucentSpritesData;
		Ref<Buffer> m_SpritesTransformsBuffer;
		Ref<Buffer> m_SpritesPrevTransformsBuffer;
		std::vector<glm::mat4> m_SpriteTransforms;
		std::vector<uint64_t> m_SpriteUploadSpecificTransforms; // Instead of uploading all transforms, upload just required transforms. uint - index to "std::vector<glm::mat4> transforms"

		std::vector<SpriteData> m_Sprites;

		std::unordered_map<uint32_t, uint64_t> m_SpriteTransformIndices; // EntityID -> uint64_t (index to m_SpriteTransforms)

		bool bUploadSpritesTransforms = true;
		bool bUploadSpritesSpecificTransforms = false;
		bool bUploadSprites = true;

		static constexpr size_t s_SpritesDefaultQuadCount = 64; // How much quads we can render without reallocating
		static constexpr size_t s_SpritesDefaultVerticesCount = s_SpritesDefaultQuadCount * 4;
		static constexpr size_t s_SpritesBaseVertexBufferSize = s_SpritesDefaultVerticesCount * sizeof(QuadVertex);
		static constexpr size_t s_SpritesBaseIndexBufferSize = s_SpritesDefaultQuadCount * (sizeof(Index) * 6);

		// ------- !Sprites -------
		
		// ------- Text 3D -------
		Ref<Buffer> m_TextTransformsBuffer;
		Ref<Buffer> m_TextPrevTransformsBuffer;
		std::unordered_map<Ref<Texture2D>, uint32_t> m_FontAtlases;
		std::vector<Ref<Texture2D>> m_Atlases;

		// ------- Lit Text 3D -------
		LitTextGeometryData m_OpaqueLitTextData;
		LitTextGeometryData m_OpaqueLitNonShadowTextData;
		LitTextGeometryData m_TranslucentLitTextData;
		UnlitTextGeometryData m_UnlitTextData;
		UnlitTextGeometryData m_UnlitNonShadowTextData;

		std::vector<glm::mat4> m_TextTransforms;
		std::vector<uint64_t> m_TextUploadSpecificTransforms; // Instead of uploading all transforms, upload just required transforms. uint - index to "std::vector<glm::mat4> transforms"
		std::unordered_map<uint32_t, uint64_t> m_TextTransformIndices; // EntityID -> uint64_t (index to m_TextTransformIndices)

		bool bUploadTextQuads = true;
		bool bUploadTextTransforms = true;
		bool bUploadTextSpecificTransforms = false;

		static constexpr size_t s_TextDefaultQuadCount = 64; // How much quads we can render without reallocating
		static constexpr size_t s_TextDefaultVerticesCount = s_TextDefaultQuadCount * 4;

		static constexpr size_t s_LitTextBaseVertexBufferSize = s_TextDefaultVerticesCount * sizeof(LitTextQuadVertex);
		static constexpr size_t s_LitTextBaseIndexBufferSize  = s_TextDefaultQuadCount * (sizeof(Index) * 6);

		static constexpr size_t s_UnlitTextBaseVertexBufferSize = s_TextDefaultVerticesCount * sizeof(UnlitTextQuadVertex);
		static constexpr size_t s_UnlitTextBaseIndexBufferSize  = s_TextDefaultQuadCount * (sizeof(Index) * 6);
		// ------- !Lit Text 3D -------

		bool bMotionRequired = false;
	};
}
