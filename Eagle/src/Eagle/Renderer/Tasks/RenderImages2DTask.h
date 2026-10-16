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

		void SetImages(const std::vector<const Image2DComponent*>& images);

	private:
		struct QuadVertex
		{
			glm::vec3 Tint;
			uint32_t TextureIndex;
			glm::vec2 Position; // Relative to the pivot, before rotation. Reference pixels, Y points down
			int EntityID;
			float Opacity;
			glm::vec2 Anchor;
			glm::vec2 Offset; // Reference pixels
			float Rotation;
		};

		struct Image2DComponentData
		{
			Ref<Texture2D> Texture;
			glm::vec3 Tint;
			glm::vec2 Anchor;
			glm::vec2 Pivot;
			glm::vec2 Offset;
			glm::vec2 Size;
			float Rotation;
			int EntityID;
			float Opacity;
		};

		void InitPipeline();

		// Returns amount of unique textures
		uint32_t ProcessImages(const std::vector<Image2DComponentData>& components);

	private:
		Ref<PipelineGraphics> m_Pipeline;
		Ref<PipelineGraphics> m_PipelineNoEntityID;
		Ref<Buffer> m_VertexBuffer;
		Ref<Buffer> m_IndexBuffer;

		std::vector<QuadVertex> m_Quads;
		std::unordered_map<Ref<Texture2D>, uint32_t> m_TexturesMap;
		std::vector<Ref<Texture2D>> m_Textures;

		bool bUpdate = true;
	};
}
