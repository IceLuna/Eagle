#pragma once

#include "RendererTask.h"
#include "Eagle/Renderer/VidWrappers/PipelineGraphics.h"

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
	private:
		struct SubTextureProps
		{
			glm::vec4 TintColor = glm::vec4{ 1.0 };
			float Opacity = 1.f;
			float TilingFactor = 1.f;
		};

	public:
		RenderSpritesTask(SceneRenderer& renderer);

		void SetSprites(const std::vector<const SpriteComponent*>& sprites);

		void RecordCommandBuffer(const Ref<CommandBuffer>& cmd) override;
		void OnResize(const glm::uvec2 size) override { m_Pipeline->Resize(size.x, size.y); }

		void AddQuad(const Transform& transform, const Ref<Material>& material, int entityID = -1);
		void AddQuad(const Transform& transform, const Ref<SubTexture2D>& subtexture, const SubTextureProps& textureProps, int entityID = -1);
		void AddQuad(const SpriteComponent& sprite);

		const Ref<Buffer>& GetVertexBuffer() const { return m_VertexBuffer; }
		const Ref<Buffer>& GetIndexBuffer() const { return m_IndexBuffer; }

		struct QuadVertex;
		const std::vector<QuadVertex>& GetVertices() const { return m_QuadVertices; }

	private:
		//General function that are being called
		void AddQuad(const glm::mat4& transform, const Ref<Material>& material, int entityID = -1);
		void AddQuad(const glm::mat4& transform, const Ref<SubTexture2D>& subtexture, const SubTextureProps& textureProps, int entityID = -1);

		void Render(const Ref<CommandBuffer>& cmd);
		void UploadQuads(const Ref<CommandBuffer>& cmd);
		void UploadIndexBuffer(const Ref<CommandBuffer>& cmd);
		void InitPipeline();

		struct RendererMaterial
		{
		public:
			RendererMaterial() = default;
			RendererMaterial(const RendererMaterial&) = default;
			RendererMaterial(const Ref<Material>& material);

			RendererMaterial& operator=(const RendererMaterial&) = default;
			RendererMaterial& operator=(const Ref<Material>& material);
			RendererMaterial& operator=(const Ref<Texture2D>& texture);

		public:
			glm::vec4 TintColor = glm::vec4{ 1.f };
			float TilingFactor = 1.f;

			// [0-9]   bits AlbedoTextureIndex
			// [10-19] bits MetallnessTextureIndex
			// [20-29] bits NormalTextureIndex
			uint32_t PackedTextureIndices = 0;

			// [0-9]   bits RoughnessTextureIndex
			// [10-19] bits AOTextureIndex
			// [20-31] bits unused
			uint32_t PackedTextureIndices2 = 0;
		};

	public:
		struct QuadVertex
		{
			glm::vec3 Position = glm::vec3{ 0.f };
			glm::vec3 Normal = glm::vec3{ 0.f };
			glm::vec3 WorldTangent = glm::vec3{ 0.f };
			glm::vec3 WorldBitangent = glm::vec3{ 0.f };
			glm::vec3 WorldNormal = glm::vec3{ 0.f };
			glm::vec2 TexCoord = glm::vec2{ 0.f };
			int EntityID = -1;
			RendererMaterial Material;
		};

	private:
		std::vector<QuadVertex> m_QuadVertices;
		Ref<PipelineGraphics> m_Pipeline;
		Ref<Buffer> m_VertexBuffer;
		Ref<Buffer> m_IndexBuffer;
		uint64_t m_TexturesUpdatedFrame = 0;

		static constexpr size_t s_DefaultQuadCount = 512; // How much quads we can render without reallocating
		static constexpr size_t s_DefaultVerticesCount = s_DefaultQuadCount * 4;
		static constexpr size_t s_BaseVertexBufferSize = s_DefaultVerticesCount * sizeof(QuadVertex); //Allocating enough space to store 2048 vertices
		static constexpr size_t s_BaseIndexBufferSize = s_DefaultQuadCount * (sizeof(Index) * 6);
	};
}
