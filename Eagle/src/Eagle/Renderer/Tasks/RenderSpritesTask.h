#pragma once

#include "RendererTask.h"
#include "Eagle/Renderer/VidWrappers/PipelineGraphics.h"

struct CPUMaterial;

namespace Eagle
{
	class SpriteComponent;
	class Material;
	class Texture2D;
	class SubTexture2D;
	class Buffer;

	struct Transform;

	class RenderSpritesTask : public RendererTask
	{
	public:
		RenderSpritesTask(SceneRenderer& renderer);

		void SetSprites(const std::vector<const SpriteComponent*>& sprites, bool bDirty);
		void SetTransforms(const std::set<const SpriteComponent*>& sprites);

		void RecordCommandBuffer(const Ref<CommandBuffer>& cmd) override;
		void OnResize(glm::uvec2 size) override { m_Pipeline->Resize(size.x, size.y); }

		void InitWithOptions(const SceneRendererSettings& settings) override
		{
			if (bMotionRequired == settings.OptionalGBuffers.bMotion)
				return;

			bMotionRequired = settings.OptionalGBuffers.bMotion;
			if (!bMotionRequired)
				m_PrevTransformsBuffer.reset();

			InitPipeline();
		}

		const Ref<Buffer>& GetVertexBuffer() const { return m_VertexBuffer; }
		const Ref<Buffer>& GetIndexBuffer() const { return m_IndexBuffer; }

		struct QuadVertex;
		const std::vector<QuadVertex>& GetVertices() const { return m_QuadVertices; }
		const Ref<Buffer>& GetSpritesTransformsBuffer() const { return m_TransformsBuffer; }

	private:
		static void AddQuad(std::vector<QuadVertex>& vertices, std::vector<Ref<Material>>& materials, std::vector<glm::mat4>& transforms, const Transform& transform, const Ref<Material>& material, int entityID = -1);
		static void AddQuad(std::vector<QuadVertex>& vertices, std::vector<Ref<Material>>& materials, std::vector<glm::mat4>& transforms, const Transform& transform, const Ref<SubTexture2D>& subtexture, const Ref<Material>& material, int entityID = -1);
		static void AddQuad(std::vector<QuadVertex>& vertices, std::vector<Ref<Material>>& materials, std::vector<glm::mat4>& transforms, const SpriteComponent& sprite);

		//General function that are being called
		static void AddQuad(std::vector<QuadVertex>& vertices, std::vector<Ref<Material>>& materials, std::vector<glm::mat4>& transforms, const glm::mat4& transform, const Ref<Material>& material, int entityID = -1);
		static void AddQuad(std::vector<QuadVertex>& vertices, std::vector<Ref<Material>>& materials, std::vector<glm::mat4>& transforms, const glm::mat4& transform, const Ref<SubTexture2D>& subtexture, const Ref<Material>& material, int entityID = -1);

		void RenderSprites(const Ref<CommandBuffer>& cmd);
		void UploadQuads(const Ref<CommandBuffer>& cmd);
		void UploadIndexBuffer(const Ref<CommandBuffer>& cmd);
		void UpdateMaterials();
		void UploadMaterials(const Ref<CommandBuffer>& cmd);
		void UploadTransforms(const Ref<CommandBuffer>& cmd);
		void InitPipeline();

	public:
		struct QuadVertex
		{
			glm::vec2 TexCoords;
			int EntityID = -1;
			uint32_t MaterialIndex = 0;
			uint32_t TransformIndex = 0;
		};

	private:
		std::vector<QuadVertex> m_QuadVertices;
		std::vector<Ref<Material>> m_Materials;
		std::vector<glm::mat4> m_Transforms;
		std::vector<CPUMaterial> m_GPUMaterials;
		std::unordered_map<uint32_t, uint64_t> m_TransformIndices; // EntityID -> uint64_t (index to m_Transforms)

		Ref<PipelineGraphics> m_Pipeline;
		Ref<Buffer> m_VertexBuffer;
		Ref<Buffer> m_IndexBuffer;
		Ref<Buffer> m_MaterialBuffer;
		Ref<Buffer> m_TransformsBuffer;
		Ref<Buffer> m_PrevTransformsBuffer;
		uint64_t m_TexturesUpdatedFrame = 0;
		bool bMotionRequired = false;
		bool bUploadSprites = true;
		bool bUploadSpritesTransforms = true;

		static constexpr size_t s_DefaultQuadCount = 512; // How much quads we can render without reallocating
		static constexpr size_t s_DefaultVerticesCount = s_DefaultQuadCount * 4;
		static constexpr size_t s_BaseVertexBufferSize = s_DefaultVerticesCount * sizeof(QuadVertex); //Allocating enough space to store 2048 vertices
		static constexpr size_t s_BaseIndexBufferSize = s_DefaultQuadCount * (sizeof(Index) * 6);
	};
}
