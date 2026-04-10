#pragma once

#include "Eagle/Renderer/VidWrappers/RenderCommandManager.h"
#include "Vulkan.h"

namespace Eagle
{
	class Pipeline;
	class StagingBuffer;
	class VulkanCommandBuffer;
	class VulkanPipelineGraphics;

	class VulkanCommandManager : public CommandManager
	{
	public:
		// @bAllowReuse. If set to true, allows already allocated command buffers to be rerecorded.
		VulkanCommandManager(CommandQueueFamily queueFamily, bool bAllowReuse);
		virtual ~VulkanCommandManager();

		VulkanCommandManager& operator=(const VulkanCommandManager&) = delete;
		VulkanCommandManager& operator=(VulkanCommandManager&& other) noexcept = delete;

		VkQueue GetVulkanQueue() const { return m_Queue; }

		[[nodiscard]] Ref<CommandBuffer> AllocateCommandBuffer(bool bBegin = true) override;
		[[nodiscard]] Ref<CommandBuffer> AllocateSecondaryCommandbuffer(bool bBegin = true) override;

		void Submit(CommandBuffer* cmdBuffers, uint32_t cmdBuffersCount,
			const Ref<Fence>& signalFence,
			const Semaphore* waitSemaphores = nullptr, uint32_t waitSemaphoresCount = 0,
			const Semaphore* signalSemaphores = nullptr, uint32_t signalSemaphoresCount = 0) override;

		void Submit(CommandBuffer* cmdBuffers, uint32_t cmdBuffersCount,
			const Semaphore* waitSemaphores = nullptr, uint32_t waitSemaphoresCount = 0,
			const Semaphore* signalSemaphores = nullptr, uint32_t signalSemaphoresCount = 0) override;

	private:
		VkCommandPool m_CommandPool = VK_NULL_HANDLE;
		VkQueue m_Queue = VK_NULL_HANDLE;
		VkQueueFlags m_QueueFlags = 0;
		uint32_t m_QueueFamilyIndex = uint32_t(-1);

		friend class VulkanCommandBuffer;
	};

	class VulkanCommandBuffer : public CommandBuffer
	{
	public:
		VulkanCommandBuffer(const VulkanCommandManager& manager, bool bSecondary);
		VulkanCommandBuffer() = default;
		virtual ~VulkanCommandBuffer();

		void Begin() override;
		void End() override;

		const void* GetHandle() const override { return m_CommandBuffer; }
		bool IsSecondary() const override { return !m_bIsPrimary; }

		void Dispatch(const Ref<PipelineCompute>& pipeline, uint32_t numGroupsX, uint32_t numGroupsY, uint32_t numGroupsZ, const void* pushConstants = nullptr) override;
		void DispatchIndirect(const Ref<PipelineCompute>& pipeline, const Ref<Buffer>& args, size_t offset, const void* pushConstants = nullptr) override;

		void BeginGraphics(const Ref<PipelineGraphics>& pipeline) override;
		void BeginGraphics(const Ref<PipelineGraphics>& pipeline, const Ref<Framebuffer>& framebuffer) override;
		void EndGraphics() override;
		void Draw(uint32_t vertexCount, uint32_t firstVertex) override;
		void DrawIndirect(const Ref<Buffer>& args, size_t offset, uint32_t drawCount, uint32_t stride) override;
		void Draw(const Ref<Buffer>& vertexBuffer, uint32_t vertexCount, uint32_t firstVertex) override;
		void DrawIndirect(const Ref<Buffer>& vertexBuffer, const Ref<Buffer>& args, size_t offset, uint32_t drawCount, uint32_t stride) override;
		void DrawInstanced(const Ref<Buffer>& vertexBuffer, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance, const Ref<Buffer>& perInstanceBuffer) override;
		void DrawInstanced(const Ref<Buffer>& perInstanceBuffer, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) override;
		void DrawInstancedIndirect(const Ref<Buffer>& vertexBuffer, const Ref<Buffer>& args, size_t offset, uint32_t drawCount, uint32_t stride, const Ref<Buffer>& perInstanceBuffer) override;
		void DrawIndexedInstanced(const Ref<Buffer>& vertexBuffer, const Ref<Buffer>& indexBuffer, uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset,
			uint32_t instanceCount, uint32_t firstInstance, const Ref<Buffer>& perInstanceBuffer) override;
		void DrawIndexed(const Ref<Buffer>& vertexBuffer, const Ref<Buffer>& indexBuffer, uint32_t indexCount, uint32_t firstIndex, uint32_t vertexOffset) override;
		void ExecuteSecondary(const Ref<CommandBuffer>& secondaryCmd) override;

