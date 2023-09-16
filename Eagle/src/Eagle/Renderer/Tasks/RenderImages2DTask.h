#pragma once

#include "RendererTask.h"
#include "Eagle/Renderer/VidWrappers/PipelineGraphics.h"

#include <glm/gtc/matrix_transform.hpp>

namespace Eagle
{
	class Image2DComponent;
	class Texture2D;

	class RenderImages2DTask : public RendererTask
	{
	public:
		RenderImages2DTask(SceneRenderer& renderer);

		void RecordCommandBuffer(const Ref<CommandBuffer>& cmd) override;
		void Upload(const Ref<CommandBuffer>& cmd);
		void Render(const Ref<CommandBuffer>& cmd);
		void OnResize(glm::uvec2 size) override;

		void SetImages(const std::vector<const Image2DComponent*>& images, bool bDirty);

	private:
		struct QuadVertex
		{
			glm::vec3 Tint;
			uint32_t TextureIndex;
			glm::vec2 Position;
			glm::vec2 Scale;
			int EntityID;
			float Opacity;
		};

		struct Image2DComponentData
		{
			Ref<Texture2D> Texture;
			glm::vec3 Tint;
			glm::vec2 Pos;
			glm::vec2 Scale;
			float Rotation;
			int EntityID;
			float Opacity;
		};

		void InitPipeline();
		void ProcessImages(const std::vector<Image2DComponentData>& components);

	private:
		Ref<PipelineGraphics> m_Pipeline;
		Ref<Buffer> m_VertexBuffer;
		Ref<Buffer> m_IndexBuffer;

		std::vector<QuadVertex> m_Quads;
		std::unordered_map<Ref<Texture2D>, uint32_t> m_TexturesMap;
		std::vector<Ref<Texture2D>> m_Textures;

		glm::mat4 m_Proj = glm::mat4(1.f);
		glm::vec2 m_Size = glm::vec2(1.f);
		float m_InvAspectRatio = 1.f;

		bool bUpdate = true;

		static constexpr size_t s_DefaultQuadCount = 64; // How much quads we can render without reallocating
		static constexpr size_t s_DefaultVerticesCount = s_DefaultQuadCount * 4;

		static constexpr size_t s_BaseVertexBufferSize = s_DefaultVerticesCount * sizeof(QuadVertex);
		static constexpr size_t s_BaseIndexBufferSize = s_DefaultQuadCount * (sizeof(Index) * 6);
	};
}
