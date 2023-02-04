#include "egpch.h"
#include "VulkanCommandManager.h"
#include "VulkanContext.h"
#include "VulkanDevice.h"
#include "VulkanPipelineGraphics.h"
#include "VulkanPipelineCompute.h"
#include "VulkanFramebuffer.h"
#include "VulkanImage.h"
#include "VulkanBuffer.h"
#include "VulkanShader.h"
#include "VulkanUtils.h"
#include "VulkanFence.h"
#include "VulkanSemaphore.h"

#include "Eagle/Renderer/StagingManager.h"
#include "Eagle/Renderer/DescriptorManager.h"
#include "Eagle/Renderer/RendererUtils.h"

namespace Eagle
{
	static uint32_t SelectQueueFamilyIndex(CommandQueueFamily queueFamily, const QueueFamilyIndices& indices)
	{
		switch (queueFamily)
		{
		case CommandQueueFamily::Graphics: return indices.GraphicsFamily;
		case CommandQueueFamily::Compute:  return indices.ComputeFamily;
		case CommandQueueFamily::Transfer: return indices.TransferFamily;
		}
		assert(!"Unknown queue family");
		return uint32_t(-1);
	}

	static VkQueue SelectQueue(CommandQueueFamily queueFamily, const VulkanDevice* device)
	{
		switch (queueFamily)
		{
		case CommandQueueFamily::Graphics: return device->GetGraphicsQueue();
		case CommandQueueFamily::Compute:  return device->GetComputeQueue();
		case CommandQueueFamily::Transfer: return device->GetTransferQueue();
		}
		assert(!"Unknown queue family");
		return VK_NULL_HANDLE;
	}

	static VkQueueFlags GetQueueFlags(CommandQueueFamily queueFamily)
	{
		switch (queueFamily)
		{
		case CommandQueueFamily::Graphics: return VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT;
		case CommandQueueFamily::Compute:  return VK_QUEUE_COMPUTE_BIT;
		case CommandQueueFamily::Transfer: return VK_QUEUE_TRANSFER_BIT;
		}
		assert(!"Unknown queue family");
		return VK_QUEUE_GRAPHICS_BIT;
	}

	//------------------
	// COMMAND MANAGER
	//------------------
	VulkanCommandManager::VulkanCommandManager(CommandQueueFamily queueFamily, bool bAllowReuse)
	{
		const VulkanDevice* device = VulkanContext::GetDevice();
		VkDevice vulkanDevice = device->GetVulkanDevice();
		m_QueueFamilyIndex = SelectQueueFamilyIndex(queueFamily, device->GetPhysicalDevice()->GetFamilyIndices());
		m_Queue = SelectQueue(queueFamily, device);
		m_QueueFlags = GetQueueFlags(queueFamily);

		VkCommandPoolCreateInfo ci{};
		ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		ci.queueFamilyIndex = m_QueueFamilyIndex;
		ci.flags = bAllowReuse ? VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT : 0;

		VK_CHECK(vkCreateCommandPool(vulkanDevice, &ci, nullptr, &m_CommandPool));
	}

	VulkanCommandManager::~VulkanCommandManager()
	{
		if (m_CommandPool)
		{
			Renderer::SubmitResourceFree([pool = m_CommandPool]()
			{
				vkDestroyCommandPool(VulkanContext::GetDevice()->GetVulkanDevice(), pool, nullptr);
			});
			m_CommandPool = VK_NULL_HANDLE;
		}
	}

	Ref<CommandBuffer> VulkanCommandManager::AllocateCommandBuffer(bool bBegin)
	{
		Ref<VulkanCommandBuffer> cmd = MakeRef<VulkanCommandBuffer>(*this, false);
		if (bBegin)
			cmd->Begin();
		return cmd;
	}

	Ref<CommandBuffer> VulkanCommandManager::AllocateSecondaryCommandbuffer(bool bBegin)
	{
		Ref<VulkanCommandBuffer> cmd = MakeRef<VulkanCommandBuffer>(*this, true);
		if (bBegin)
			cmd->Begin();
		return cmd;
	}

	void VulkanCommandManager::Submit(CommandBuffer* cmdBuffers, uint32_t cmdBuffersCount,
		const Ref<Fence>& signalFence,
		const Semaphore* waitSemaphores, uint32_t waitSemaphoresCount,
		const Semaphore* signalSemaphores, uint32_t signalSemaphoresCount)
	{
		std::vector<VkCommandBuffer> vkCmdBuffers(cmdBuffersCount);
		for (uint32_t i = 0; i < cmdBuffersCount; ++i)
		{
			VulkanCommandBuffer* cmdBuffer = (VulkanCommandBuffer*)cmdBuffers + i;
			vkCmdBuffers[i] = cmdBuffer->m_CommandBuffer;

			for (auto& staging : cmdBuffer->m_UsedStagingBuffers)
			{
				if (staging->GetState() == StagingBufferState::Pending)
				{
					staging->SetFence(signalFence);
					staging->SetState(StagingBufferState::InFlight);
				}
			}
			cmdBuffer->m_UsedStagingBuffers.clear();
		}

		std::vector<VkSemaphore> vkSignalSemaphores(signalSemaphoresCount);
		for (uint32_t i = 0; i < signalSemaphoresCount; ++i)
			vkSignalSemaphores[i] = (VkSemaphore)((VulkanSemaphore*)signalSemaphores + i)->GetHandle();

		std::vector<VkSemaphore> vkWaitSemaphores(waitSemaphoresCount);
		std::vector<VkPipelineStageFlags> vkDstStageMask(waitSemaphoresCount);
		for (uint32_t i = 0; i < waitSemaphoresCount; ++i)
		{
			vkWaitSemaphores[i] = (VkSemaphore)((VulkanSemaphore*)waitSemaphores + i)->GetHandle();
			vkDstStageMask[i] = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
		}

		VkSubmitInfo info{};
		info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		info.commandBufferCount = cmdBuffersCount;
		info.pCommandBuffers = vkCmdBuffers.data();
		info.waitSemaphoreCount = waitSemaphoresCount;
		info.pWaitSemaphores = vkWaitSemaphores.data();
		info.signalSemaphoreCount = signalSemaphoresCount;
		info.pSignalSemaphores = vkSignalSemaphores.data();
		info.pWaitDstStageMask = vkDstStageMask.data();

		VK_CHECK(vkQueueSubmit(m_Queue, 1, &info, (VkFence)signalFence->GetHandle()));
	}

