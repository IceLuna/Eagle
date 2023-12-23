#include "egpch.h"
#include "VulkanSwapchain.h"
#include "VulkanUtils.h"
#include "VulkanImage.h"
#include "VulkanSemaphore.h"

#include <GLFW/glfw3.h>

namespace Eagle
{
	namespace Utils
	{
		static VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats)
		{
			for (auto& format : formats)
			{
				if (format.format == VK_FORMAT_B8G8R8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
					return format;
			}

			// Return first available format
			return formats[0];
		}

		static VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& modes)
		{
			auto it = std::find(modes.begin(), modes.end(), VK_PRESENT_MODE_MAILBOX_KHR);
			if (it != modes.end())
				return VK_PRESENT_MODE_MAILBOX_KHR;

			it = std::find(modes.begin(), modes.end(), VK_PRESENT_MODE_IMMEDIATE_KHR);
			if (it != modes.end())
				return VK_PRESENT_MODE_IMMEDIATE_KHR;

			return VK_PRESENT_MODE_FIFO_KHR;
		}

		static VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, GLFWwindow* window)
		{
			if (capabilities.currentExtent.width != UINT_MAX)
				return capabilities.currentExtent;

			int width, height;
			glfwGetFramebufferSize(window, &width, &height);

			VkExtent2D extent = { uint32_t(width), uint32_t(height) };
			extent.width = std::clamp(extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
			extent.height = std::clamp(extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

			return extent;
		}
	}

	VulkanSwapchain::VulkanSwapchain(VkInstance instance, GLFWwindow* window)
		: m_Instance(instance)
		, m_Window(window)
	{
		VK_CHECK(glfwCreateWindowSurface(m_Instance, m_Window, nullptr, &m_Surface));
	}

	VulkanSwapchain::~VulkanSwapchain()
	{
		m_WaitSemaphores.clear();
		m_Images.clear();

		RenderManager::SubmitResourceFree([device = m_Device->GetVulkanDevice(), swapchain = m_Swapchain, instance = m_Instance, surface = m_Surface]()
		{
			vkDestroySwapchainKHR(device, swapchain, nullptr);
			vkDestroySurfaceKHR(instance, surface, nullptr);
		});
		
	}

	void VulkanSwapchain::Init(const VulkanDevice* device, bool bEnableVSync)
	{
		m_Device = device;
		m_bVSyncEnabled = bEnableVSync;

		const VulkanPhysicalDevice* physicalDevice = m_Device->GetPhysicalDevice();
		if (!physicalDevice->RequiresPresentQueue())
		{
			EG_RENDERER_CRITICAL("Physical device either does NOT support presenting or does NOT request its support!");
			EG_CORE_ASSERT(false);
			std::exit(-1);
		}

		RecreateSwapchain();
		CreateSyncObjects();
	}

	void VulkanSwapchain::SetVSyncEnabled(bool bEnabled)
	{
		if (m_bVSyncEnabled != bEnabled)
		{
			m_bVSyncEnabled = bEnabled;
			Application::Get().CallNextFrame([this]()
			{
				RenderManager::Wait();
				RecreateSwapchain();
			});
		}
	}

	void VulkanSwapchain::Present(const Ref<Semaphore>& waitSemaphore)
	{
		VkSemaphore vkWaitSemaphore = waitSemaphore ? (VkSemaphore)waitSemaphore->GetHandle() : nullptr;
		VkPresentInfoKHR info{};
		info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		info.pWaitSemaphores = &vkWaitSemaphore;
		info.waitSemaphoreCount = waitSemaphore ? 1 : 0;
		info.swapchainCount = 1;
		info.pSwapchains = &m_Swapchain;
		info.pImageIndices = &m_SwapchainPresentImageIndex;

		VkResult result = vkQueuePresentKHR(m_Device->GetPresentQueue(), &info);
		if (result != VK_SUCCESS)
		{
			if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
			{
				// Swap chain is no longer compatible with the surface and needs to be recreated
				RecreateSwapchain();
				return;
			}
			else VK_CHECK(result);
		}
	}

	const Ref<Semaphore>& VulkanSwapchain::AcquireImage(uint32_t* outFrameIndex)
	{
		auto* semaphore = &m_WaitSemaphores[m_FrameIndex];
		VkSemaphore vkSemaphore = (VkSemaphore)(*semaphore)->GetHandle();
		VkResult result = vkAcquireNextImageKHR(m_Device->GetVulkanDevice(), m_Swapchain, UINT64_MAX, vkSemaphore, VK_NULL_HANDLE, &m_SwapchainPresentImageIndex);
		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
		{
			RecreateSwapchain();

			// Recreate semaphore so it can be used for another `acquire` call
			m_WaitSemaphores[m_FrameIndex] = MakeRef<VulkanSemaphore>();
			auto* semaphore = &m_WaitSemaphores[m_FrameIndex];
			VkSemaphore vkSemaphore = (VkSemaphore)(*semaphore)->GetHandle();
			VK_CHECK(vkAcquireNextImageKHR(m_Device->GetVulkanDevice(), m_Swapchain, UINT64_MAX, vkSemaphore, VK_NULL_HANDLE, &m_SwapchainPresentImageIndex));
		}

		*outFrameIndex = m_SwapchainPresentImageIndex;
		m_FrameIndex = (m_FrameIndex + 1) % uint32_t(m_WaitSemaphores.size());
		return *semaphore;
	}

	void VulkanSwapchain::RecreateSwapchain()
	{
		VkDevice device = m_Device->GetVulkanDevice();
		VkSwapchainKHR oldSwapchain = m_Swapchain;
		m_Swapchain = VK_NULL_HANDLE;
		m_SupportDetails = m_Device->GetPhysicalDevice()->QuerySwapchainSupportDetails(m_Surface);

		const auto& capabilities = m_SupportDetails.Capabilities;

		m_Format = Utils::ChooseSwapSurfaceFormat(m_SupportDetails.Formats);
		VkPresentModeKHR presentMode = m_bVSyncEnabled ? VK_PRESENT_MODE_FIFO_KHR : Utils::ChooseSwapPresentMode(m_SupportDetails.PresentModes);
		m_Extent = Utils::ChooseSwapExtent(capabilities, m_Window);

		uint32_t imageCount = std::max(RendererConfig::FramesInFlight, capabilities.minImageCount + 1);
		if ((capabilities.maxImageCount > 0) && (imageCount > capabilities.maxImageCount))
			imageCount = capabilities.maxImageCount;

		const auto& familyIndices = m_Device->GetPhysicalDevice()->GetFamilyIndices();
		uint32_t queueFamilyIndices[] = { familyIndices.GraphicsFamily, familyIndices.PresentFamily };

		VkSwapchainCreateInfoKHR createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = m_Surface;
		createInfo.minImageCount = imageCount;
		createInfo.imageArrayLayers = 1;
		createInfo.imageExtent = m_Extent;
		createInfo.imageColorSpace = m_Format.colorSpace;
		createInfo.imageFormat = m_Format.format;
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		createInfo.presentMode = presentMode;
		createInfo.oldSwapchain = oldSwapchain;
		createInfo.preTransform = capabilities.currentTransform;
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		createInfo.clipped = VK_TRUE;
		createInfo.pNext = nullptr;
		createInfo.flags = 0;

		if (familyIndices.GraphicsFamily != familyIndices.PresentFamily)
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = 2;
			createInfo.pQueueFamilyIndices = queueFamilyIndices;
		}
		else
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			createInfo.queueFamilyIndexCount = 0;
			createInfo.pQueueFamilyIndices = nullptr;
		}

		VK_CHECK(vkCreateSwapchainKHR(device, &createInfo, nullptr, &m_Swapchain));

		if (oldSwapchain)
			vkDestroySwapchainKHR(device, oldSwapchain, nullptr);

		// Swapchain can create more images than we requested. So we should retrive image count
		vkGetSwapchainImagesKHR(device, m_Swapchain, &imageCount, nullptr);
		std::vector<VkImage> vkImages(imageCount);
		vkGetSwapchainImagesKHR(device, m_Swapchain, &imageCount, vkImages.data());

		ImageSpecifications specs;
		specs.Size = glm::uvec3{ m_Extent.width, m_Extent.height, 1u };
		specs.Format = VulkanToImageFormat(m_Format.format);
		specs.Usage = ImageUsage::ColorAttachment;
		specs.Layout = ImageLayoutType::Present;
		specs.Type = ImageType::Type2D;

		m_Images.clear();
		const std::string debugName = "SwapchainImage";
		int i = 0;
		for (auto& image : vkImages)
			m_Images.push_back(MakeRef<VulkanImage>(image, specs, false, debugName + std::to_string(i++)));

		if (m_RecreatedCallback)
			m_RecreatedCallback();
	}

	void VulkanSwapchain::CreateSyncObjects()
	{
		m_WaitSemaphores.clear();
		for (auto& imageView : m_Images)
			m_WaitSemaphores.push_back(MakeRef<VulkanSemaphore>());
	}
}
