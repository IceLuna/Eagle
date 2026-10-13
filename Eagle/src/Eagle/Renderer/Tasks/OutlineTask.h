#pragma once

#include "RendererTask.h"
#include "Eagle/Renderer/VidWrappers/PipelineGraphics.h"
#include "Eagle/Renderer/VidWrappers/PipelineCompute.h"
#include "Eagle/Renderer/RendererUtils.h"

#include <ankerl/unordered_dense.h>

namespace Eagle
{
	class Buffer;
	class Image;
	class Framebuffer;

	// Draws outlines around objects by their ObjectID (entity ID): meshes, sprites, 3D texts (lit & unlit), billboards
	class OutlineTask : public RendererTask
	{
	public:
		OutlineTask(SceneRenderer& renderer);

		void RecordCommandBuffer(const Ref<CommandBuffer>& cmd) override;
		void OnResize(glm::uvec2 size) override;

		void SetOutlines(ankerl::unordered_dense::map<uint32_t, OutlineSettings>&& outlines);

		struct MeshDraw
		{
			uint32_t IndexCount = 0;
			uint32_t FirstIndex = 0;
			uint32_t VertexOffset = 0;  // Static meshes: offset in the vertex buffer. Skeletal meshes: offset in the skin cache
			uint32_t InstanceIndex = 0; // Index in the unculled instance buffer of GeometryManagerTask
			uint32_t StyleID = 0;
			bool bDoubleSided = false;
		};

		struct QuadsDraw
		{
			Ref<Buffer> VertexBuffer;
			Ref<Buffer> IndexBuffer;
			uint32_t FirstQuad = 0;
			uint32_t QuadsCount = 0;
			uint32_t StyleID = 0;
			bool bDoubleSided = false;
		};

	private:
		struct DrawLists
		{
			std::vector<MeshDraw> Meshes;
			std::vector<MeshDraw> MaskedMeshes;
			std::vector<MeshDraw> SkeletalMeshes;
			std::vector<MeshDraw> MaskedSkeletalMeshes;
			std::vector<QuadsDraw> Sprites;
			std::vector<QuadsDraw> MaskedSprites;
			std::vector<QuadsDraw> LitTexts;
			std::vector<QuadsDraw> MaskedLitTexts;
			std::vector<QuadsDraw> UnlitTexts;

			void Clear();
			bool IsEmpty() const;
		};

		struct OutlinePipeline
		{
			Ref<PipelineGraphics> Pipeline;
			uint64_t TexturesUpdatedFrames[RendererConfig::FramesInFlight] = { 0 };
		};

		void InitResources();
		void ReleaseResources();
		void InitPipelines();

		void BuildDrawLists();
		void BuildBillboardDrawList();
		uint32_t GetStyleID(uint32_t objectID) const; // Returns 0 if the object isn't outlined

		void UploadStyles(const Ref<CommandBuffer>& cmd);
		void RenderMask(const Ref<CommandBuffer>& cmd);
		void Composite(const Ref<CommandBuffer>& cmd);

		void BindTextures(OutlinePipeline& pipeline) const;
		void BindMaterials(OutlinePipeline& pipeline) const;
		void DrawMeshes(const Ref<CommandBuffer>& cmd, const Ref<PipelineGraphics>& pipeline, const std::vector<MeshDraw>& draws, bool bSkeletal) const;
		void DrawQuads(const Ref<CommandBuffer>& cmd, const Ref<PipelineGraphics>& pipeline, const std::vector<QuadsDraw>& draws, const glm::mat4& viewProj) const;

	private:
		// ObjectID -> Style index + 1
		ankerl::unordered_dense::map<uint32_t, uint32_t> m_ObjectStyles;
		std::vector<OutlineSettings> m_Styles;

		DrawLists m_DrawLists;
		std::vector<QuadsDraw> m_BillboardDraws;

		Ref<Image> m_Mask;
		Ref<Image> m_MaskDepth;
		Ref<Framebuffer> m_MaskFramebuffer;
		Ref<Buffer> m_StylesBuffer;

		OutlinePipeline m_MeshPipeline;
		OutlinePipeline m_MaskedMeshPipeline;
		OutlinePipeline m_SkeletalMeshPipeline;
		OutlinePipeline m_MaskedSkeletalMeshPipeline;
		OutlinePipeline m_SpritePipeline;
		OutlinePipeline m_MaskedSpritePipeline;
		OutlinePipeline m_LitTextPipeline;
		OutlinePipeline m_MaskedLitTextPipeline;
		OutlinePipeline m_UnlitTextPipeline;
		OutlinePipeline m_BillboardPipeline;
		Ref<PipelineCompute> m_CompositePipeline;

		uint64_t m_GeometryVersion = UINT64_MAX;
		uint32_t m_MaxRadius = 0; // In pixels
		bool bDrawListsDirty = true;
		bool bStylesDirty = true;
	};
}
