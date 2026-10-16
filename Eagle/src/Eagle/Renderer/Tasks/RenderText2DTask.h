#pragma once

#include "RendererTask.h"
#include "Eagle/Renderer/VidWrappers/PipelineGraphics.h"
#include "Eagle/Classes/Font.h"

namespace Eagle
{
	class Text2DComponent;
	class Texture2D;
	class Font;

	class RenderText2DTask : public RendererTask
	{
	public:
		RenderText2DTask(SceneRenderer& renderer);

		void RecordCommandBuffer(const Ref<CommandBuffer>& cmd) override;
		void Upload(const Ref<CommandBuffer>& cmd);
		void Render(const Ref<CommandBuffer>& cmd);
		void OnResize(glm::uvec2 size) override;

		void SetTexts(const std::vector<const Text2DComponent*>& texts);

	private:
		struct QuadVertex
		{
			glm::vec3 Color;
			uint32_t AtlasIndex;
			glm::vec2 Position; // Relative to the pivot, before rotation. Reference pixels, Y points down
			glm::vec2 TexCoord;
			int EntityID;
			float Opacity;
			glm::vec2 Anchor;
			glm::vec2 Offset; // Reference pixels
			float Rotation;
		};

		struct Text2DComponentData
		{
			Ref<Font> Font;
			Ref<const TextLayout> Layout;
			glm::vec3 Color;
			float FontSize;
			glm::vec2 Anchor;
			glm::vec2 Pivot;
			glm::vec2 Offset;
			float Rotation;
			int EntityID;
			float Opacity;
		};

		void InitPipeline();

		// Returns the number of unique atlases
		uint32_t ProcessTexts(const std::vector<Text2DComponentData>& textComponents);

	private:
		Ref<PipelineGraphics> m_Pipeline;
		Ref<PipelineGraphics> m_PipelineNoEntityID;
		Ref<Buffer> m_VertexBuffer;
		Ref<Buffer> m_IndexBuffer;

		std::vector<QuadVertex> m_Quads;
		std::unordered_map<Ref<Texture2D>, uint32_t> m_FontAtlases;
		std::vector<Ref<Texture2D>> m_Atlases;

		bool bUpload = true;
	};
}
