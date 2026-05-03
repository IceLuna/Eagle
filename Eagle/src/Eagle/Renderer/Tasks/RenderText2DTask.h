#pragma once

#include "RendererTask.h"
#include "Eagle/Renderer/VidWrappers/PipelineGraphics.h"

#include <glm/gtc/matrix_transform.hpp>

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
			glm::vec2 Position;
			glm::vec2 TexCoord;
			int EntityID;
			float Opacity;
		};

		struct Text2DComponentData
		{
			Ref<Font> Font;
			std::u32string Text;
			glm::vec3 Color;
			float LineSpacing;
			glm::vec2 Pos;
			glm::vec2 Scale;
			float Rotation;
			float KerningOffset;
			float MaxWidth;
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
