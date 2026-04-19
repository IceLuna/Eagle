#include "egpch.h"
#include "VulkanImGuiLayer.h"

#include "Eagle/Core/Application.h"
#include "Eagle/Renderer/RenderManager.h"
#include "Eagle/Renderer/VidWrappers/RenderCommandManager.h"
#include "VulkanContext.h"
#include "VulkanDevice.h"
#include "VulkanSwapchain.h"
#include "VulkanPipelineCache.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <backends/imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <ImGuizmo.h>

struct ImDrawDataSnapshotEntry
{
	ImDrawList* SrcCopy = NULL;     // Drawlist owned by main context
	ImDrawList* OurCopy = NULL;     // Our copy
};

struct ImDrawDataSnapshot
{
	// Members
	ImDrawData                      DrawData;
	ImPool<ImDrawDataSnapshotEntry> Cache;

	// Functions
	~ImDrawDataSnapshot() { Clear(); }
	void                            Clear();
	void                            SnapUsingSwap(ImDrawData* src, uint32_t frameIndex); // Efficient snapshot by swapping data, meaning "src" is unusable.

	// Internals
	ImGuiID                         GetDrawListID(ImDrawList* src_list, uint32_t frameIndex) { return ImHashData(&src_list, sizeof(src_list), frameIndex); }     // Hash pointer
	ImDrawDataSnapshotEntry* GetOrAddEntry(ImDrawList* src_list, uint32_t frameIndex) { return Cache.GetOrAddByKey(GetDrawListID(src_list, frameIndex)); }
};

//-----------------------------------------------------------------------------
// ImDrawDataSnapshot - IMPLEMENTATION
//-----------------------------------------------------------------------------

inline void ImDrawDataSnapshot::Clear()
{
	for (int n = 0; n < Cache.GetMapSize(); n++)
		if (ImDrawDataSnapshotEntry* entry = Cache.TryGetMapData(n))
			IM_DELETE(entry->OurCopy);
	Cache.Clear();
	DrawData.Clear();
}

inline void ImDrawDataSnapshot::SnapUsingSwap(ImDrawData* src, uint32_t frameIndex)
{
	ImDrawData* dst = &DrawData;
	IM_ASSERT(src != dst && src->Valid);

	// Copy all fields except CmdLists[]
	ImVector<ImDrawList*> backup_draw_list;
	backup_draw_list.swap(src->CmdLists);
	IM_ASSERT(src->CmdLists.Data == NULL);
	*dst = *src;
	backup_draw_list.swap(src->CmdLists);

	// Swap and mark as used
	for (ImDrawList* src_list : src->CmdLists)
	{
		ImDrawDataSnapshotEntry* entry = GetOrAddEntry(src_list, frameIndex);
		if (entry->OurCopy == NULL)
		{
			entry->SrcCopy = src_list;
			entry->OurCopy = IM_NEW(ImDrawList)(src_list->_Data);
		}
		IM_ASSERT(entry->SrcCopy == src_list);
		entry->SrcCopy->CmdBuffer.swap(entry->OurCopy->CmdBuffer); // Cheap swap
		entry->SrcCopy->IdxBuffer.swap(entry->OurCopy->IdxBuffer);
		entry->SrcCopy->VtxBuffer.swap(entry->OurCopy->VtxBuffer);
		entry->SrcCopy->CmdBuffer.reserve(entry->OurCopy->CmdBuffer.Capacity); // Preserve bigger size to avoid reallocs for two consecutive frames
		entry->SrcCopy->IdxBuffer.reserve(entry->OurCopy->IdxBuffer.Capacity);
		entry->SrcCopy->VtxBuffer.reserve(entry->OurCopy->VtxBuffer.Capacity);
		dst->CmdLists.push_back(entry->OurCopy);
	}
};

namespace Eagle
{
	static constexpr uint32_t s_AdditionalPools = 1;
	static ImDrawDataSnapshot s_Snapshots[RendererConfig::FramesInFlight] = {};

	void VulkanImGuiLayer::UploadFonts()
	{
		// We can't release ImGui VK data while it's used
		RenderManager::Wait();

		ImGui_ImplVulkan_DestroyFontsTexture();
		Ref<CommandBuffer> commandBuffer = RenderManager::AllocateCommandBuffer(true);
		ImGui_ImplVulkan_CreateFontsTexture((VkCommandBuffer)commandBuffer->GetHandle());
		commandBuffer->End();
		RenderManager::SubmitCommandBuffer(commandBuffer, true);
		ImGui_ImplVulkan_DestroyFontUploadObjects();
	}

