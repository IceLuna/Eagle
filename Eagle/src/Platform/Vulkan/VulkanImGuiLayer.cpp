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

namespace
{
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

	// ImGui windows that were dragged outside of the main window (platform windows / secondary viewports).
	// Just like the main viewport, they're rendered on the render thread from snapshots of their draw data:
	// the main thread snapshots them in `UpdatePlatform`, and the render thread renders & presents them in `RenderPlatformWindows` after the frame's own work.
	// This keeps everything on the render thread's queue, so the images they display (for exaple, the scene viewport) are synchronized by the same barriers as the main window's ImGui pass.
	struct PlatformWindowToRender
	{
		ImGuiViewport* Viewport = nullptr;
		ImDrawDataSnapshot* Snapshot = nullptr;
		ImVec2 Size;
		bool bClear = true;
	};

	static std::unordered_map<ImGuiID, ImDrawDataSnapshot> s_PlatformWindowSnapshots[Eagle::RendererConfig::FramesInFlight];

	// ImGui's renderer callbacks for platform windows. We call them on the render thread, see `HookPlatformWindowCallbacks`
	static void (*s_OriginalCreateWindow)(ImGuiViewport*) = nullptr;
	static void (*s_OriginalDestroyWindow)(ImGuiViewport*) = nullptr;

	// ImGui calls these from the main thread (in `ImGui::UpdatePlatformWindows()`, `ImGui::NewFrame()`, `ImGui::DestroyPlatformWindows()`).
	// They create/destroy the Vulkan resources of platform windows, which the render thread uses, and they call vkDeviceWaitIdle(),
	// which requires that no other thread uses any queue at the same time. So we run them on the render thread instead.
	// That also guarantees that all frames queued before them, which might still use a destroyed window, have completed.
	// Note: this blocks the main thread until the render thread catches up, but it only happens when a platform window is created/destroyed
	static void HookPlatformWindowCallbacks()
	{
		ImGuiPlatformIO& platformIO = ImGui::GetPlatformIO();
		s_OriginalCreateWindow = platformIO.Renderer_CreateWindow;
		s_OriginalDestroyWindow = platformIO.Renderer_DestroyWindow;
		EG_CORE_ASSERT(s_OriginalCreateWindow && s_OriginalDestroyWindow);

		platformIO.Renderer_CreateWindow = [](ImGuiViewport* viewport)
		{
			Eagle::RenderManager::ExecuteOnRenderThread([viewport]() { s_OriginalCreateWindow(viewport); });
		};
		platformIO.Renderer_DestroyWindow = [](ImGuiViewport* viewport)
		{
			Eagle::RenderManager::ExecuteOnRenderThread([viewport]()
			{
				s_OriginalDestroyWindow(viewport);
				for (auto& snapshots : s_PlatformWindowSnapshots)
					snapshots.erase(viewport->ID);
			});
		};
		
		// Nothing to do here. The swapchain is resized by `ImGui_ImplVulkan_RenderViewport()` on the render thread,
		// when it renders the first frame that was laid out for the new size (the size is snapshotted with the draw data, see `UpdatePlatform`).
		// So the main thread doesn't wait for the render thread on every size change (which happens every frame while a window is being resized),
		// and the frames that are still in flight are rendered into the swapchain of the size they were laid out for
		platformIO.Renderer_SetWindowSize = [](ImGuiViewport*, ImVec2) {};

		// We don't use `ImGui::RenderPlatformWindowsDefault()`. See `UpdatePlatform` and `RenderPlatformWindows`
		platformIO.Renderer_RenderWindow = nullptr;
		platformIO.Renderer_SwapBuffers = nullptr;
	}

#ifdef EG_PLATFORM_WINDOWS
	// The GLFW backend doesn't support OS-level parenting of platform windows (`ImGuiBackendFlags_HasParentViewport`), so each floating window
	// is a top-level window. So while the main window is minimized, they'd stay on screen, frozen and ignoring input, since no UI frames run then.
	// So, like the Win32 backend does, the platform windows that have a parent viewport are owned by the main window.
	// Windows hides them while it's minimized and shows them when it's restored. It also keeps them above it, like tool windows.
	// Note: they're owned by the main window even if their parent viewport is another one. The main window outlives them,
	// while another owner could be destroyed first, which would destroy them too (Windows destroys the windows that an owner owns)
	static void (*s_OriginalPlatformUpdateWindow)(ImGuiViewport*) = nullptr;
	static void (*s_OriginalPlatformShowWindow)(ImGuiViewport*) = nullptr;

	static HWND GetPlatformWindowOwner(const ImGuiViewport* viewport)
	{
		return viewport->ParentViewport ? (HWND)ImGui::GetMainViewport()->PlatformHandleRaw : nullptr;
	}

	// Win32 calls `GWLP_HWNDPARENT` the owner of a window that isn't `WS_CHILD`. `SetParent()` would make it a child window (clipped to the parent)
	static HWND GetWindowOwner(HWND hwnd) { return (HWND)::GetWindowLongPtrW(hwnd, GWLP_HWNDPARENT); }
	static void SetWindowOwner(HWND hwnd, HWND owner) { ::SetWindowLongPtrW(hwnd, GWLP_HWNDPARENT, (LONG_PTR)owner); }

