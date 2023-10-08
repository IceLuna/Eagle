#pragma once

#include "DescriptorManager.h"
#include "Eagle/Renderer/RendererUtils.h"
#include "Fence.h"
#include "Semaphore.h"
#include "Eagle/Debug/GPUTimings.h"

namespace Eagle
{
	enum class CommandQueueFamily
	{
		Graphics,
		Compute,
		Transfer
	};

	class CommandBuffer;
	class PipelineGraphics;
	class PipelineCompute;
	class Framebuffer;
	class Image;
	class Buffer;
	class StagingBuffer;

	class CommandManager
	{
	protected:
		CommandManager() = default;

	public:
		static Ref<CommandManager> Create(CommandQueueFamily queueFamily, bool bAllowReuse);

		virtual ~CommandManager() = default;

		CommandManager(const CommandManager&) = delete;
		CommandManager(CommandManager&& other) = delete;

		CommandManager& operator=(const CommandManager&) = delete;
		CommandManager& operator=(CommandManager&& other) noexcept = delete;

		[[nodiscard]] virtual Ref<CommandBuffer> AllocateCommandBuffer(bool bBegin = true) = 0;
		[[nodiscard]] virtual Ref<CommandBuffer> AllocateSecondaryCommandbuffer(bool bBegin = true) = 0;

		virtual void Submit(CommandBuffer* cmdBuffers, uint32_t cmdBuffersCount,
			const Ref<Fence>& signalFence,
			const Semaphore* waitSemaphores = nullptr, uint32_t waitSemaphoresCount = 0,
			const Semaphore* signalSemaphores = nullptr, uint32_t signalSemaphoresCount = 0) = 0;

		virtual void Submit(CommandBuffer* cmdBuffers, uint32_t cmdBuffersCount,
			const Semaphore* waitSemaphores = nullptr, uint32_t waitSemaphoresCount = 0,
			const Semaphore* signalSemaphores = nullptr, uint32_t signalSemaphoresCount = 0) = 0;
	};

	class CommandBuffer
	{
	protected:
		CommandBuffer() = default;

	public:
		CommandBuffer(const CommandBuffer&) = delete;
		CommandBuffer(CommandBuffer&& other) = delete;
		virtual ~CommandBuffer() = default;

		CommandBuffer& operator=(const CommandBuffer&) = delete;
		CommandBuffer& operator=(CommandBuffer&& other) = delete;

		virtual void* GetHandle() = 0;
		virtual bool IsSecondary() const = 0;

		virtual void Begin() = 0;
		virtual void End() = 0;

		virtual void Dispatch(Ref<PipelineCompute>& pipeline, uint32_t numGroupsX, uint32_t numGroupsY, uint32_t numGroupsZ, const void* pushConstants = nullptr) = 0;

		virtual void BeginGraphics(Ref<PipelineGraphics>& pipeline) = 0;
		virtual void BeginGraphics(Ref<PipelineGraphics>& pipeline, const Ref<Framebuffer>& framebuffer) = 0;
		virtual void EndGraphics() = 0;
		virtual void Draw(uint32_t vertexCount, uint32_t firstVertex) = 0;
		virtual void Draw(const Ref<Buffer>& vertexBuffer, uint32_t vertexCount, uint32_t firstVertex) = 0;
		virtual void DrawIndexedInstanced(const Ref<Buffer>& vertexBuffer, const Ref<Buffer>& indexBuffer, uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset,
			uint32_t instanceCount, uint32_t firstInstance, const Ref<Buffer>& perInstanceBuffer) = 0;
		virtual void DrawIndexed(const Ref<Buffer>& vertexBuffer, const Ref<Buffer>& indexBuffer, uint32_t indexCount, uint32_t firstIndex, uint32_t vertexOffset) = 0;
		virtual void ExecuteSecondary(const Ref<CommandBuffer>& secondaryCmd) = 0;

		virtual void SetGraphicsRootConstants(const void* vertexRootConstants, const void* fragmentRootConstants) = 0;

