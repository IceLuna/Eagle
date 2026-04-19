#pragma once

#include "RendererTask.h"
#include "Eagle/Renderer/VidWrappers/Buffer.h"
#include "Eagle/Classes/StaticMesh.h"
#include "Eagle/Classes/SkeletalMesh.h"
#include "Eagle/Core/GUID.h"
#include "Eagle/Math/Transform.h"

struct CPUMaterial;

namespace Eagle
{
	struct MeshIndicesData
	{
		uint32_t IndicesCount = 0;
		uint32_t FirstIndex = 0;
	};

	template <typename MeshType>
	struct MeshKey
	{
		Ref<MeshType> Mesh;

		// Metadata to be reused when building a draw lists
		mutable uint32_t VerticesCount = 0u;
		mutable uint32_t VerticesOffset = 0u;
		mutable std::vector<MeshIndicesData> PerMaterialIndices;

		bool operator==(const MeshKey<MeshType>& other) const
		{
			return Mesh == other.Mesh;
		}
		bool operator!=(const MeshKey<MeshType>& other) const
		{
			return !((*this) == other);
		}
	};
}

namespace std
{
	template <typename MeshType>
	struct hash<Eagle::MeshKey<MeshType>>
	{
		std::size_t operator()(const Eagle::MeshKey<MeshType>& v) const
		{
			std::hash<Eagle::Ref<MeshType>> hasher;
			return hasher(v.Mesh);
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
	class Font;

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
			// .x = TransformIndex; .y = MaterialIndex; .z = ObjectID; .w = VertexOffset
			glm::uvec4 Data = glm::uvec4(0, 0, 0, ~0);
			struct
			{
				uint32_t PackedTransformIndex; // The highest is a flag whether a mesh receives decals
				uint32_t MaterialIndex;
				uint32_t ObjectID;

				// Because of skin cache + culling (indirect draw calls), we can't properly pick the correct skinned vertex because the order can change based on culling.
				// In order to solve this problem, each instance data has to store an offset to its vertices in the skin cache
				uint32_t VertexOffset;
			};
		};
	};

	template <typename VertexType, typename PerInstanceDataType>
	struct MeshGeometryData
	{
		Ref<Buffer> VertexBuffer;
		Ref<Buffer> InstanceBuffer;
		Ref<Buffer> IndexBuffer;
		Ref<Buffer> TransformsBuffer;
		Ref<Buffer> PrevTransformsBuffer;

		std::vector<VertexType> Vertices;
		std::vector<PerInstanceDataType> InstanceVertices;
		std::vector<Index> Indices;
	};

	struct SpriteGeometryData
	{
		Ref<Buffer> VertexBuffer;
		Ref<Buffer> IndexBuffer;
		std::vector<QuadVertex> QuadVertices;

		void Clear()
		{
			QuadVertices.clear();
		}
	};

	template <typename GeometryDataType>
	struct QuadsRenderData
	{
		struct BlendModeGeomType
		{
			GeometryDataType ShadowCastingQuads;
			GeometryDataType NonShadowQuads; // Quads that don't cast shadows

			bool IsEmpty() const
			{
				return ShadowCastingQuads.QuadVertices.empty() && NonShadowQuads.QuadVertices.empty();
			}
		};
		BlendModeGeomType Opaque;
		BlendModeGeomType Masked;
		BlendModeGeomType Translucent;

		void Clear()
		{
			Opaque.ShadowCastingQuads.Clear();
			Opaque.NonShadowQuads.Clear();
			Masked.ShadowCastingQuads.Clear();
			Masked.NonShadowQuads.Clear();
			Translucent.ShadowCastingQuads.Clear();
			Translucent.NonShadowQuads.Clear();
		}

		void Init(const BufferSpecifications& vertexSpecs, const BufferSpecifications& indexSpecs, bool bOpaqueOnly = false);
	};

	struct LitTextGeometryData
	{
		Ref<Buffer> VertexBuffer;
		Ref<Buffer> IndexBuffer;
		std::vector<LitTextQuadVertex> QuadVertices;

		void Clear()
		{
			QuadVertices.clear();
		}
	};

	struct UnlitTextGeometryData
	{
		Ref<Buffer> VertexBuffer;
		Ref<Buffer> IndexBuffer;
		std::vector<UnlitTextQuadVertex> QuadVertices;

		void Clear()
		{
			QuadVertices.clear();
		}
	};

	template <typename PerInstanceDataType>
	struct MeshInstance
	{
		std::vector<Ref<Material>> Materials;
		// Each material has its own submesh data it's assigned to.
		// So, it's a submesh for each material
		std::vector<PerInstanceDataType> SubMeshData;

		bool bCastsShadows = false;
	};