	static void HookPlatformWindowOwnership()
	{
		// Platform windows have the main viewport as their parent (unless a window class sets another one), which matches them being owned by it.
		// ImGui also uses it to guess which windows are above others
		ImGui::GetIO().ConfigViewportsNoDefaultParent = false;

		ImGuiPlatformIO& platformIO = ImGui::GetPlatformIO();
		s_OriginalPlatformUpdateWindow = platformIO.Platform_UpdateWindow;
		s_OriginalPlatformShowWindow = platformIO.Platform_ShowWindow;
		EG_CORE_ASSERT(s_OriginalPlatformShowWindow);

		// ImGui calls it every frame for each platform window, and before a new one is shown. Its parent viewport can change
		platformIO.Platform_UpdateWindow = [](ImGuiViewport* viewport)
		{
			if (s_OriginalPlatformUpdateWindow)
				s_OriginalPlatformUpdateWindow(viewport);

			const HWND hwnd = (HWND)viewport->PlatformHandleRaw;
			const HWND owner = GetPlatformWindowOwner(viewport);
			if (hwnd && GetWindowOwner(hwnd) != owner)
				SetWindowOwner(hwnd, owner);
		};

		// Showing an owned window brings its owner to the front, even if it isn't activated. So the popups and tooltips of a floating window
		// would bring the main window in front of other apps. Like the Win32 backend, they're unowned while they're shown (ImGui #7354, #8669)
		platformIO.Platform_ShowWindow = [](ImGuiViewport* viewport)
		{
			const HWND hwnd = (HWND)viewport->PlatformHandleRaw;
			const HWND owner = hwnd ? GetWindowOwner(hwnd) : nullptr;
			const bool bAvoidBringingOwnerToFront = owner && (viewport->Flags & (ImGuiViewportFlags_NoFocusOnAppearing | ImGuiViewportFlags_NoTaskBarIcon));
			if (bAvoidBringingOwnerToFront)
				SetWindowOwner(hwnd, nullptr);

			s_OriginalPlatformShowWindow(viewport);

			if (bAvoidBringingOwnerToFront)
				SetWindowOwner(hwnd, owner);
		};
	}
#endif
}

namespace Eagle
{
	static constexpr uint32_t s_AdditionalPools = 1;
	static ImDrawDataSnapshot s_Snapshots[RendererConfig::FramesInFlight] = {};

	static std::vector<PlatformWindowToRender> s_PlatformWindowsToRender[RendererConfig::FramesInFlight];

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
		initInfo.QueueFamily = device->GetPhysicalDevice()->GetFamilyIndices().GraphicsFamily;
		initInfo.Queue = device->GetGraphicsQueue(0);
		initInfo.PipelineCache = VulkanPipelineCache::GetCache();
		initInfo.DescriptorPool = pool;
		initInfo.MinImageCount = RendererConfig::FramesInFlight;
		initInfo.ImageCount = RendererConfig::FramesInFlight;
		initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
		ImGui_ImplVulkan_Init(&initInfo, (VkRenderPass)RenderManager::GetPresentRenderPassHandle());
		if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			HookPlatformWindowCallbacks();
#ifdef EG_PLATFORM_WINDOWS
			HookPlatformWindowOwnership();
#endif
		}

		UploadFonts();
	}
	
	void VulkanImGuiLayer::OnDetach()
	{
		for (auto& snapshot : s_Snapshots)
			snapshot.Clear();
		for (auto& windows : s_PlatformWindowsToRender)
			windows.clear();
		for (auto& snapshots : s_PlatformWindowSnapshots)
			snapshots.clear();

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

	void VulkanImGuiLayer::SkipFrame()
	{
		s_PlatformWindowsToRender[RenderManager::GetCurrentFrameIndex_CPU()].clear();

		ImGuiIO& io = ImGui::GetIO();
		io.ClearEventsQueue();
		io.ClearInputKeys();
		io.ClearInputMouse();
	}

	void VulkanImGuiLayer::UpdatePlatform()
	{
		const uint32_t frameIndex = RenderManager::GetCurrentFrameIndex_CPU();
		auto& windowsToRender = s_PlatformWindowsToRender[frameIndex];
		windowsToRender.clear();

		ImGuiIO& io = ImGui::GetIO();
		if ((io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) == 0)
			return;

		// Creates/destroys/resizes platform windows. The renderer callbacks run on the render thread, see `HookPlatformWindowCallbacks`
		ImGui::UpdatePlatformWindows();

		// Instead of `ImGui::RenderPlatformWindowsDefault()`, snapshot the draw data of each platform window.
		// They're rendered by the render thread, see `RenderPlatformWindows`
		const ImGuiPlatformIO& platformIO = ImGui::GetPlatformIO();
		auto& snapshots = s_PlatformWindowSnapshots[frameIndex];
		for (int i = 1; i < platformIO.Viewports.Size; ++i) // Skipping the main viewport
		{
			ImGuiViewport* viewport = platformIO.Viewports[i];
			if (viewport->Flags & ImGuiViewportFlags_IsMinimized)
				continue;
			if (viewport->RendererUserData == nullptr || viewport->DrawData == nullptr || !viewport->DrawData->Valid)
				continue;

			ImDrawDataSnapshot& snapshot = snapshots[viewport->ID];
			snapshot.SnapUsingSwap(viewport->DrawData, frameIndex);

			PlatformWindowToRender& window = windowsToRender.emplace_back();
			window.Viewport = viewport;
			window.Snapshot = &snapshot;
			window.Size = viewport->Size;
			window.bClear = (viewport->Flags & ImGuiViewportFlags_NoRendererClear) == 0;
		}
	}

	void VulkanImGuiLayer::RenderPlatformWindows()
	{
		// Same as `ImGui::RenderPlatformWindowsDefault()`: render all, then present all
		const auto& windowsToRender = s_PlatformWindowsToRender[RenderManager::GetCurrentFrameIndex()];
		for (const auto& window : windowsToRender)
			ImGui_ImplVulkan_RenderViewport(window.Viewport, &window.Snapshot->DrawData, window.bClear, window.Size);
		for (const auto& window : windowsToRender)
			ImGui_ImplVulkan_PresentViewport(window.Viewport);
	}
}
