#pragma once

#include "VulkanDevice.h"
#include "VulkanAllocator.h"

#include "glm/glm.hpp"

struct GLFWwindow;

namespace Eagle
{
	class Image;
	class Semaphore;
	class Fence;

	class VulkanSwapchain
	{
	public:
		VulkanSwapchain(VkInstance instance, GLFWwindow* window);
		virtual ~VulkanSwapchain();

		void Init(const VulkanDevice* device, bool bEnableVSync);
		VkSurfaceKHR GetSurface() const { return m_Surface; }
		bool IsVSyncEnabled() const { return m_bVSyncEnabled; }

		void SetVSyncEnabled(bool bEnabled);

		void SetOnSwapchainRecreatedCallback(const std::function<void()>& callback) { m_RecreatedCallback = callback; }

		const std::vector<Ref<Image>>& GetImages() const { return m_Images; }
		uint32_t GetImageCount() const { return (uint32_t)m_Images.size(); }

		void Present(const Ref<Semaphore>& waitSemaphore, uint32_t imageIndex);

		// Returns a semaphore that will be signaled when image is ready.
		// A fence is signaled too, so `WaitForAcquireFence` must be called before the next acquire (it's also called automatically)
		const Ref<Semaphore>& AcquireImage(uint32_t frameIndex, uint32_t* outImageIndex);

		// Blocks the CPU until the image returned by the latest `AcquireImage` is actually available (released by the presentation engine).
		// Must be called from the thread that acquires & presents.
		void WaitForAcquireFence();

		glm::uvec2 GetSize() const { return { m_Extent.width, m_Extent.height }; }

		// Returns false if swapchain isn't valid. It can happen if a window is minimized
		bool IsValid() const { return m_Extent.width != 0 && m_Extent.height != 0; }
		
		bool Recreate();

	private:
		void CreateSyncObjects();

	private:
		std::function<void()> m_RecreatedCallback;
		std::vector<Ref<Image>> m_Images;
		std::vector<Ref<Semaphore>> m_WaitSemaphores;
		std::vector<Ref<Fence>> m_AcquireFences;
		Fence* m_PendingAcquireFence = nullptr; // Fence of the latest acquire that hasn't been waited on yet
		SwapchainSupportDetails m_SupportDetails;
		VkExtent2D m_Extent;
		VkSurfaceFormatKHR m_Format;
		VkInstance m_Instance;
		VkSurfaceKHR m_Surface = VK_NULL_HANDLE;
		VkSwapchainKHR m_Swapchain = VK_NULL_HANDLE;
		const VulkanDevice* m_Device = nullptr;
		GLFWwindow* m_Window = nullptr;
		uint32_t m_FrameIndex = 0;
		bool m_bVSyncEnabled = false;
	};
}
