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
#include <implot.h>

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
	static uint32_t s_FrameIndex = 0;
	static ImDrawDataSnapshot s_Snapshots[RendererConfig::FramesInFlight] = {};

	void VulkanImGuiLayer::OnAttach()
	{
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImPlot::CreateContext();

		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  //Enagle Keyboard controls 
		//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad; //Enable Gamepad controls
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;	   //Enable Docking
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;	   //Enable Multi-Viewport
		io.ConfigWindowsMoveFromTitleBarOnly = true;
		io.ConfigDebugHighlightIdConflicts = false;
		io.ConfigDebugHighlightIdConflictsShowItemPicker = false;

		m_IniPath = (Application::GetCorePath() / "imgui.ini").u8string();
		const Path boldFont = Application::GetCorePath() / "assets/fonts/opensans/OpenSans-Bold.ttf";
		const Path regularFont = Application::GetCorePath() / "assets/fonts/opensans/OpenSans-Regular.ttf";

		io.IniFilename = m_IniPath.c_str();
		if (std::filesystem::exists(boldFont))
		{
			io.Fonts->AddFontFromFileTTF(boldFont.string().c_str(), 32.f * Window::s_HighDPIScaleFactor, 0, ImGui::GetIO().Fonts->GetGlyphRangesCyrillic());
			io.Fonts->Fonts[0]->Scale = 0.5f;
		}
		if (std::filesystem::exists(regularFont))
		{
			io.FontDefault = io.Fonts->AddFontFromFileTTF(regularFont.string().c_str(), 32.f * Window::s_HighDPIScaleFactor, 0, ImGui::GetIO().Fonts->GetGlyphRangesCyrillic());
			io.FontDefault->Scale = 0.5f;
		}

		ImGuiStyle& style = ImGui::GetStyle();
		style.ScaleAllSizes(Window::s_HighDPIScaleFactor);
		style.TabRounding = 8.f;
		style.FrameRounding = 8.f;
		style.GrabRounding = 8.f;
		style.WindowRounding = 8.f;
		style.PopupRounding = 8.f;

		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			//style.WindowRounding = 0.f;
			style.Colors[ImGuiCol_WindowBg].w = 1.f;
		}

		SetDarkThemeColors();

		// ImGuizmo style
		{
			ImGuizmo::Style& style = ImGuizmo::GetStyle();
			style.RotationLineThickness = 6.f;
			style.RotationOuterLineThickness = 6.f;
			style.TranslationLineArrowSize = 12.f;
			style.Colors[ImGuizmo::DIRECTION_X] = ImGui::ColorConvertU32ToFloat4(0xFF715ED8);
			style.Colors[ImGuizmo::DIRECTION_Y] = ImGui::ColorConvertU32ToFloat4(0xFF25AA25);
			style.Colors[ImGuizmo::DIRECTION_Z] = ImGui::ColorConvertU32ToFloat4(0xFFCC532C);
			style.Colors[ImGuizmo::PLANE_X] = ImGui::ColorConvertU32ToFloat4(0xFF7A68D8);
			style.Colors[ImGuizmo::PLANE_Y] = ImGui::ColorConvertU32ToFloat4(0xFF55AB55);
			style.Colors[ImGuizmo::PLANE_Z] = ImGui::ColorConvertU32ToFloat4(0xFFD96742);
			style.Colors[ImGuizmo::SELECTION] = ImGui::ColorConvertU32ToFloat4(0xFF20AACC);
			ImGuizmo::SetGizmoSizeClipSpace(0.15f);
		}

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
		initInfo.Queue = device->GetGraphicsQueue();
		initInfo.PipelineCache = VulkanPipelineCache::GetCache();
		initInfo.DescriptorPool = pool;
		initInfo.MinImageCount = RendererConfig::FramesInFlight;
		initInfo.ImageCount = RendererConfig::FramesInFlight;
		initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
		ImGui_ImplVulkan_Init(&initInfo, (VkRenderPass)RenderManager::GetPresentRenderPassHandle());

		// Upload Fonts
		{
			// Use any command queue
			Ref<CommandBuffer> commandBuffer = RenderManager::AllocateCommandBuffer(true);
			ImGui_ImplVulkan_CreateFontsTexture((VkCommandBuffer)commandBuffer->GetHandle());
			commandBuffer->End();
			RenderManager::SubmitCommandBuffer(commandBuffer, true);
			ImGui_ImplVulkan_DestroyFontUploadObjects();
		}
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
		ImPlot::DestroyContext();
		ImGui::DestroyContext();

		vkDestroyDescriptorPool(device, pool, nullptr);
		for (auto& pool : m_Pools)
			vkDestroyDescriptorPool(device, (VkDescriptorPool)pool, nullptr);
	}
	
	void VulkanImGuiLayer::BeginFrame()
	{
		ImGui_ImplVulkan_NewFrame((VkDescriptorPool)m_Pools[s_FrameIndex]);
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
		ImGuizmo::BeginFrame();

		s_FrameIndex = (s_FrameIndex + 1) % (RendererConfig::FramesInFlight + s_AdditionalPools);
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
		ImGuiIO& io = ImGui::GetIO(); (void)io;
		// Update and Render additional Platform Windows
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
		}
	}
}