		void SetGraphicsRootConstants(const void* vertexRootConstants, const void* fragmentRootConstants) override;
		void SetGraphicsCullMode(CullMode cullMode) override;

		void TransitionLayout(const Ref<Image>& image, ImageLayout oldLayout, ImageLayout newLayout) override;
		void TransitionLayout(const Ref<Image>& image, const ImageView& imageView, ImageLayout oldLayout, ImageLayout newLayout) override;
		void ClearColorImage(const Ref<Image>& image, const glm::vec4& color, ImageLayout layout, ImageLayout newLayout) override;
		void ClearDepthStencilImage(const Ref<Image>& image, float depthValue, uint32_t stencilValue, ImageLayout layout, ImageLayout newLayout) override;
		void CopyImage(const Ref<Image>& src, const ImageView& srcView,
			const Ref<Image>& dst, const ImageView& dstView, ImageLayout dstOldLayout, ImageLayout dstNewLayout,
			const glm::ivec3& srcOffset, const glm::ivec3& dstOffset,
			const glm::uvec3& size) override;

		void TransitionLayout(const Ref<Buffer>& buffer, BufferLayout oldLayout, BufferLayout newLayout) override;
		void CopyBuffer(const Ref<Buffer>& src, const Ref<Buffer>& dst, size_t srcOffset, size_t dstOffset, size_t size) override;
		void CopyBufferTransitionless(const Ref<Buffer>& src, const Ref<Buffer>& dst, size_t srcOffset, size_t dstOffset, size_t size) override; // Doesn't transition layouts
		void CopyBuffer(const Ref<StagingBuffer>& src, const Ref<Buffer>& dst, size_t srcOffset, size_t dstOffset, size_t size) override;
		void FillBuffer(const Ref<Buffer>& dst, uint32_t data, size_t offset = 0, size_t numBytes = 0) override;

		void CopyBufferToImage(const Ref<Buffer>& src, const Ref<Image>& dst, const std::vector<BufferImageCopy>& regions) override;
		void CopyImageToBuffer(const Ref<Image>& src, const Ref<Buffer>& dst, const std::vector<BufferImageCopy>& regions) override;

		void Write(const Ref<Image>& image, const void* data, size_t size, ImageLayout initialLayout, ImageLayout finalLayout) override;
		void Write(const Ref<Buffer>& buffer, const void* data, size_t size, size_t offset, BufferLayout initialLayout, BufferLayout finalLayout) override;
		void WriteTransitionless(Ref<Buffer>& buffer, const void* data, size_t size, size_t offset) override;

		void GenerateMips(const Ref<Image>& image, ImageLayout initialLayout, ImageLayout finalLayout) override;
		void GenerateMips(const Ref<Image>& image, const std::vector<ScopedDataBuffer>& dataPerMip, ImageLayout initialLayout, ImageLayout finalLayout) override;

#ifdef EG_GPU_TIMINGS
		virtual void StartTiming(const Ref<RHIGPUTiming>& timing, uint32_t frameIndex) override;
		virtual void EndTiming(const Ref<RHIGPUTiming>& timing, uint32_t frameIndex) override;
		virtual void BeginMarker(std::string_view name) override;
		virtual void EndMarker() override;
#endif

		VkCommandBuffer GetVulkanCommandBuffer() const { return m_CommandBuffer; }

	private:
		void CommitDescriptors(Ref<Pipeline>& pipeline, VkPipelineBindPoint bindPoint);

	private:
		std::unordered_set<StagingBuffer*> m_UsedStagingBuffers;
		VkDevice m_Device = VK_NULL_HANDLE;
		VkCommandPool m_CommandPool = VK_NULL_HANDLE;
		VkCommandBuffer m_CommandBuffer = VK_NULL_HANDLE;
		VkQueueFlags m_QueueFlags;
		Ref<VulkanPipelineGraphics> m_CurrentGraphicsPipeline;
		Ref<Framebuffer> m_CurrentFramebuffer;
		bool m_bIsPrimary = true;
		bool m_bIsRecording = false;

		friend class VulkanCommandManager;
		friend class VulkanImage;
	};
}
