#pragma once

#include "RendererTask.h"
#include "Eagle/Renderer/VidWrappers/PipelineGraphics.h"
#include "Eagle/Renderer/RendererUtils.h"
#include "Eagle/Core/Transform.h"

namespace Eagle
{
	class Buffer;
	class BillboardComponent;
	class Image;
	class Texture2D;
	struct Transform;

	class RenderBillboardsTask : public RendererTask
	{
	public:
		RenderBillboardsTask(SceneRenderer& renderer, const Ref<Image>& renderTo);

		void RecordCommandBuffer(const Ref<CommandBuffer>& cmd) override;
		void OnResize(glm::uvec2 size) override { m_Pipeline->Resize(size.x, size.y); }

		void InitWithOptions(const SceneRendererSettings& settings) override;

		void SetBillboards(const std::vector<const BillboardComponent*>& billboards);
		void AddAdditionalBillboard(const Transform& worldTransform, const Ref<Texture2D>& texture, int entityID = -1);

	private:
		struct BillboardVertex
		{
			glm::vec3 Position = glm::vec3{ 0.f };
			glm::vec3 PrevPosition = glm::vec3{ 0.f };
			glm::vec2 TexCoord = glm::vec2{ 0.f };
			uint32_t TextureIndex = 0;
			int EntityID = -1;
		};

		struct BillboardData
		{
			Transform WorldTransform;
			uint32_t TextureIndex = 0;
			int EntityID = -1;

			BillboardData() = default;

			BillboardData(const Transform& transform, uint32_t textureIndex, int entityID)
				: WorldTransform(transform), TextureIndex(textureIndex), EntityID(entityID) {}
		};

		void InitPipeline();
		void UpdateBuffers(const Ref<CommandBuffer>& cmd);
		void UpdateIndexBuffer(const Ref<CommandBuffer>& cmd);
		void RenderBillboards(const Ref<CommandBuffer>& cmd);
		void ProcessBillboardsData();

	private:
		std::vector<BillboardData> m_BillboardsData; // Data to build VertexBuffer from
		std::vector<BillboardVertex> m_Vertices;
		Ref<Buffer> m_VertexBuffer;
		Ref<Buffer> m_IndexBuffer;
		Ref<Image> m_ResultImage;
		Ref<PipelineGraphics> m_Pipeline;
		uint64_t m_TexturesUpdatedFrames[RendererConfig::FramesInFlight] = { 0 };
		bool bJitter = false;
		bool bMotionRequired = false;

		static constexpr size_t s_DefaultBillboardQuadCount = 10; // How much quads we can render without reallocating
		static constexpr size_t s_DefaultBillboardVerticesCount = s_DefaultBillboardQuadCount * 4;

		static constexpr size_t s_BaseBillboardVertexBufferSize = s_DefaultBillboardVerticesCount * sizeof(BillboardVertex);
		static constexpr size_t s_BaseBillboardIndexBufferSize  = s_DefaultBillboardQuadCount * (sizeof(Index) * 6);
	};
}