	using StaticMeshesMap = std::unordered_map<MeshKey<StaticMesh>, std::vector<MeshInstance<PerInstanceData>>>;
	using SkeletalMeshesMap = std::unordered_map<MeshKey<SkeletalMesh>, std::vector<MeshInstance<SkeletalPerInstanceData>>>;
	using StaticMeshGeometryData = MeshGeometryData<Vertex, PerInstanceData>;
	using SkeletalMeshGeometryData = MeshGeometryData<SkeletalVertex, SkeletalPerInstanceData>;

	struct MeshDrawData
	{
		AABB MeshAABB;
		uint32_t SkinnedVertexOffset = 0;
		uint32_t VertexOffset = 0;
		uint32_t VerticesCount = 0;
		uint32_t InstanceCount = 0;

		struct MaterialData
		{
			uint32_t FirstIndex = 0;
			uint32_t IndexCount = 0;
			uint32_t FirstInstance = 0;
			uint32_t InstanceCount = 0;
		};
		std::vector<MaterialData> PerMaterialData;
	};

	struct MeshDrawDataInfo
	{
		std::vector<MeshDrawData> DrawData;
		
		// Just a meta data indicating how many draw call it would require to draw this mesh and all of it's instances
		// Used to allocate enough memory for indirect draw calls
		uint32_t DrawCallsCount = 0;

		void Clear()
		{
			DrawData.clear();
			DrawCallsCount = 0u;
		}
	};

	struct MeshesDrawLists
	{
		struct
		{
			// All meshes
			MeshDrawDataInfo Opaque;
			MeshDrawDataInfo Translucent;
			MeshDrawDataInfo Masked;

			// Shadow casting only
			MeshDrawDataInfo ShadowCastingOpaque;
			MeshDrawDataInfo ShadowCastingTranslucent;
			MeshDrawDataInfo ShadowCastingMasked;

			void Clear()
			{
				Opaque.Clear();
				Translucent.Clear();
				Masked.Clear();

				ShadowCastingOpaque.Clear();
				ShadowCastingTranslucent.Clear();
				ShadowCastingMasked.Clear();
			}
		} SingleSided, DoubleSided;

		void Clear()
		{
			SingleSided.Clear();
			DoubleSided.Clear();
		}
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

	struct LitTextData
	{
		Ref<Material> Material;
		std::u32string Text;
		Ref<Font> Font;
		int EntityID;
		float LineHeightOffset;
		float KerningOffset;
		float MaxWidth;
		uint32_t TransformIndex;
		uint32_t MaterialIndex;
		bool bCastsShadows = false;
	};

	struct UnlitTextData
	{
		glm::vec3 Color;
		std::u32string Text;
		Ref<Font> Font;
		int EntityID;
		float LineHeightOffset;
		float KerningOffset;
		float MaxWidth;
		uint32_t TransformIndex;
		bool bCastsShadows = false;
		bool bDoubleSided = false;
	};

	class GeometryManagerTask : public RendererTask
	{
	public:
		GeometryManagerTask(SceneRenderer& renderer);

		void RecordCommandBuffer(const Ref<CommandBuffer>& cmd) override;
		void InitWithOptions(const SceneRendererSettings& settings) override;

		// ------- Meshes -------
		void SetMeshes(const std::vector<const StaticMeshComponent*>& meshes, bool bDirty);
		void SetTransforms(const std::vector<const StaticMeshComponent*>& meshes);

		// ------- Skeletal Meshes -------
		void SetSkeletalMeshes(const std::vector<SkeletalMeshComponent*>& meshes, bool bDirty);
		void SetTransforms(const std::vector<const SkeletalMeshComponent*>& meshes);

		// ------- Sprites -------
		void SetSprites(const std::vector<const SpriteComponent*>& sprites, bool bDirty);
		void SetTransforms(const std::vector<const SpriteComponent*>& sprites);

		// ------- Texts -------
		void SetTexts(const std::vector<const TextComponent*>& texts, bool bDirty);
		void SetTransforms(const std::vector<const TextComponent*>& texts);

		// Static Mesh getters
		const MeshesDrawLists& GetStaticMeshesDrawData() const { return m_StaticMeshesDrawData; }
		const StaticMeshGeometryData& GetStaticMeshesBuffers() const { return m_StaticMeshesBuffers; }
		const Ref<Buffer>& GetMeshesTransformBuffer() const { return m_StaticMeshesBuffers.TransformsBuffer; }
		const Ref<Buffer>& GetMeshesPrevTransformBuffer() const { return m_StaticMeshesBuffers.PrevTransformsBuffer; }