	void VulkanCommandManager::Submit(CommandBuffer* cmdBuffers, uint32_t cmdBuffersCount,
		const Semaphore* waitSemaphores, uint32_t waitSemaphoresCount,
		const Semaphore* signalSemaphores, uint32_t signalSemaphoresCount)
	{
		Submit(cmdBuffers, cmdBuffersCount, Fence::Create(),
			waitSemaphores, waitSemaphoresCount, signalSemaphores, signalSemaphoresCount);
	}

	//------------------
	// COMMAND BUFFER
	//------------------

	VulkanCommandBuffer::VulkanCommandBuffer(const VulkanCommandManager& manager, bool bSecondary)
	{
		m_Device = VulkanContext::GetDevice()->GetVulkanDevice();
		m_CommandPool = manager.m_CommandPool;
		m_QueueFlags = manager.m_QueueFlags;
		m_bIsPrimary = !bSecondary;

		VkCommandBufferAllocateInfo info{};
		info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		info.commandPool = m_CommandPool;
		info.commandBufferCount = 1;
		info.level = bSecondary ? VK_COMMAND_BUFFER_LEVEL_SECONDARY : VK_COMMAND_BUFFER_LEVEL_PRIMARY;

		VK_CHECK(vkAllocateCommandBuffers(m_Device, &info, &m_CommandBuffer));
	}

	VulkanCommandBuffer::~VulkanCommandBuffer()
	{
		if (m_CommandBuffer)
		{
			Renderer::SubmitResourceFree([device = m_Device, pool = m_CommandPool, buffer = m_CommandBuffer]()
			{
				vkFreeCommandBuffers(device, pool, 1, &buffer);
			});
			m_CommandBuffer = VK_NULL_HANDLE;
		}
	}

	void VulkanCommandBuffer::Begin()
	{
		EG_CORE_ASSERT(m_bIsRecording == false);
		VkCommandBufferBeginInfo info{};
		info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		info.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

		VK_CHECK(vkBeginCommandBuffer(m_CommandBuffer, &info));
		m_bIsRecording = true;
	}

	void VulkanCommandBuffer::End()
	{
		EG_CORE_ASSERT(m_bIsRecording == true);
		vkEndCommandBuffer(m_CommandBuffer);
		m_bIsRecording = false;
	}

	void VulkanCommandBuffer::Dispatch(Ref<PipelineCompute>& pipeline, uint32_t numGroupsX, uint32_t numGroupsY, uint32_t numGroupsZ, const void* pushConstants)
	{
		Ref<VulkanPipelineCompute> vulkanPipeline = Cast<VulkanPipelineCompute>(pipeline);
		vkCmdBindPipeline(m_CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, (VkPipeline)vulkanPipeline->GetPipelineHandle());

		Ref<Pipeline> purePipeline = Cast<Pipeline>(pipeline);
		CommitDescriptors(purePipeline, VK_PIPELINE_BIND_POINT_COMPUTE);

		if (pushConstants)
		{
			auto& ranges = vulkanPipeline->GetState().ComputeShader->GetPushConstantRanges();
			assert(ranges.size());
			vkCmdPushConstants(m_CommandBuffer, (VkPipelineLayout)vulkanPipeline->GetPipelineLayoutHandle(),
				ShaderTypeToVulkan(ranges[0].ShaderStage), ranges[0].Offset, ranges[0].Size, pushConstants);
		}

		vkCmdDispatch(m_CommandBuffer, numGroupsX, numGroupsY, numGroupsZ);
	}

