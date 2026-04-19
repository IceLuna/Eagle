#pragma once

#include "RendererTask.h"
#include "Eagle/Renderer/VidWrappers/PipelineGraphics.h"

namespace Eagle
{
	class Image;
	class Buffer;
	class DecalComponent;
	struct GBuffer;

	class RenderDecalsTask : public RendererTask
	{
	public:
		RenderDecalsTask(SceneRenderer& renderer);
		~RenderDecalsTask();

		void RecordCommandBuffer(const Ref<CommandBuffer>& cmd) override;
		void OnResize(const glm::uvec2 size) { m_Pipeline->Resize(size); m_WithNormalsPipeline->Resize(size); }

		void SetDecals(const std::vector<const DecalComponent*>& decals, bool bDirty);
		void SetTransforms(const std::vector<const DecalComponent*>& decals);

	private:
		void InitPipeline();
		void Upload(const Ref<CommandBuffer>& cmd);
		void Render(const Ref<CommandBuffer>& cmd);

		void AddMaterialCallbacks();
		void BindDescriptors(const Ref<PipelineGraphics>& pipeline, const GBuffer& gbuffer);

	private:
		struct DecalData
		{
			glm::vec2 AspectRatio = glm::vec2(1.f);
			uint32_t MaterialIndex = 0u;
			uint32_t TransformIndex = 0u;
			uint32_t EntityID = 0u;
		};

		inline static const std::vector<PipelineGraphicsState::VertexInputAttribute> PerInstanceAttribs = { { 0u }, { 1u }, { 2u }, { 3u } }; // Locations of Per-Instance data in shader

		Ref<Buffer> m_InstanceBuffer;
		Ref<Buffer> m_TransformsBuffer;
		std::vector<glm::mat4> m_Transforms;
		std::unordered_map<uint32_t, uint64_t> m_TransformsMapping; // key - entity ID; value - index into m_Transforms
		uint32_t m_NoNormalsDecalsCount = 0;
		uint32_t m_WithNormalsDecalsCount = 0;
		bool bUpload = true;
		bool bUploadTransforms = true;

		Ref<PipelineGraphics> m_Pipeline; // Doesn't affect normals in the gbuffer
		Ref<PipelineGraphics> m_WithNormalsPipeline;
		std::vector<DecalData> m_Decals;
		// Keep track of material changes
		std::map<uint32_t, Ref<Material>> m_Materials; // Key - Material index
		GUID m_CallbackID{};
		uint64_t m_TexturesUpdatedFrames[RendererConfig::FramesInFlight] = { 0 };
	};
}