		// Skeletal Mesh getters
		const MeshesDrawLists& GetSkeletalMeshesDrawData() const { return m_SkeletalMeshesDrawData; }
		const SkeletalMeshGeometryData& GetSkeletalMeshesBuffers() const { return m_SkeletalMeshesBuffers; }
		const Ref<Buffer>& GetSkeletalMeshesTransformBuffer() const { return m_SkeletalMeshesBuffers.TransformsBuffer; }
		const SkeletalMeshesMap& GetSkeletalMeshes() const { return m_SkeletalMeshes; }

		const std::vector<std::vector<glm::mat4>>& GetAnimationTransforms() const { return m_AnimationTransforms; }
		const std::vector<Ref<Buffer>>& GetAnimationTransformsBuffers() const { return m_AnimationTransformsBuffers; }

		// Sprite getters
		const QuadsRenderData<SpriteGeometryData>& GetSingleSidedSpritesRenderData() const { return m_SingleSidedSprites; }
		const QuadsRenderData<SpriteGeometryData>& GetDoubleSidedSpritesRenderData() const { return m_DoubleSidedSprites; }
		const Ref<Buffer>& GetSpritesTransformBuffer() const { return m_SpritesTransformsBuffer; }
		const Ref<Buffer>& GetSpritesPrevTransformBuffer() const { return m_SpritesPrevTransformsBuffer; }

		// Text getters
		const QuadsRenderData<LitTextGeometryData>& GetSingleSidedTextsRenderData() const { return m_SingleSidedTexts; }
		const QuadsRenderData<LitTextGeometryData>& GetDoubleSidedTextsRenderData() const { return m_DoubleSidedTexts; }
		const QuadsRenderData<UnlitTextGeometryData>& GetSingleSidedUnlitTextsRenderData() const { return m_SingleSidedUnlitTexts; }
		const QuadsRenderData<UnlitTextGeometryData>& GetDoubleSidedUnlitTextsRenderData() const { return m_DoubleSidedUnlitTexts; }
		const Ref<Buffer>& GetTextsTransformBuffer() const { return m_TextTransformsBuffer; }
		const Ref<Buffer>& GetTextsPrevTransformBuffer() const { return m_TextPrevTransformsBuffer; }
		const std::vector<Ref<Texture2D>>& GetAtlases() const { return m_Atlases; }

	private:
		// ------- Meshes -------
		void SortMeshes(const Ref<CommandBuffer>& cmd);
		void UploadStaticMeshes(const Ref<CommandBuffer>& cmd);

		// ------- Skeletal Meshes -------
		void SortSkeletalMeshes(const Ref<CommandBuffer>& cmd);
		void UploadSkeletalMeshes(const Ref<CommandBuffer>& cmd);

		// ------- Sprites -------
		void SortSprites();
		void UploadSprites(const Ref<CommandBuffer>& cmd, const SpriteGeometryData& spritesData);
		void UploadSprites(const Ref<CommandBuffer>& cmd, const QuadsRenderData<SpriteGeometryData>& spritesData);
		static void AddQuad(std::vector<QuadVertex>& vertices, const SpriteData& sprite, const glm::mat4& transform, uint32_t transformIndex);
		static void AddQuad(std::vector<QuadVertex>& vertices, const glm::mat4& transform, const Ref<Material>& material, uint32_t transformIndex, const glm::vec2 UVs[4], int entityID = -1);

		// ------- Texts -------
		void SortTexts();
		void UploadTexts(const Ref<CommandBuffer>& cmd, const QuadsRenderData<LitTextGeometryData>& textsData);
		void UploadTexts(const Ref<CommandBuffer>& cmd, const LitTextGeometryData& textsData);
		void UploadTexts(const Ref<CommandBuffer>& cmd, const QuadsRenderData<UnlitTextGeometryData>& textsData);
		void UploadTexts(const Ref<CommandBuffer>& cmd, const UnlitTextGeometryData& textsData);

		void UploadAnimationTransforms(const Ref<CommandBuffer>& cmd);

	private:
		// ------- Static Meshes -------
		StaticMeshGeometryData m_StaticMeshesBuffers;
		StaticMeshesMap m_StaticMeshes;
		MeshesDrawLists m_StaticMeshesDrawData;
		std::vector<glm::mat4> m_MeshTransforms;
		std::vector<uint64_t> m_MeshUploadSpecificTransforms; // Instead of uploading all transforms, upload just required transforms. uint - index to "std::vector<glm::mat4> transforms"

		std::unordered_map<uint32_t, uint64_t> m_MeshTransformIndices; // EntityID -> uint64_t (index to m_MeshTransforms)

		bool bUploadMeshTransforms = true;
		bool bUploadMeshSpecificTransforms = false;
		bool bUploadMeshes = true;

