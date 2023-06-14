#pragma once

#include "RendererTask.h"
#include "Eagle/Renderer/VidWrappers/PipelineGraphics.h"

namespace Eagle
{
	class TextComponent;
	class Buffer;

	class RenderTextLitTask : public RendererTask
	{
	public:
		RenderTextLitTask(SceneRenderer& renderer);

		void SetTexts(const std::vector<const TextComponent*>& texts, bool bDirty);

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

	private:
		void InitPipeline();
		void UploadIndexBuffer(const Ref<CommandBuffer>& cmd);
		void UpdateBuffers(const Ref<CommandBuffer>& cmd);
		void UploadTransforms(const Ref<CommandBuffer>& cmd);
		void RenderText(const Ref<CommandBuffer>& cmd);

		struct QuadVertex
		{
			glm::vec4 AlbedoRoughness = glm::vec4(1.f);
			glm::vec4 EmissiveMetallness = glm::vec4(1.f);
			glm::vec3 Position = glm::vec3{ 0.f };
			int EntityID = -1;
			glm::vec2 TexCoord = glm::vec2{ 0.f };
			uint32_t AtlasIndex = 0;
			float AO = 1.f;
			uint32_t TransformIndex = 0;
		};

	private:
		Ref<PipelineGraphics> m_Pipeline;
		Ref<Buffer> m_VertexBuffer;
		Ref<Buffer> m_IndexBuffer;
		Ref<Buffer> m_TransformsBuffer;
		Ref<Buffer> m_PrevTransformsBuffer;

		std::unordered_map<Ref<Texture2D>, uint32_t> m_FontAtlases;
		std::vector<Ref<Texture2D>> m_Atlases;
		std::vector<QuadVertex> m_QuadVertices;
		std::vector<glm::mat4> m_Transforms;
		bool m_UploadQuads = true;
		bool m_UploadTransforms = true;
		bool bMotionRequired = false;

		static constexpr size_t s_DefaultQuadCount = 512; // How much quads we can render without reallocating
		static constexpr size_t s_DefaultVerticesCount = s_DefaultQuadCount * 4;
		static constexpr size_t s_BaseVertexBufferSize = s_DefaultVerticesCount * sizeof(QuadVertex); //Allocating enough space to store 2048 vertices
		static constexpr size_t s_BaseIndexBufferSize = s_DefaultQuadCount * (sizeof(Index) * 6);
	};
}
