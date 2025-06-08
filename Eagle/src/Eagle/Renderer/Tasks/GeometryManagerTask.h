#pragma once

#include "RendererTask.h"
#include "Eagle/Classes/StaticMesh.h"
#include "Eagle/Classes/SkeletalMesh.h"
#include "Eagle/Core/GUID.h"
#include "Eagle/Math/Transform.h"

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

	struct SkeletalMeshKey
	{
		Ref<SkeletalMesh> Mesh;
		GUID GUID;
		bool bCastsShadows;

		bool operator==(const SkeletalMeshKey& other) const
		{
			return GUID == other.GUID && bCastsShadows == other.bCastsShadows;
		}
		bool operator!=(const SkeletalMeshKey& other) const
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

	template <>
	struct hash<Eagle::SkeletalMeshKey>
	{
		std::size_t operator()(const Eagle::SkeletalMeshKey& v) const
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
	class SkeletalMeshComponent;
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
		glm::vec2 Position = glm::vec2{ 0.f };
		glm::vec2 TexCoord = glm::vec2{ 0.f };
		int EntityID = -1;
		uint32_t AtlasIndex = 0;
		uint32_t TransformIndex = 0;
		uint32_t MaterialIndex = 0;
	};

	struct UnlitTextQuadVertex
	{
		glm::vec3 Color = glm::vec3(1.f);
		glm::vec2 Position = glm::vec2{ 0.f };
		glm::vec2 TexCoord = glm::vec2{ 0.f };
		int EntityID = -1;
		uint32_t AtlasIndex = 0;
		uint32_t TransformIndex = 0;
	};

	struct PerInstanceData
	{
		union
		{
			glm::uvec3 Data = glm::uvec3(0u); // .x = TransformIndex; .y = MaterialIndex; .z = ObjectID
			struct
			{
				uint32_t PackedTransformIndex; // The highest is a flag whether a mesh receives decals
				uint32_t MaterialIndex;
				uint32_t ObjectID;
			};
		};
	};

	struct SkeletalPerInstanceData
	{
		union
		{
			glm::uvec4 Data = glm::uvec4(0u); // .x = TransformIndex; .y = MaterialIndex; .z = ObjectID; w = AnimTransformIndex
			struct
			{
				uint32_t PackedTransformIndex; // The highest bit is a flag whether a mesh receives decals or not
				uint32_t MaterialIndex;
				uint32_t ObjectID;
				uint32_t AnimTransformIndex;
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

	struct SkeletalMeshGeometryData
	{
		Ref<Buffer> VertexBuffer;
		Ref<Buffer> InstanceBuffer;
		Ref<Buffer> IndexBuffer;

		std::vector<SkeletalVertex> Vertices;
		std::vector<SkeletalPerInstanceData> InstanceVertices;
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
		std::vector<Ref<Material>> Materials;
		std::vector<PerInstanceData> InstanceDatas;
	};

	struct MeshDatas
	{
		std::vector<MeshData> Datas;
		std::vector<uint32_t> MaterialSlots; // Material slots to use
	};

	struct SkeletalMeshData
	{
		std::vector<Ref<Material>> Materials;
		std::vector<SkeletalPerInstanceData> InstanceDatas;
	};

	struct SkeletalMeshDatas
	{
		std::vector<SkeletalMeshData> Datas;
		std::vector<uint32_t> MaterialSlots; // Material slots to use
	};

	struct SpriteData
	{
		Ref<Material> Material;
		glm::vec2 AtlasSpriteUVs[4];
		uint32_t EntityID = 0u;
		bool bAtlas = false;
		bool bCastsShadows = true;
		bool bReceivesDecals = true;
	};

	class GeometryManagerTask : public RendererTask
	{
	public:
		GeometryManagerTask(SceneRenderer& renderer);

		void RecordCommandBuffer(const Ref<CommandBuffer>& cmd) override;
		void InitWithOptions(const SceneRendererSettings& settings) override;

		// ------- Meshes -------
		void SetMeshes(const std::vector<const StaticMeshComponent*>& meshes, bool bDirty);
		void SetTransforms(const std::unordered_set<const StaticMeshComponent*>& meshes);
		void SortMeshes();
		void UploadMeshes(const Ref<CommandBuffer>& cmd, MeshGeometryData& data, const std::unordered_map<MeshKey, MeshDatas>& meshes);

		// ------- Skeletal Meshes -------
		void SetSkeletalMeshes(const std::vector<SkeletalMeshComponent*>& meshes, bool bDirty);
		void SetTransforms(const std::unordered_set<const SkeletalMeshComponent*>& meshes);
		void SortSkeletalMeshes();
		void UploadSkeletalMeshes(const Ref<CommandBuffer>& cmd, SkeletalMeshGeometryData& data, const std::unordered_map<SkeletalMeshKey, SkeletalMeshDatas>& meshes);

		// ------- Sprites -------
		void SetSprites(const std::vector<const SpriteComponent*>& sprites, bool bDirty);
		void SetTransforms(const std::unordered_set<const SpriteComponent*>& sprites);
		void SortSprites();
		void UploadSprites(const Ref<CommandBuffer>& cmd, SpriteGeometryData& spritesData);

		// ------- Texts -------
		void SetTexts(const std::vector<const TextComponent*>& texts, bool bDirty);
		void SetTransforms(const std::unordered_set<const TextComponent*>& texts);
		void SortLitTexts();
		void UploadTexts(const Ref<CommandBuffer>& cmd, LitTextGeometryData& textsData);
		void UploadTexts(const Ref<CommandBuffer>& cmd, UnlitTextGeometryData& textsData);

		// Mesh getters
		const std::unordered_map<MeshKey, MeshDatas>& GetAllMeshes() const { return m_Meshes; }
		const std::unordered_map<MeshKey, MeshDatas>& GetOpaqueMeshes() const { return m_OpaqueMeshes; }
		const std::unordered_map<MeshKey, MeshDatas>& GetTranslucentMeshes() const { return m_TranslucentMeshes; }
		const std::unordered_map<MeshKey, MeshDatas>& GetMaskedMeshes() const { return m_MaskedMeshes; }

		const MeshGeometryData& GetOpaqueMeshesData() const { return m_OpaqueMeshesData; }
		const MeshGeometryData& GetTranslucentMeshesData() const { return m_TranslucentMeshesData; }
		const MeshGeometryData& GetMaskedMeshesData() const { return m_MaskedMeshesData; }
		const Ref<Buffer>& GetMeshesTransformBuffer() const { return m_MeshesTransformsBuffer; }
		const Ref<Buffer>& GetMeshesPrevTransformBuffer() const { return m_MeshesPrevTransformsBuffer; }

		// Skeletal Mesh getters
		const std::unordered_map<SkeletalMeshKey, SkeletalMeshDatas>& GetAllSkeletalMeshes() const { return m_SkeletalMeshes; }
		const std::unordered_map<SkeletalMeshKey, SkeletalMeshDatas>& GetOpaqueSkeletalMeshes() const { return m_OpaqueSkeletalMeshes; }
		const std::unordered_map<SkeletalMeshKey, SkeletalMeshDatas>& GetTranslucentSkeletalMeshes() const { return m_TranslucentSkeletalMeshes; }
		const std::unordered_map<SkeletalMeshKey, SkeletalMeshDatas>& GetMaskedSkeletalMeshes() const { return m_MaskedSkeletalMeshes; }

		const SkeletalMeshGeometryData& GetOpaqueSkeletalMeshesData() const { return m_OpaqueSkeletalMeshesData; }
		const SkeletalMeshGeometryData& GetTranslucentSkeletalMeshesData() const { return m_TranslucentSkeletalMeshesData; }
		const SkeletalMeshGeometryData& GetMaskedSkeletalMeshesData() const { return m_MaskedSkeletalMeshesData; }
		const Ref<Buffer>& GetSkeletalMeshesTransformBuffer() const { return m_SkeletalMeshesTransformsBuffer; }
		const Ref<Buffer>& GetSkeletalMeshesPrevTransformBuffer() const { return m_SkeletalMeshesPrevTransformsBuffer; }

		const std::vector<std::vector<glm::mat4>>& GetAnimationTransforms() const { return m_AnimationTransforms; }
		const std::vector<Ref<Buffer>>& GetAnimationTransformsBuffers() const { return m_AnimationTransformsBuffers; }
		const std::vector<Ref<Buffer>>& GetAnimationPrevTransformsBuffers() const { return m_AnimationPrevTransformsBuffers; }

		// Sprite getters
		const SpriteGeometryData& GetOpaqueSpriteData() const { return m_OpaqueSpritesData; }
		const SpriteGeometryData& GetOpaqueNotCastingShadowSpriteData() const { return m_OpaqueNonShadowSpritesData; }
		const SpriteGeometryData& GetMaskedSpriteData() const { return m_MaskedSpritesData; }
		const SpriteGeometryData& GetMaskedNotCastingShadowSpriteData() const { return m_MaskedNonShadowSpritesData; }
		const SpriteGeometryData& GetTranslucentSpriteData() const { return m_TranslucentSpritesData; }
		const SpriteGeometryData& GetTranslucentNotCastingShadowSpriteData() const { return m_TranslucentNonShadowSpritesData; }
		const Ref<Buffer>& GetSpritesTransformBuffer() const { return m_SpritesTransformsBuffer; }
		const Ref<Buffer>& GetSpritesPrevTransformBuffer() const { return m_SpritesPrevTransformsBuffer; }

		// Text getters
		const LitTextGeometryData& GetOpaqueLitTextData() const { return m_OpaqueLitTextData; }
		const LitTextGeometryData& GetOpaqueLitNotCastingShadowTextData() const { return m_OpaqueLitNonShadowTextData; }
		const LitTextGeometryData& GetMaskedLitTextData() const { return m_MaskedLitTextData; }
		const LitTextGeometryData& GetMaskedLitNotCastingShadowTextData() const { return m_MaskedLitNonShadowTextData; }
		const LitTextGeometryData& GetTranslucentLitTextData() const { return m_TranslucentLitTextData; }
		const LitTextGeometryData& GetTranslucentLitNotCastingShadowTextData() const { return m_TranslucentNonShadowLitTextData; }
		const UnlitTextGeometryData& GetUnlitTextData() const { return m_UnlitTextData; }
		const UnlitTextGeometryData& GetUnlitNotCastingShadowTextData() const { return m_UnlitNonShadowTextData; }
		const Ref<Buffer>& GetTextsTransformBuffer() const { return m_TextTransformsBuffer; }
		const Ref<Buffer>& GetTextsPrevTransformBuffer() const { return m_TextPrevTransformsBuffer; }
		const std::vector<Ref<Texture2D>>& GetAtlases() const { return m_Atlases; }

	private:
		// ------- Sprites -------
		static void AddQuad(std::vector<QuadVertex>& vertices, const SpriteData& sprite, const glm::mat4& transform, uint32_t transformIndex);

		// General function that is being called
		static void AddQuad(std::vector<QuadVertex>& vertices, const glm::mat4& transform, const Ref<Material>& material, uint32_t transformIndex, const glm::vec2 UVs[4], int entityID = -1);

		void UploadAnimationTransforms(const Ref<CommandBuffer>& cmd, bool bTransformsGarbage);

	private:
		// ------- Meshes -------
		MeshGeometryData m_OpaqueMeshesData;
		MeshGeometryData m_TranslucentMeshesData;
		MeshGeometryData m_MaskedMeshesData;
		Ref<Buffer> m_MeshesTransformsBuffer;
		Ref<Buffer> m_MeshesPrevTransformsBuffer;

		// Mesh -> array of its instances
		std::unordered_map<MeshKey, MeshDatas> m_Meshes; // All meshes
		std::unordered_map<MeshKey, MeshDatas> m_OpaqueMeshes;
		std::unordered_map<MeshKey, MeshDatas> m_TranslucentMeshes;
		std::unordered_map<MeshKey, MeshDatas> m_MaskedMeshes;
		std::vector<glm::mat4> m_MeshTransforms;
		std::vector<uint64_t> m_MeshUploadSpecificTransforms; // Instead of uploading all transforms, upload just required transforms. uint - index to "std::vector<glm::mat4> transforms"

		std::unordered_map<uint32_t, uint64_t> m_MeshTransformIndices; // EntityID -> uint64_t (index to m_MeshTransforms)

		bool bUploadMeshTransforms = true;
		bool bUploadMeshSpecificTransforms = false;
		bool bUploadMeshes = true;

		static constexpr size_t s_MeshesBaseVertexBufferSize = 1 * 1024 * 1024; // 1 MB
		static constexpr size_t s_MeshesBaseIndexBufferSize = 1 * 1024 * 1024; // 1 MB
		// ------- !Meshes -------

		// ------- Skeletal Meshes -------
		SkeletalMeshGeometryData m_OpaqueSkeletalMeshesData;
		SkeletalMeshGeometryData m_TranslucentSkeletalMeshesData;
		SkeletalMeshGeometryData m_MaskedSkeletalMeshesData;
		Ref<Buffer> m_SkeletalMeshesTransformsBuffer;
		Ref<Buffer> m_SkeletalMeshesPrevTransformsBuffer;

		// Mesh -> array of its instances
		std::unordered_map<SkeletalMeshKey, SkeletalMeshDatas> m_SkeletalMeshes; // All meshes
		std::unordered_map<SkeletalMeshKey, SkeletalMeshDatas> m_OpaqueSkeletalMeshes;
		std::unordered_map<SkeletalMeshKey, SkeletalMeshDatas> m_TranslucentSkeletalMeshes;
		std::unordered_map<SkeletalMeshKey, SkeletalMeshDatas> m_MaskedSkeletalMeshes;
		std::vector<glm::mat4> m_SkeletalMeshTransforms;
		std::vector<uint64_t> m_SkeletalMeshUploadSpecificTransforms; // Instead of uploading all transforms, upload just required transforms. uint - index to "std::vector<glm::mat4> transforms"

		std::unordered_map<uint32_t, uint64_t> m_SkeletalMeshTransformIndices; // EntityID -> uint64_t (index to m_MeshTransforms)

		// Transforms of animations
		std::vector<std::vector<glm::mat4>> m_AnimationTransforms;
		std::vector<Ref<Buffer>> m_AnimationTransformsBuffers;
		std::vector<Ref<Buffer>> m_AnimationPrevTransformsBuffers;

		bool bUploadSkeletalMeshTransforms = true;
		bool bUploadSkeletalMeshSpecificTransforms = false;
		bool bUploadSkeletalMeshes = true;

		// ------- !Skeletal Meshes -------

		// ------- Sprites -------
		SpriteGeometryData m_OpaqueSpritesData;
		SpriteGeometryData m_OpaqueNonShadowSpritesData; // Quads that don't cast shadows
		SpriteGeometryData m_MaskedSpritesData;
		SpriteGeometryData m_MaskedNonShadowSpritesData; // Quads that don't cast shadows
		SpriteGeometryData m_TranslucentSpritesData;
		SpriteGeometryData m_TranslucentNonShadowSpritesData; // Quads that don't cast shadows

		Ref<Buffer> m_SpritesTransformsBuffer;
		Ref<Buffer> m_SpritesPrevTransformsBuffer;
		std::vector<glm::mat4> m_SpriteTransforms;
		std::vector<uint64_t> m_SpriteUploadSpecificTransforms; // Instead of uploading all transforms, upload just required transforms. uint - index to "std::vector<glm::mat4> transforms"

		std::vector<SpriteData> m_Sprites;

		std::unordered_map<uint32_t, uint64_t> m_SpriteTransformIndices; // EntityID -> uint64_t (index to m_SpriteTransforms)

		bool bUploadSpritesTransforms = true;
		bool bUploadSpritesSpecificTransforms = false;
		bool bUploadSprites = true;

		// ------- !Sprites -------
		
		// ------- Text 3D -------
		Ref<Buffer> m_TextTransformsBuffer;
		Ref<Buffer> m_TextPrevTransformsBuffer;
		std::unordered_map<Ref<Texture2D>, uint32_t> m_FontAtlases;
		std::vector<Ref<Texture2D>> m_Atlases;
		std::unordered_map<uint32_t, Ref<Material>> m_TextMaterials; // Materials that are used by the text quads.

		// ------- Lit Text 3D -------
		LitTextGeometryData m_OpaqueLitTextData;
		LitTextGeometryData m_OpaqueLitNonShadowTextData;
		LitTextGeometryData m_MaskedLitTextData;
		LitTextGeometryData m_MaskedLitNonShadowTextData;
		LitTextGeometryData m_TranslucentLitTextData;
		LitTextGeometryData m_TranslucentNonShadowLitTextData;
		UnlitTextGeometryData m_UnlitTextData;
		UnlitTextGeometryData m_UnlitNonShadowTextData;

		std::vector<glm::mat4> m_TextTransforms;
		std::vector<uint64_t> m_TextUploadSpecificTransforms; // Instead of uploading all transforms, upload just required transforms. uint - index to "std::vector<glm::mat4> transforms"
		std::unordered_map<uint32_t, uint64_t> m_TextTransformIndices; // EntityID -> uint64_t (index to m_TextTransformIndices)

		bool bUploadTextQuads = true;
		bool bUploadTextTransforms = true;
		bool bUploadTextSpecificTransforms = false;
		// ------- !Lit Text 3D -------

		bool bMotionRequired = false;
	};
}