	void VulkanImGuiLayer::OnAttach()
	{
		Application& app = Application::Get();
		GLFWwindow* window = app.GetWindow().GetGLFWWindow();

		auto& vulkanContext = VulkanContext::Get();
		auto device = VulkanContext::GetDevice();
		auto vulkanDevice = device->GetVulkanDevice();

		// Create Descriptor Pool
		constexpr uint32_t poolSize = 2048;
		VkDescriptorPoolSize poolSizes[] =
		{
			{ VK_DESCRIPTOR_TYPE_SAMPLER, poolSize },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, poolSize },
			{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, poolSize },
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, poolSize },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, poolSize },
			{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, poolSize },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, poolSize },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, poolSize },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, poolSize },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, poolSize },
			{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, poolSize }
		};
		VkDescriptorPoolCreateInfo poolInfo = {};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
		poolInfo.maxSets = 100 * IM_ARRAYSIZE(poolSizes);
		poolInfo.poolSizeCount = (uint32_t)IM_ARRAYSIZE(poolSizes);
		poolInfo.pPoolSizes = poolSizes;

		m_Pools.reserve(RendererConfig::FramesInFlight + s_AdditionalPools);
		for (uint32_t i = 0; i < RendererConfig::FramesInFlight + s_AdditionalPools; ++i)
		{
			VkDescriptorPool pool;
			VK_CHECK(vkCreateDescriptorPool(vulkanDevice, &poolInfo, nullptr, &pool));
			m_Pools.push_back(pool);
		}

		VkDescriptorPool pool = (VkDescriptorPool)m_DescriptorPool;
		vkCreateDescriptorPool(vulkanDevice, &poolInfo, nullptr, &pool);
		m_DescriptorPool = pool;

		// Setup Platform/Renderer bindings
		ImGui_ImplGlfw_InitForVulkan(window, true);
		ImGui_ImplVulkan_InitInfo initInfo{};
		initInfo.Instance = VulkanContext::GetInstance();
		initInfo.PhysicalDevice = VulkanContext::GetDevice()->GetPhysicalDevice()->GetVulkanPhysicalDevice();
		initInfo.Device = vulkanDevice;
		initInfo.Queue = device->GetGraphicsQueue(1);
		initInfo.PipelineCache = VulkanPipelineCache::GetCache();
		initInfo.DescriptorPool = pool;
		initInfo.MinImageCount = RendererConfig::FramesInFlight;
		initInfo.ImageCount = RendererConfig::FramesInFlight;
		initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
		ImGui_ImplVulkan_Init(&initInfo, (VkRenderPass)RenderManager::GetPresentRenderPassHandle());

		UploadFonts();
	}
	
	void VulkanImGuiLayer::OnDetach()
	{
		for (auto& snapshot : s_Snapshots)
			snapshot.Clear();

		VkDescriptorPool pool = (VkDescriptorPool)m_DescriptorPool;
		VkDevice device = VulkanContext::GetDevice()->GetVulkanDevice();

		VK_CHECK(vkDeviceWaitIdle(device));
		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplGlfw_Shutdown();

		vkDestroyDescriptorPool(device, pool, nullptr);
		for (auto& pool : m_Pools)
			vkDestroyDescriptorPool(device, (VkDescriptorPool)pool, nullptr);
	}
	
	void VulkanImGuiLayer::BeginFrame()
	{
		ImGui_ImplVulkan_NewFrame((VkDescriptorPool)m_Pools[m_FrameIndex]);
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
		ImGuizmo::BeginFrame();

		m_FrameIndex = (m_FrameIndex + 1) % (RendererConfig::FramesInFlight + s_AdditionalPools);
	}
	
	void VulkanImGuiLayer::EndFrame()
	{
		ImGui::Render();

		const uint32_t frameIndex = RenderManager::GetCurrentFrameIndex_CPU();
		auto& snapshot = s_Snapshots[frameIndex];
		snapshot.SnapUsingSwap(ImGui::GetDrawData(), frameIndex);
	}

	void VulkanImGuiLayer::Render(const Ref<CommandBuffer>& cmd)
	{
		auto& snapshot = s_Snapshots[RenderManager::GetCurrentFrameIndex()];
		ImGui_ImplVulkan_RenderDrawData(&snapshot.DrawData, (VkCommandBuffer)cmd->GetHandle());
	}

	void VulkanImGuiLayer::UpdatePlatform()
	{
		ImGuiIO& io = ImGui::GetIO();
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
		}
	}
}