		static constexpr size_t s_MeshesBaseVertexBufferSize = 1 * 1024 * 1024; // 1 MB
		static constexpr size_t s_MeshesBaseIndexBufferSize = 1 * 1024 * 1024; // 1 MB
		// ------- !Static Meshes -------

		// ------- Skeletal Meshes -------
		SkeletalMeshGeometryData m_SkeletalMeshesBuffers;
		SkeletalMeshesMap m_SkeletalMeshes;
		MeshesDrawLists m_SkeletalMeshesDrawData;
		std::vector<glm::mat4> m_SkeletalMeshTransforms;
		std::vector<uint64_t> m_SkeletalMeshUploadSpecificTransforms; // Instead of uploading all transforms, upload just required transforms. uint - index to "std::vector<glm::mat4> transforms"

		std::unordered_map<uint32_t, uint64_t> m_SkeletalMeshTransformIndices; // EntityID -> uint64_t (index to m_MeshTransforms)

		// Transforms of animations
		std::vector<std::vector<glm::mat4>> m_AnimationTransforms;
		std::vector<Ref<Buffer>> m_AnimationTransformsBuffers;

		bool bUploadSkeletalMeshTransforms = true;
		bool bUploadSkeletalMeshSpecificTransforms = false;
		bool bUploadSkeletalMeshes = true;

		// ------- !Skeletal Meshes -------

		// ------- Sprites -------
		QuadsRenderData<SpriteGeometryData> m_SingleSidedSprites;
		QuadsRenderData<SpriteGeometryData> m_DoubleSidedSprites;

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

		// ------- Lit Text 3D -------
		std::vector<LitTextData> m_LitTexts;
		std::vector<UnlitTextData> m_UnlitTexts;
		QuadsRenderData<LitTextGeometryData> m_SingleSidedTexts;
		QuadsRenderData<LitTextGeometryData> m_DoubleSidedTexts;
		QuadsRenderData<UnlitTextGeometryData> m_SingleSidedUnlitTexts;
		QuadsRenderData<UnlitTextGeometryData> m_DoubleSidedUnlitTexts;

		std::vector<glm::mat4> m_TextTransforms;
		std::vector<uint64_t> m_TextUploadSpecificTransforms; // Instead of uploading all transforms, upload just required transforms. uint - index to "std::vector<glm::mat4> transforms"
		std::unordered_map<uint32_t, uint64_t> m_TextTransformIndices; // EntityID -> uint64_t (index to m_TextTransformIndices)

		bool bUploadTextQuads = true;
		bool bUploadTextTransforms = true;
		bool bUploadTextSpecificTransforms = false;
		// ------- !Lit Text 3D -------

		bool bMotionRequired = false;
	};
	
	template<typename GeometryDataType>
	inline void QuadsRenderData<GeometryDataType>::Init(const BufferSpecifications& vertexSpecs, const BufferSpecifications& indexSpecs, bool bOpaqueOnly)
	{
		Opaque.ShadowCastingQuads.VertexBuffer = Buffer::Create(vertexSpecs, "VertexBuffer_2D_Opaque");
		Opaque.ShadowCastingQuads.IndexBuffer = Buffer::Create(indexSpecs, "IndexBuffer_2D_Opaque");

		Opaque.NonShadowQuads.VertexBuffer = Buffer::Create(vertexSpecs, "VertexBuffer_2D_Opaque_NotCastingShadow");
		Opaque.NonShadowQuads.IndexBuffer = Buffer::Create(indexSpecs, "IndexBuffer_2D_Opaque_NotCastingShadow");

		if (bOpaqueOnly)
			return;

		Masked.ShadowCastingQuads.VertexBuffer = Buffer::Create(vertexSpecs, "VertexBuffer_2D_Masked");
		Masked.ShadowCastingQuads.IndexBuffer = Buffer::Create(indexSpecs, "IndexBuffer_2D_Masked");

		Masked.NonShadowQuads.VertexBuffer = Buffer::Create(vertexSpecs, "VertexBuffer_2D_Masked_NotCastingShadow");
		Masked.NonShadowQuads.IndexBuffer = Buffer::Create(indexSpecs, "IndexBuffer_2D_Masked_NotCastingShadow");

		Translucent.ShadowCastingQuads.VertexBuffer = Buffer::Create(vertexSpecs, "VertexBuffer_2D_Translucent");
		Translucent.ShadowCastingQuads.IndexBuffer = Buffer::Create(indexSpecs, "IndexBuffer_2D_Translucent");

		Translucent.NonShadowQuads.VertexBuffer = Buffer::Create(vertexSpecs, "VertexBuffer_2D_Translucent_NotCastingShadow");
		Translucent.NonShadowQuads.IndexBuffer = Buffer::Create(indexSpecs, "IndexBuffer_2D_Translucent_NotCastingShadow");
	}
}
