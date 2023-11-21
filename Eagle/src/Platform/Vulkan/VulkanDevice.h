#pragma once

#include "Vulkan.h"
#include "Eagle/Renderer/RendererUtils.h"

namespace Eagle
{
	struct QueueFamilyIndices
	{
		static constexpr uint32_t InvalidQueueIndex = uint32_t(-1);

		uint32_t GraphicsFamily = InvalidQueueIndex;
		uint32_t ComputeFamily = InvalidQueueIndex;
		uint32_t TransferFamily = InvalidQueueIndex;
		uint32_t PresentFamily = InvalidQueueIndex;

		bool IsComplete(bool bRequirePresent) const
		{
			return (GraphicsFamily != InvalidQueueIndex) &&
				(ComputeFamily != InvalidQueueIndex) &&
				(TransferFamily != InvalidQueueIndex) &&
				(!bRequirePresent || (PresentFamily != InvalidQueueIndex));
		}
	};

	struct SwapchainSupportDetails
	{
		VkSurfaceCapabilitiesKHR Capabilities{};
		std::vector<VkSurfaceFormatKHR> Formats;
		std::vector<VkPresentModeKHR> PresentModes;
	};

	struct DeviceSupportedFeatures
	{
		bool bSupportsConservativeRasterization = false;
		bool bAnisotropy = false;
	};

	enum class ImageFormat;

	class VulkanPhysicalDevice
	{
	public:
		VulkanPhysicalDevice(VkSurfaceKHR surface, bool bRequirePresentSupport);

		const QueueFamilyIndices& GetFamilyIndices() const { return m_FamilyIndices; }
		VkPhysicalDevice GetVulkanPhysicalDevice() const { return m_PhysicalDevice; }
		bool RequiresPresentQueue() const { return m_RequiresPresentQueue; }
		const std::vector<const char*> GetDeviceExtensions() const { return m_DeviceExtensions; }
		SwapchainSupportDetails QuerySwapchainSupportDetails(VkSurfaceKHR surface) const;
		const DeviceSupportedFeatures& GetSupportedFeatures() const { return m_SupportedFeatures; }
		ImageFormat GetDepthFormat() const { return m_DepthFormat; }
		bool IsMipGenerationSupported(ImageFormat format) const;

		const VkPhysicalDeviceProperties& GetProperties() const { return m_Properties; }
		const VkPhysicalDeviceDriverProperties& GetDriverProperties() const { return m_DriverProperties; }
		const VkPhysicalDeviceMemoryProperties& GetMemoryProperties() const { return m_MemoryProperties; }

		static Scope<VulkanPhysicalDevice> Select(VkSurfaceKHR surface, bool bRequirePresentSupport) { return MakeScope<VulkanPhysicalDevice>(surface, bRequirePresentSupport); }

	private:
		ImageFormat FindDepthFormat() const;

	private:
		std::vector<const char*> m_DeviceExtensions{};
		VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
		QueueFamilyIndices m_FamilyIndices;
		VkPhysicalDeviceProperties m_Properties;
		VkPhysicalDeviceDriverProperties m_DriverProperties{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES };
		VkPhysicalDeviceMemoryProperties m_MemoryProperties;
		DeviceSupportedFeatures m_SupportedFeatures;
		ImageFormat m_DepthFormat = ImageFormat::Unknown;
		bool m_RequiresPresentQueue = false;
	};

	class VulkanDevice
	{
	public:
		VulkanDevice(const std::unique_ptr<VulkanPhysicalDevice>& physicalDevice, VkPhysicalDeviceFeatures2 enabledFeatures);
		virtual ~VulkanDevice();

		void WaitIdle() const { vkDeviceWaitIdle(m_Device); }

		const VulkanPhysicalDevice* GetPhysicalDevice() const { return m_PhysicalDevice; }
		VkQueue GetGraphicsQueue() const { return m_GraphicsQueue; }
		VkQueue GetComputeQueue() const { return m_ComputeQueue; }
		VkQueue GetTransferQueue() const { return m_TransferQueue; }
		VkQueue GetPresentQueue() const { return m_PresentQueue; }

		VkDevice GetVulkanDevice() const { return m_Device; }

		static Scope<VulkanDevice> Create(const std::unique_ptr<VulkanPhysicalDevice>& physicalDevice, const VkPhysicalDeviceFeatures2& enabledFeatures)
		{
			return MakeScope<VulkanDevice>(physicalDevice, enabledFeatures);
		}

	private:
		VkDevice m_Device = VK_NULL_HANDLE;
		VkQueue m_GraphicsQueue = VK_NULL_HANDLE;
		VkQueue m_ComputeQueue = VK_NULL_HANDLE;
		VkQueue m_TransferQueue = VK_NULL_HANDLE;
		VkQueue m_PresentQueue = VK_NULL_HANDLE;
		const VulkanPhysicalDevice* m_PhysicalDevice = nullptr;
	};
}