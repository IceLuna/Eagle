#pragma once

#include "VulkanPipeline.h"
#include "Eagle/Renderer/VidWrappers/PipelineGraphics.h"

namespace Eagle
{
	class VulkanPipelineGraphics : virtual public VulkanPipeline, virtual public PipelineGraphics
	{
	public:
		VulkanPipelineGraphics(const PipelineGraphicsState& state, const Ref<PipelineGraphics>& parentPipeline = nullptr);
		virtual ~VulkanPipelineGraphics();

		void* GetRenderPassHandle() const override { return m_RenderPass; };
		void* GetPipelineHandle() const override { return m_GraphicsPipeline; };
		void* GetPipelineLayoutHandle() const override { return m_PipelineLayout; };

		virtual void Recreate() override;

		// Resizes framebuffer
		void Resize(uint32_t width, uint32_t height) override;

	private:
		void Create(VkPipeline parentPipeline);

	private:
		VkPipeline m_GraphicsPipeline = VK_NULL_HANDLE;
		VkRenderPass m_RenderPass = VK_NULL_HANDLE;
		VkFramebuffer m_Framebuffer = VK_NULL_HANDLE;
		VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;

		friend class VulkanCommandBuffer;
	};
}
