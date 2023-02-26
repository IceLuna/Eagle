#pragma once

#include "VulkanDevice.h"
#include "VulkanAllocator.h"

#include "glm/glm.hpp"

struct GLFWwindow;

namespace Eagle
{
	class Image;
	class Semaphore;

	class VulkanSwapchain
	{
	public:
		VulkanSwapchain(VkInstance instance, GLFWwindow* window);
		virtual ~VulkanSwapchain();

		void Init(const VulkanDevice* device, bool bEnableVSync);
		VkSurfaceKHR GetSurface() const { return m_Surface; }
		bool IsVSyncEnabled() const { return m_bVSyncEnabled; }

		void SetVSyncEnabled(bool bEnabled)
		{
			if (m_bVSyncEnabled != bEnabled)
			{
				m_bVSyncEnabled = bEnabled;
				RecreateSwapchain();
			}
		}

		void OnResized() { RecreateSwapchain(); }

		void SetOnSwapchainRecreatedCallback(const std::function<void()>& callback) { m_RecreatedCallback = callback; }

		const std::vector<Ref<Image>>& GetImages() const { return m_Images; }
		uint32_t GetImageCount() const { return (uint32_t)m_Images.size(); }

		void Present(const Ref<Semaphore>& waitSemaphore);

		// Returns a semaphore that will be signaled when image is ready
		const Ref<Semaphore>& AcquireImage(uint32_t* outFrameIndex);

		uint32_t GetFrameIndex() const { return m_FrameIndex; }
		glm::uvec2 GetSize() const { return { m_Extent.width, m_Extent.height }; }

	private:
		void RecreateSwapchain();
		void CreateSyncObjects();

	private:
		std::function<void()> m_RecreatedCallback;
		std::vector<Ref<Image>> m_Images;
		std::vector<Ref<Semaphore>> m_WaitSemaphores;
		SwapchainSupportDetails m_SupportDetails;
		VkExtent2D m_Extent;
		VkSurfaceFormatKHR m_Format;
		VkInstance m_Instance;
		VkSurfaceKHR m_Surface = VK_NULL_HANDLE;
		VkSwapchainKHR m_Swapchain = VK_NULL_HANDLE;
		const VulkanDevice* m_Device = nullptr;
		GLFWwindow* m_Window = nullptr;
		uint32_t m_FrameIndex = 0;
		uint32_t m_SwapchainPresentImageIndex = 0;
		bool m_bVSyncEnabled = false;
	};
}