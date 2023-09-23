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
#include <backends/imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <ImGuizmo.h>

namespace Eagle
{
	static constexpr uint32_t s_AdditionalPools = 1;
	static uint32_t s_FrameIndex = 0;

	void VulkanImGuiLayer::OnAttach()
	{
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  //Enagle Keyboard controls 
		//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad; //Enable Gamepad controls
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;	   //Enable Docking
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;	   //Enable Multi-Viewport

		io.Fonts->AddFontFromFileTTF("assets/fonts/opensans/OpenSans-Bold.ttf", 32.f * Window::s_HighDPIScaleFactor, 0, ImGui::GetIO().Fonts->GetGlyphRangesCyrillic());
		io.FontDefault = io.Fonts->AddFontFromFileTTF("assets/fonts/opensans/OpenSans-Regular.ttf", 32.f * Window::s_HighDPIScaleFactor, 0, ImGui::GetIO().Fonts->GetGlyphRangesCyrillic());
		io.FontDefault->Scale = 0.5f;
		io.Fonts->Fonts[0]->Scale = 0.5f;

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

		Application& app = Application::Get();
		GLFWwindow* window = static_cast<GLFWwindow*>(app.GetWindow().GetGLFWWindow());

		auto& vulkanContext = VulkanContext::Get();
		auto device = VulkanContext::GetDevice();
		auto vulkanDevice = device->GetVulkanDevice();

		// Create Descriptor Pool
		constexpr uint32_t poolSize = 1000;
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

		VkDescriptorPool pool = (VkDescriptorPool)m_PersistantDescriptorPool;
		vkCreateDescriptorPool(vulkanDevice, &poolInfo, nullptr, &pool);
		m_PersistantDescriptorPool = pool;

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
		VkDescriptorPool presistantDescPool = (VkDescriptorPool)m_PersistantDescriptorPool;

		{
			VkDevice device = VulkanContext::GetDevice()->GetVulkanDevice();

			VK_CHECK(vkDeviceWaitIdle(device));
			ImGui_ImplVulkan_Shutdown();
			ImGui_ImplGlfw_Shutdown();
			ImGui::DestroyContext();

			vkDestroyDescriptorPool(device, presistantDescPool, nullptr);
			for (auto& pool : m_Pools)
				vkDestroyDescriptorPool(device, (VkDescriptorPool)pool, nullptr);
		}
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
	}

	void VulkanImGuiLayer::Render(Ref<CommandBuffer>& cmd)
	{
		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), (VkCommandBuffer)cmd->GetHandle());
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
