#pragma once

#include "RendererTask.h"
#include "Eagle/Renderer/VidWrappers/PipelineGraphics.h"

namespace Eagle
{
	class Image;
	class Buffer;
	class DecalComponent;

	class RenderDecalsTask : public RendererTask
	{
	public:
		RenderDecalsTask(SceneRenderer& renderer);
		~RenderDecalsTask();

		void RecordCommandBuffer(const Ref<CommandBuffer>& cmd) override;
		void OnResize(const glm::uvec2 size) { m_Pipeline->Resize(size); }

		void SetDecals(const std::vector<const DecalComponent*>& decals, bool bDirty);
		void SetTransforms(const std::unordered_set<const DecalComponent*>& decals);

	private:
		void InitPipeline();
		void Upload(const Ref<CommandBuffer>& cmd);
		void Render(const Ref<CommandBuffer>& cmd);

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
		bool bUpload = true;
		bool bUploadTransforms = true;
		bool bUpdateMaterials = false;

		Ref<PipelineGraphics> m_Pipeline;
		std::vector<DecalData> m_Decals;
		// Keep track of material changes if a decal needs to update aspect ratio
		std::map<uint32_t, Ref<Material>> m_MaterialsToAdjustTo; // Key - Material index
		GUID m_CallbackID{};
		uint64_t m_TexturesUpdatedFrames[RendererConfig::FramesInFlight] = { 0 };
	};
}