	void VulkanCommandBuffer::BeginGraphics(Ref<PipelineGraphics>& pipeline)
	{
		Ref<VulkanPipelineGraphics> vulkanPipeline = Cast<VulkanPipelineGraphics>(pipeline);
		auto& state = pipeline->GetState();
		m_CurrentGraphicsPipeline = vulkanPipeline;

		size_t usedResolveAttachmentsCount = std::count_if(state.ResolveAttachments.begin(), state.ResolveAttachments.end(), [](const auto& attachment) { return attachment.Image; });
		std::vector<VkClearValue> clearValues(state.ColorAttachments.size() + usedResolveAttachmentsCount);
		size_t i = 0;
		for (auto& attachment : state.ColorAttachments)
		{
			VkClearValue clearValue{};
			if (attachment.bClearEnabled)
			{
				memcpy(&clearValue, &attachment.ClearColor, sizeof(clearValue));
				clearValues[i] = clearValue;
			}
			++i;
		}

		if (state.DepthStencilAttachment.Image)
		{
			VkClearValue clearValue{};
			if (state.DepthStencilAttachment.bClearEnabled)
				clearValue.depthStencil = { state.DepthStencilAttachment.DepthClearValue, state.DepthStencilAttachment.StencilClearValue };

			clearValues.push_back(clearValue);
		}

		VkRenderPassBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		beginInfo.renderPass = vulkanPipeline->m_RenderPass;
		beginInfo.framebuffer = vulkanPipeline->m_Framebuffer;
		beginInfo.clearValueCount = uint32_t(clearValues.size());
		beginInfo.pClearValues = clearValues.data();
		beginInfo.renderArea.extent = { vulkanPipeline->m_Width, vulkanPipeline->m_Height };
		vkCmdBeginRenderPass(m_CommandBuffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);

		VkViewport viewport{};
		viewport.width = float(vulkanPipeline->m_Width);
		viewport.height = float(vulkanPipeline->m_Height);
		viewport.minDepth = 0.f;
		viewport.maxDepth = 1.f;
		vkCmdSetViewport(m_CommandBuffer, 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.extent = { vulkanPipeline->m_Width, vulkanPipeline->m_Height };
		vkCmdSetScissor(m_CommandBuffer, 0, 1, &scissor);

		vkCmdBindPipeline(m_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vulkanPipeline->m_GraphicsPipeline);
	}

	void VulkanCommandBuffer::BeginGraphics(Ref<PipelineGraphics>& pipeline, const Ref<Framebuffer>& framebuffer)
	{
		m_CurrentGraphicsPipeline = Cast<VulkanPipelineGraphics>(pipeline);
		auto& state = pipeline->GetState();

		size_t usedResolveAttachmentsCount = std::count_if(state.ResolveAttachments.begin(), state.ResolveAttachments.end(), [](const auto& attachment) { return attachment.Image; });
		std::vector<VkClearValue> clearValues(state.ColorAttachments.size() + usedResolveAttachmentsCount);
		size_t i = 0;
		for (auto& attachment : state.ColorAttachments)
		{
			VkClearValue clearValue{};
			if (attachment.bClearEnabled)
			{
				memcpy(&clearValue, &attachment.ClearColor, sizeof(clearValue));
				clearValues[i] = clearValue;
			}
			++i;
		}

		if (state.DepthStencilAttachment.Image)
		{
			VkClearValue clearValue{};
			if (state.DepthStencilAttachment.bClearEnabled)
				clearValue.depthStencil = { state.DepthStencilAttachment.DepthClearValue, state.DepthStencilAttachment.StencilClearValue };

			clearValues.push_back(clearValue);
		}

		glm::uvec2 size = framebuffer->GetSize();
		VkRenderPassBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		beginInfo.pNext = nullptr;
		beginInfo.renderPass = m_CurrentGraphicsPipeline->m_RenderPass;
		beginInfo.framebuffer = (VkFramebuffer)framebuffer->GetHandle();
		beginInfo.clearValueCount = uint32_t(clearValues.size());
		beginInfo.pClearValues = clearValues.data();
		beginInfo.renderArea.extent = { size.x, size.y };
		vkCmdBeginRenderPass(m_CommandBuffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdBindPipeline(m_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_CurrentGraphicsPipeline->m_GraphicsPipeline);

		VkViewport viewport{};
		viewport.width = float(size.x);
		viewport.height = float(size.y);
		viewport.minDepth = 0.f;
		viewport.maxDepth = 1.f;
		vkCmdSetViewport(m_CommandBuffer, 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.extent = { size.x, size.y };
		vkCmdSetScissor(m_CommandBuffer, 0, 1, &scissor);
	}

	void VulkanCommandBuffer::EndGraphics()
	{
		assert(m_CurrentGraphicsPipeline);

		auto& state = m_CurrentGraphicsPipeline->m_State;
		for (auto& attachment : state.ColorAttachments)
		{
			if (attachment.Image)
				attachment.Image->SetImageLayout(attachment.FinalLayout);
		}
		if (state.DepthStencilAttachment.Image)
			state.DepthStencilAttachment.Image->SetImageLayout(state.DepthStencilAttachment.FinalLayout);

		vkCmdEndRenderPass(m_CommandBuffer);
		m_CurrentGraphicsPipeline = nullptr;
	}

	void VulkanCommandBuffer::Draw(uint32_t vertexCount, uint32_t firstVertex)
	{
		assert(m_CurrentGraphicsPipeline);
		Ref<Pipeline> purePipeline = Cast<Pipeline>(m_CurrentGraphicsPipeline);
		CommitDescriptors(purePipeline, VK_PIPELINE_BIND_POINT_GRAPHICS);
		vkCmdDraw(m_CommandBuffer, vertexCount, 1, firstVertex, 0);
	}

	void VulkanCommandBuffer::DrawIndexedInstanced(const Ref<Buffer>& vertexBuffer, const Ref<Buffer>& indexBuffer, uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset,
		uint32_t instanceCount, uint32_t firstInstance, const Ref<Buffer>& perInstanceBuffer)
	{
		assert(m_CurrentGraphicsPipeline);
		assert(vertexBuffer->HasUsage(BufferUsage::VertexBuffer));
		assert(perInstanceBuffer->HasUsage(BufferUsage::VertexBuffer));
		assert(indexBuffer->HasUsage(BufferUsage::IndexBuffer));

		Ref<VulkanBuffer> vulkanVertexBuffer = Cast<VulkanBuffer>(vertexBuffer);
		Ref<VulkanBuffer> vulkanPerInstanceBuffer = Cast<VulkanBuffer>(perInstanceBuffer);
		Ref<VulkanBuffer> vulkanIndexBuffer = Cast<VulkanBuffer>(indexBuffer);

		Ref<Pipeline> purePipeline = Cast<Pipeline>(m_CurrentGraphicsPipeline);
		CommitDescriptors(purePipeline, VK_PIPELINE_BIND_POINT_GRAPHICS);

		VkBuffer vertexBuffers[2] = { (VkBuffer)vulkanVertexBuffer->GetHandle(), (VkBuffer)vulkanPerInstanceBuffer->GetHandle() };
		VkDeviceSize offsets[] = { 0, 0 };
		vkCmdBindVertexBuffers(m_CommandBuffer, 0, 2, vertexBuffers, offsets);
		vkCmdBindIndexBuffer(m_CommandBuffer, (VkBuffer)vulkanIndexBuffer->GetHandle(), 0, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed(m_CommandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
	}

	void VulkanCommandBuffer::DrawIndexed(const Ref<Buffer>& vertexBuffer, const Ref<Buffer>& indexBuffer, uint32_t indexCount, uint32_t firstIndex, uint32_t vertexOffset, DescriptorWriteData customDescriptor)
	{
		assert(m_CurrentGraphicsPipeline);
		assert(vertexBuffer->HasUsage(BufferUsage::VertexBuffer));
		assert(indexBuffer->HasUsage(BufferUsage::IndexBuffer));

		Ref<VulkanBuffer> vulkanVertexBuffer = Cast<VulkanBuffer>(vertexBuffer);
		Ref<VulkanBuffer> vulkanIndexBuffer = Cast<VulkanBuffer>(indexBuffer);

		Ref<Pipeline> purePipeline = Cast<Pipeline>(m_CurrentGraphicsPipeline);
		CommitDescriptors(purePipeline, VK_PIPELINE_BIND_POINT_GRAPHICS, customDescriptor);

		VkDeviceSize offsets[] = { 0, 0 };
		VkBuffer vkVertex = (VkBuffer)vulkanVertexBuffer->GetHandle();
		VkBuffer vkIndex = (VkBuffer)vulkanIndexBuffer->GetHandle();
		vkCmdBindVertexBuffers(m_CommandBuffer, 0, 1, &vkVertex, offsets);
		vkCmdBindIndexBuffer(m_CommandBuffer, vkIndex, 0, VK_INDEX_TYPE_UINT32);

		vkCmdDrawIndexed(m_CommandBuffer, indexCount, 1, firstIndex, vertexOffset, 0);
	}

	void VulkanCommandBuffer::SetGraphicsRootConstants(const void* vertexRootConstants, const void* fragmentRootConstants)
	{
		assert(m_CurrentGraphicsPipeline);
		const auto& pipelineState = m_CurrentGraphicsPipeline->GetState();

		const Ref<VulkanShader> vs = Cast<VulkanShader>(pipelineState.VertexShader);
		const Ref<VulkanShader> fs = Cast<VulkanShader>(pipelineState.FragmentShader);
		VkPipelineLayout pipelineLayout = (VkPipelineLayout)m_CurrentGraphicsPipeline->GetPipelineLayoutHandle();

		// Also sets fragmentRootConstants if present
		if (vertexRootConstants)
		{
			auto& ranges = vs->GetPushConstantRanges();
			auto& fsRanges = fs->GetPushConstantRanges();
			assert(ranges.size());

			uint32_t fsRangeSize = fragmentRootConstants ? fsRanges[0].Size : 0;
			VkPushConstantRange range;
			range.offset = ranges[0].Offset;
			range.size = (vertexRootConstants == fragmentRootConstants) ? std::max(ranges[0].Size, fsRangeSize) : ranges[0].Size;
			range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
			range.stageFlags |= fragmentRootConstants ? VK_SHADER_STAGE_FRAGMENT_BIT : 0;
			if (range.size)
				vkCmdPushConstants(m_CommandBuffer, pipelineLayout, range.stageFlags, range.offset, range.size, vertexRootConstants);
		}
		if (fragmentRootConstants && (vertexRootConstants != fragmentRootConstants))
		{
			auto& ranges = fs->GetPushConstantRanges();
			assert(ranges.size());
			if (ranges[0].Size == 0)
				return;

			VkPushConstantRange range{};
			range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
			range.offset = ranges[0].Offset;
			range.size = ranges[0].Size;
			if (vertexRootConstants)
			{
				auto& vsRanges = vs->GetPushConstantRanges();
				assert(vsRanges.size());

				range.offset += vsRanges[0].Size;
				range.size -= range.offset;
			}
			if (range.size)
				vkCmdPushConstants(m_CommandBuffer, pipelineLayout, range.stageFlags, range.offset, range.size, fragmentRootConstants);
		}
	}

	void VulkanCommandBuffer::TransitionLayout(Ref<Image>& image, ImageLayout oldLayout, ImageLayout newLayout)
	{
		TransitionLayout(image, ImageView{ 0, image->GetMipsCount(), 0 }, oldLayout, newLayout);
	}

	void VulkanCommandBuffer::TransitionLayout(Ref<Image>& image, const ImageView& imageView, ImageLayout oldLayout, ImageLayout newLayout)
	{
		Ref<VulkanImage> vulkanImage = Cast<VulkanImage>(image);
		image->SetImageLayout(newLayout);
		const VkImageLayout vkOldLayout = ImageLayoutToVulkan(oldLayout);
		const VkImageLayout vkNewLayout = ImageLayoutToVulkan(newLayout);

		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = vkOldLayout;
		barrier.newLayout = vkNewLayout;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = (VkImage)vulkanImage->GetHandle();

		barrier.subresourceRange.baseMipLevel = imageView.MipLevel;
		barrier.subresourceRange.baseArrayLayer = imageView.Layer;
		barrier.subresourceRange.levelCount = imageView.MipLevels;
		barrier.subresourceRange.layerCount = imageView.LayersCount;
		barrier.subresourceRange.aspectMask = vulkanImage->GetTransitionAspectMask(oldLayout, newLayout);

		VkPipelineStageFlags srcStage;
		VkPipelineStageFlags dstStage;

		GetTransitionStagesAndAccesses(vkOldLayout, m_QueueFlags, vkNewLayout, m_QueueFlags, &srcStage, &barrier.srcAccessMask, &dstStage, &barrier.dstAccessMask);

		vkCmdPipelineBarrier(m_CommandBuffer,
			srcStage, dstStage,
			0,
			0, nullptr,
			0, nullptr,
			1, &barrier);
	}

	void VulkanCommandBuffer::ClearColorImage(Ref<Image>& image, const glm::vec4& color)
	{
		Ref<VulkanImage> vulkanImage = Cast<VulkanImage>(image);
		assert(vulkanImage->GetDefaultAspectMask() == VK_IMAGE_ASPECT_COLOR_BIT);
		VkClearColorValue clearColor{};
		clearColor.float32[0] = color.r;
		clearColor.float32[1] = color.g;
		clearColor.float32[2] = color.b;
		clearColor.float32[3] = color.a;

		VkImageSubresourceRange range{};
		range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		range.levelCount = vulkanImage->GetMipsCount();
		range.layerCount = vulkanImage->GetLayersCount();

		vkCmdClearColorImage(m_CommandBuffer, (VkImage)vulkanImage->GetHandle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &range);
	}

	void VulkanCommandBuffer::ClearDepthStencilImage(Ref<Image>& image, float depthValue, uint32_t stencilValue)
	{
		Ref<VulkanImage> vulkanImage = Cast<VulkanImage>(image);
		VkImageAspectFlags aspectMask = vulkanImage->GetDefaultAspectMask();
		VkFormat format = vulkanImage->GetVulkanFormat();
		assert((aspectMask & VK_IMAGE_ASPECT_DEPTH_BIT) > 0 || (aspectMask & VK_IMAGE_ASPECT_STENCIL_BIT));

		VkClearDepthStencilValue clearValue{};
		clearValue.depth = depthValue;
		clearValue.stencil = stencilValue;

		VkImageSubresourceRange range{};
		range.aspectMask = aspectMask;
		range.levelCount = vulkanImage->GetMipsCount();
		range.layerCount = vulkanImage->GetLayersCount();

		vkCmdClearDepthStencilImage(m_CommandBuffer, (VkImage)vulkanImage->GetHandle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue, 1, &range);
	}

	void VulkanCommandBuffer::CopyImage(const Ref<Image>& src, const ImageView& srcView,
		Ref<Image>& dst, const ImageView& dstView,
		const glm::ivec3& srcOffset, const glm::ivec3& dstOffset,
		const glm::uvec3& size)
	{
		Ref<VulkanImage> vulkanSrcImage = Cast<VulkanImage>(src);
		Ref<VulkanImage> vulkanDstImage = Cast<VulkanImage>(dst);

		assert(vulkanSrcImage->GetDefaultAspectMask() == vulkanDstImage->GetDefaultAspectMask());
		assert(vulkanSrcImage->HasUsage(ImageUsage::TransferSrc));
		assert(vulkanDstImage->HasUsage(ImageUsage::TransferDst));

		VkImage vkSrcImage = (VkImage)vulkanSrcImage->GetHandle();
		VkImage vkDstImage = (VkImage)vulkanDstImage->GetHandle();
		VkImageAspectFlags aspectMask = vulkanSrcImage->GetDefaultAspectMask();

		VkImageCopy copyRegion{};
		copyRegion.srcOffset = { srcOffset.x, srcOffset.y, srcOffset.z };
		copyRegion.srcSubresource.aspectMask = aspectMask;
		copyRegion.srcSubresource.mipLevel = srcView.MipLevel;
		copyRegion.srcSubresource.baseArrayLayer = srcView.Layer;
		copyRegion.srcSubresource.layerCount = vulkanSrcImage->GetLayersCount();

		copyRegion.srcOffset = { dstOffset.x, dstOffset.y, dstOffset.z };
		copyRegion.srcSubresource.aspectMask = aspectMask;
		copyRegion.srcSubresource.mipLevel = dstView.MipLevel;
		copyRegion.srcSubresource.baseArrayLayer = dstView.Layer;
		copyRegion.srcSubresource.layerCount = vulkanDstImage->GetLayersCount();

		copyRegion.extent = { size.x, size.y, size.z };
		vkCmdCopyImage(m_CommandBuffer, vkSrcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			vkDstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &copyRegion);
	}

	void VulkanCommandBuffer::TransitionLayout(Ref<Buffer>& buffer, BufferLayout oldLayout, BufferLayout newLayout)
	{
		Ref<VulkanBuffer> vulkanBuffer = Cast<VulkanBuffer>(buffer);

		VkPipelineStageFlags srcStage, dstStage;
		VkAccessFlags srcAccess, dstAccess;

		GetStageAndAccess(oldLayout, m_QueueFlags, &srcStage, &srcAccess);
		GetStageAndAccess(newLayout, m_QueueFlags, &dstStage, &dstAccess);

		VkBufferMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		barrier.srcAccessMask = srcAccess;
		barrier.dstAccessMask = dstAccess;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.buffer = (VkBuffer)vulkanBuffer->GetHandle();
		barrier.size = vulkanBuffer->GetSize();

		vkCmdPipelineBarrier(m_CommandBuffer,
			srcStage, dstStage,
			0,
			0, nullptr,
			1, &barrier,
			0, nullptr);
	}

	void VulkanCommandBuffer::CopyBuffer(const Ref<Buffer>& src, Ref<Buffer>& dst, size_t srcOffset, size_t dstOffset, size_t size)
	{
		Ref<VulkanBuffer> vulkanSrcBuffer = Cast<VulkanBuffer>(src);
		Ref<VulkanBuffer> vulkanDstBuffer = Cast<VulkanBuffer>(dst);

		assert(vulkanSrcBuffer->HasUsage(BufferUsage::TransferSrc));
		assert(vulkanDstBuffer->HasUsage(BufferUsage::TransferDst));

		VkBufferCopy region{};
		region.size = size;
		region.srcOffset = srcOffset;
		region.dstOffset = dstOffset;

		vkCmdCopyBuffer(m_CommandBuffer, (VkBuffer)vulkanSrcBuffer->GetHandle(), (VkBuffer)vulkanDstBuffer->GetHandle(), 1, &region);
	}

	void VulkanCommandBuffer::CopyBuffer(const Ref<StagingBuffer>& src, Ref<Buffer>& dst, size_t srcOffset, size_t dstOffset, size_t size)
	{
		CopyBuffer(src->GetBuffer(), dst, srcOffset, dstOffset, size);
	}

	void VulkanCommandBuffer::FillBuffer(Ref<Buffer>& dst, uint32_t data, size_t offset, size_t numBytes)
	{
		Ref<VulkanBuffer> vulkanBuffer = Cast<VulkanBuffer>(dst);

		assert(vulkanBuffer->HasUsage(BufferUsage::TransferDst));
		assert(numBytes % 4 == 0);

		vkCmdFillBuffer(m_CommandBuffer, (VkBuffer)vulkanBuffer->GetHandle(), offset, numBytes ? numBytes : VK_WHOLE_SIZE, data);
	}

	void VulkanCommandBuffer::CopyBufferToImage(const Ref<Buffer>& src, Ref<Image>& dst, const std::vector<BufferImageCopy>& regions)
	{
		Ref<VulkanBuffer> vulkanBuffer = Cast<VulkanBuffer>(src);
		Ref<VulkanImage> vulkanImage = Cast<VulkanImage>(dst);

		const size_t regionsCount = regions.size();
		assert(vulkanBuffer->HasUsage(BufferUsage::TransferSrc));
		assert(vulkanImage->HasUsage(ImageUsage::TransferDst));
		assert(regionsCount > 0);

		std::vector<VkBufferImageCopy> imageCopyRegions;
		imageCopyRegions.reserve(regionsCount);
		VkImageAspectFlags aspectMask = vulkanImage->GetDefaultAspectMask();

		for (auto& region : regions)
		{
			VkBufferImageCopy copyRegion = imageCopyRegions.emplace_back();
			copyRegion = {};

			copyRegion.bufferOffset = region.BufferOffset;
			copyRegion.bufferRowLength = region.BufferRowLength;
			copyRegion.bufferImageHeight = region.BufferImageHeight;

			copyRegion.imageSubresource.aspectMask = aspectMask;
			copyRegion.imageSubresource.baseArrayLayer = region.ImageArrayLayer;
			copyRegion.imageSubresource.layerCount = region.ImageArrayLayers;
			copyRegion.imageSubresource.mipLevel = region.ImageMipLevel;

			copyRegion.imageOffset = { region.ImageOffset.x, region.ImageOffset.y, region.ImageOffset.z };
			copyRegion.imageExtent = { region.ImageExtent.x, region.ImageExtent.y, region.ImageExtent.z };
		}

		vkCmdCopyBufferToImage(m_CommandBuffer, (VkBuffer)vulkanBuffer->GetHandle(), (VkImage)vulkanImage->GetHandle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			uint32_t(regionsCount), imageCopyRegions.data());
	}

	void VulkanCommandBuffer::CopyImageToBuffer(const Ref<Image>& src, Ref<Buffer>& dst, const std::vector<BufferImageCopy>& regions)
	{
		Ref<VulkanImage> vulkanImage = Cast<VulkanImage>(src);
		Ref<VulkanBuffer> vulkanBuffer = Cast<VulkanBuffer>(dst);

		const size_t regionsCount = regions.size();
		assert(vulkanImage->HasUsage(ImageUsage::TransferSrc));
		assert(vulkanBuffer->HasUsage(BufferUsage::TransferDst));
		assert(regionsCount > 0);

		std::vector<VkBufferImageCopy> imageCopyRegions;
		imageCopyRegions.reserve(regionsCount);
		VkImageAspectFlags aspectMask = vulkanImage->GetDefaultAspectMask();

		for (auto& region : regions)
		{
			VkBufferImageCopy copyRegion = imageCopyRegions.emplace_back();
			copyRegion = {};

			copyRegion.bufferOffset = region.BufferOffset;
			copyRegion.bufferRowLength = region.BufferRowLength;
			copyRegion.bufferImageHeight = region.BufferImageHeight;

			copyRegion.imageSubresource.aspectMask = aspectMask;
			copyRegion.imageSubresource.baseArrayLayer = region.ImageArrayLayer;
			copyRegion.imageSubresource.layerCount = region.ImageArrayLayers;
			copyRegion.imageSubresource.mipLevel = region.ImageMipLevel;

			copyRegion.imageOffset = { region.ImageOffset.x, region.ImageOffset.y, region.ImageOffset.z };
			copyRegion.imageExtent = { region.ImageExtent.x, region.ImageExtent.y, region.ImageExtent.z };
		}

		vkCmdCopyImageToBuffer(m_CommandBuffer, (VkImage)vulkanImage->GetHandle(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			(VkBuffer)vulkanBuffer->GetHandle(), uint32_t(regionsCount), imageCopyRegions.data());
	}

	void VulkanCommandBuffer::Write(Ref<Image>& image, const void* data, size_t size, ImageLayout initialLayout, ImageLayout finalLayout)
	{
		Ref<VulkanImage> vulkanImage = Cast<VulkanImage>(image);

		assert(vulkanImage->HasUsage(ImageUsage::TransferDst));
		assert(!vulkanImage->HasUsage(ImageUsage::DepthStencilAttachment)); // Writing to depth-stencil is not supported

		Ref<StagingBuffer> stagingBuffer = StagingManager::AcquireBuffer(size, false);
		m_UsedStagingBuffers.insert(stagingBuffer.get());
		void* mapped = stagingBuffer->Map();
		memcpy(mapped, data, size);
		stagingBuffer->Unmap();

		if (initialLayout != ImageLayoutType::CopyDest)
			TransitionLayout(image, initialLayout, ImageLayoutType::CopyDest);

		auto& imageSize = vulkanImage->GetSize();
		VkBufferImageCopy region{};
		region.imageSubresource.aspectMask = vulkanImage->GetDefaultAspectMask();
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = vulkanImage->GetLayersCount();
		region.imageExtent = { imageSize.x, imageSize.y, imageSize.z };

		vkCmdCopyBufferToImage(m_CommandBuffer, (VkBuffer)stagingBuffer->GetBuffer()->GetHandle(), (VkImage)vulkanImage->GetHandle(),
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

		if (finalLayout != ImageLayoutType::CopyDest)
			TransitionLayout(image, ImageLayoutType::CopyDest, finalLayout);
	}

	void VulkanCommandBuffer::Write(Ref<Buffer>& buffer, const void* data, size_t size, size_t offset, BufferLayout initialLayout, BufferLayout finalLayout)
	{
		Ref<VulkanBuffer> vulkanBuffer = Cast<VulkanBuffer>(buffer);
		assert(vulkanBuffer);
		assert(vulkanBuffer->HasUsage(BufferUsage::TransferDst));

		Ref<StagingBuffer> stagingBuffer = StagingManager::AcquireBuffer(size, false);
		m_UsedStagingBuffers.insert(stagingBuffer.get());
		void* mapped = stagingBuffer->Map();
		memcpy(mapped, data, size);
		stagingBuffer->Unmap();

		if (initialLayout != BufferLayoutType::CopyDest)
			TransitionLayout(buffer, initialLayout, BufferLayoutType::CopyDest);

		CopyBuffer(stagingBuffer, buffer, 0, offset, size);

		if (finalLayout != BufferLayoutType::CopyDest)
			TransitionLayout(buffer, BufferLayoutType::CopyDest, finalLayout);
	}

	void VulkanCommandBuffer::GenerateMips(Ref<Image>& image, ImageLayout initialLayout, ImageLayout finalLayout)
	{
		assert(image->HasUsage(ImageUsage::TransferSrc | ImageUsage::TransferDst));
		assert(image->GetSamplesCount() == SamplesCount::Samples1); // Multisampled images are not supported

		if (!VulkanContext::GetDevice()->GetPhysicalDevice()->IsMipGenerationSupported(image->GetFormat()))
		{
			EG_RENDERER_ERROR("Physical Device does NOT support mips generation!");
			return;
		}

		glm::ivec3 currentMipSize = image->GetSize();
		VkImage vkImage = (VkImage)image->GetHandle();
		const uint32_t mipCount = image->GetMipsCount();
		const uint32_t layersCount = image->GetLayersCount();

		TransitionLayout(image, initialLayout, ImageLayoutType::CopyDest);
		for (uint32_t i = 1; i < mipCount; ++i)
		{
			// All layers of previous mip are transitioned to copy-source layout
			for (uint32_t layer = 0; layer < layersCount; ++layer)
			{
				ImageView imageView{ i - 1, 1, layer, 1 };
				TransitionLayout(image, imageView, ImageLayoutType::CopyDest, ImageReadAccess::CopySource);
			}

			glm::ivec3 nextMipSize = { currentMipSize.x >> 1, currentMipSize.y >> 1, currentMipSize.z >> 1 };
			nextMipSize = glm::max(glm::ivec3(1), nextMipSize);

			VkImageBlit imageBlit{};
			imageBlit.srcOffsets[0] = { 0, 0, 0 };
			imageBlit.srcOffsets[1] = { currentMipSize.x, currentMipSize.y, currentMipSize.z };
			imageBlit.dstOffsets[0] = { 0, 0, 0 };
			imageBlit.dstOffsets[1] = { nextMipSize.x, nextMipSize.y, nextMipSize.z };

			imageBlit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageBlit.srcSubresource.mipLevel = i - 1;
			imageBlit.srcSubresource.baseArrayLayer = 0;
			imageBlit.srcSubresource.layerCount = layersCount;

			imageBlit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageBlit.dstSubresource.mipLevel = i;
			imageBlit.dstSubresource.baseArrayLayer = 0;
			imageBlit.dstSubresource.layerCount = layersCount;

			currentMipSize = nextMipSize;

			vkCmdBlitImage(m_CommandBuffer,
				vkImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				vkImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1, &imageBlit,
				VK_FILTER_LINEAR);

			// Transition all layers of previous mip to final layout
			for (uint32_t layer = 0; layer < layersCount; ++layer)
			{
				ImageView imageView{ i - 1, 1, layer, 1 };
				TransitionLayout(image, imageView, ImageReadAccess::CopySource, finalLayout);
			}
		}

		// Transition all layers of last mip to final layout
		for (uint32_t layer = 0; layer < layersCount; ++layer)
		{
			ImageView lastImageView{ mipCount - 1, 1, layer, 1 };
			TransitionLayout(image, lastImageView, ImageLayoutType::CopyDest, finalLayout);
		}
	}

#ifdef EG_GPU_TIMINGS
	void VulkanCommandBuffer::StartTiming(Ref<RHIGPUTiming>& timing, uint32_t frameIndex)
	{
		VkQueryPool pool = (VkQueryPool)timing->GetQueryPoolHandle();
		const uint32_t queryIndex = frameIndex * 2;
		vkCmdResetQueryPool(m_CommandBuffer, pool, queryIndex, 2);
		vkCmdWriteTimestamp(m_CommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, pool, queryIndex);
	}

	void VulkanCommandBuffer::EndTiming(Ref<RHIGPUTiming>& timing, uint32_t frameIndex)
	{
		VkQueryPool pool = (VkQueryPool)timing->GetQueryPoolHandle();
		vkCmdWriteTimestamp(m_CommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, pool, frameIndex * 2 + 1);
	}

	void VulkanCommandBuffer::BeginMarker(std::string_view name)
	{
		auto func = VulkanContext::GetFunctions().cmdBeginDebugUtilsLabelEXT;
		if (func)
		{
			EG_ASSERT(name.data());

			VkDebugUtilsLabelEXT label{};
			label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
			label.pLabelName = name.data();
			label.color[0] = 0.0f;
			label.color[1] = 0.0f;
			label.color[2] = 0.0f;
			label.color[3] = 0.0f;

			func(m_CommandBuffer, &label);
		}
	}

	void VulkanCommandBuffer::EndMarker()
	{
		auto func = VulkanContext::GetFunctions().cmdEndDebugUtilsLabelEXT;
		if (func)
		{
			func(m_CommandBuffer);
		}
	}
#endif

	void VulkanCommandBuffer::CommitDescriptors(Ref<Pipeline>& pipeline, VkPipelineBindPoint bindPoint, DescriptorWriteData customDescriptor)
	{
		struct SetData
		{
			const DescriptorSet* DescriptorSet = nullptr;
			DescriptorSetData* Data = nullptr;
			uint32_t Set;
		};

		auto& descriptorSetsData = pipeline->GetDescriptorSetsData();
		std::vector<SetData> dirtyDatas;
		dirtyDatas.reserve(descriptorSetsData.size());
		for (auto& it : descriptorSetsData)
		{
			if (it.second.IsDirty())
				dirtyDatas.push_back({ nullptr, &it.second, it.first });
		}
		if (customDescriptor.DescriptorSetData && customDescriptor.DescriptorSetData->IsDirty())
			dirtyDatas.push_back({ customDescriptor.DescriptorSet, customDescriptor.DescriptorSetData, customDescriptor.DescriptorSet->GetSetIndex() });

		const size_t dirtyDataCount = dirtyDatas.size();
		std::vector<DescriptorWriteData> writeDatas;
		writeDatas.reserve(dirtyDataCount);

		// Populating writeData. Allocating DescriptorSet if neccessary
		auto& descriptorSets = pipeline->GetDescriptorSets();
		for (size_t i = 0; i < dirtyDataCount; ++i)
		{
			uint32_t set = dirtyDatas[i].Set;
			const DescriptorSet* currentDescriptorSet = dirtyDatas[i].DescriptorSet;

			if (currentDescriptorSet == nullptr)
			{
				auto it = descriptorSets.find(set);
				if (it == descriptorSets.end())
					currentDescriptorSet = pipeline->AllocateDescriptorSet(set).get();
				else
					currentDescriptorSet = it->second.get();
			}

			EG_CORE_ASSERT(currentDescriptorSet);
			writeDatas.push_back({ currentDescriptorSet, dirtyDatas[i].Data });
		}

		if (!dirtyDatas.empty())
			DescriptorManager::WriteDescriptors(pipeline, writeDatas);

		VkPipelineLayout vkPipelineLayout = (VkPipelineLayout)pipeline->GetPipelineLayoutHandle();
		for (auto& it : descriptorSetsData)
		{
			uint32_t set = it.first;
			auto it = descriptorSets.find(set);
			assert(it != descriptorSets.end());

			VkDescriptorSet descriptorSet = (VkDescriptorSet)it->second->GetHandle();
			vkCmdBindDescriptorSets(m_CommandBuffer, bindPoint, vkPipelineLayout,
				set, 1, &descriptorSet, 0, nullptr);
		}
		if (customDescriptor.DescriptorSetData)
		{
			uint32_t set = customDescriptor.DescriptorSet->GetSetIndex();

			VkDescriptorSet descriptorSet = (VkDescriptorSet)customDescriptor.DescriptorSet->GetHandle();
			vkCmdBindDescriptorSets(m_CommandBuffer, bindPoint, vkPipelineLayout,
				set, 1, &descriptorSet, 0, nullptr);
		}
	}

}