		void StorageImageBarrier(Ref<Image>& image) { TransitionLayout(image, ImageLayoutType::StorageImage, ImageLayoutType::StorageImage); }
		virtual void TransitionLayout(const Ref<Image>& image, ImageLayout oldLayout, ImageLayout newLayout) = 0;
		virtual void TransitionLayout(const Ref<Image>& image, const ImageView& imageView, ImageLayout oldLayout, ImageLayout newLayout) = 0;
		virtual void ClearColorImage(Ref<Image>& image, const glm::vec4& color) = 0;
		virtual void ClearDepthStencilImage(Ref<Image>& image, float depthValue, uint32_t stencilValue) = 0;

		void CopyImage(const Ref<Image>& src, const ImageView& srcView,
			Ref<Image>& dst, const ImageView& dstView,
			const glm::ivec3& srcOffset, const glm::ivec3& dstOffset,
			const glm::uvec3& size)
		{
			CopyImage(src, srcView, dst, dstView, dst->GetLayout(), dst->GetLayout(), srcOffset, dstOffset, size);
		}

		void CopyImage(const Ref<Image>& src, Ref<Image>& dst, ImageLayout dstOldLayout, ImageLayout dstNewLayout)
		{
			EG_CORE_ASSERT(src->GetSize() == dst->GetSize());
			CopyImage(src, ImageView{}, dst, ImageView{}, dstOldLayout, dstNewLayout, {}, {}, dst->GetSize());
		}

		virtual void CopyImage(const Ref<Image>& src, const ImageView& srcView,
			Ref<Image>& dst, const ImageView& dstView, ImageLayout dstOldLayout, ImageLayout dstNewLayout,
			const glm::ivec3& srcOffset, const glm::ivec3& dstOffset,
			const glm::uvec3& size) = 0;

		void StorageBufferBarrier(const Ref<Buffer>& buffer) { TransitionLayout(buffer, BufferLayoutType::StorageBuffer, BufferLayoutType::StorageBuffer); };
		virtual void TransitionLayout(const Ref<Buffer>& buffer, BufferLayout oldLayout, BufferLayout newLayout) = 0;
		virtual void CopyBuffer(const Ref<Buffer>& src, Ref<Buffer>& dst, size_t srcOffset, size_t dstOffset, size_t size) = 0;
		virtual void CopyBuffer(const Ref<StagingBuffer>& src, Ref<Buffer>& dst, size_t srcOffset, size_t dstOffset, size_t size) = 0;
		virtual void FillBuffer(Ref<Buffer>& dst, uint32_t data, size_t offset = 0, size_t numBytes = 0) = 0;

		void Barrier(const Ref<Buffer>& buffer) { TransitionLayout(buffer, buffer->GetLayout(), buffer->GetLayout()); }
		void Barrier(Ref<Image>& image) { TransitionLayout(image, image->GetLayout(), image->GetLayout()); }

		virtual void CopyBufferToImage(const Ref<Buffer>& src, Ref<Image>& dst, const std::vector<BufferImageCopy>& regions) = 0;
		virtual void CopyImageToBuffer(const Ref<Image>& src, Ref<Buffer>& dst, const std::vector<BufferImageCopy>& regions) = 0;

		// TODO: Implement writing to all mips
		virtual void Write(Ref<Image>& image, const void* data, size_t size, ImageLayout initialLayout, ImageLayout finalLayout) = 0;
		virtual void Write(Ref<Buffer>& buffer, const void* data, size_t size, size_t offset, BufferLayout initialLayout, BufferLayout finalLayout) = 0;

		virtual void GenerateMips(Ref<Image>& image, ImageLayout initialLayout, ImageLayout finalLayout) = 0;

#ifdef EG_GPU_TIMINGS
		virtual void StartTiming(Ref<RHIGPUTiming>& timing, uint32_t frameIndex) = 0;
		virtual void EndTiming(Ref<RHIGPUTiming>& timing, uint32_t frameIndex) = 0;
		virtual void BeginMarker(std::string_view name) = 0;
		virtual void EndMarker() = 0;
#endif
	};
}
