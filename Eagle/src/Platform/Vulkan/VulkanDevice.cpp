#include "egpch.h"
#include "VulkanDevice.h"
#include "VulkanContext.h"
#include "VulkanUtils.h"

namespace Eagle
{
	namespace Utils
	{
		struct SupportedTextureCompressions
		{
			bool bTextureCompressionASTC_LDR = false;
			bool bTextureCompressionETC2 = false;
			bool bTextureCompressionBC = false;
		};

		static QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface, bool bRequirePresent)
		{
			QueueFamilyIndices result;

			uint32_t queueFamilyCount = 0;
			vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
			std::vector<VkQueueFamilyProperties> familyProps(queueFamilyCount);
			vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, familyProps.data());

			constexpr uint32_t queueFlags = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT;
			// Dedicated queue for compute
			// Try to find a queue family index that supports compute but not graphics
			if (queueFlags & VK_QUEUE_COMPUTE_BIT)
			{
				for (uint32_t i = 0; i < familyProps.size(); ++i)
				{
					auto& queueProps = familyProps[i];
					if ((queueProps.queueFlags & VK_QUEUE_COMPUTE_BIT) && ((queueProps.queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0))
					{
						result.ComputeFamily = i;
						break;
					}
				}
			}

			// Dedicated queue for transfer
			// Try to find a queue family index that supports transfer but not graphics and compute
			if (queueFlags & VK_QUEUE_TRANSFER_BIT)
			{
				for (uint32_t i = 0; i < familyProps.size(); ++i)
				{
					auto& queueProps = familyProps[i];
					if ((queueProps.queueFlags & VK_QUEUE_TRANSFER_BIT) && ((queueProps.queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0) && ((queueProps.queueFlags & VK_QUEUE_COMPUTE_BIT) == 0))
					{
						result.TransferFamily = i;
						break;
					}
				}
			}

			VkBool32 supportsPresent = VK_FALSE;
			for (uint32_t i = 0; i < familyProps.size(); ++i)
			{
				auto& queueProps = familyProps[i];

				if ((queueFlags & VK_QUEUE_COMPUTE_BIT) && (result.ComputeFamily == -1))
					if (queueProps.queueFlags & VK_QUEUE_COMPUTE_BIT)
						result.ComputeFamily = i;

				if ((queueFlags & VK_QUEUE_TRANSFER_BIT) && (result.TransferFamily == -1))
					if (queueProps.queueFlags & VK_QUEUE_TRANSFER_BIT)
						result.TransferFamily = i;

				if ((queueFlags & VK_QUEUE_GRAPHICS_BIT) && (result.GraphicsFamily == -1))
					if (queueProps.queueFlags & VK_QUEUE_GRAPHICS_BIT)
						result.GraphicsFamily = i;

				if (surface != VK_NULL_HANDLE)
					vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &supportsPresent);
				if (supportsPresent)
					result.PresentFamily = i;

				if (result.IsComplete(bRequirePresent))
					break;
			}

			return result;
		}

		static SwapchainSupportDetails QuerySwapchainSupport(VkPhysicalDevice device, VkSurfaceKHR surface)
		{
			SwapchainSupportDetails result;
			vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &result.Capabilities);

			uint32_t formatCount = 0;
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
			result.Formats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, result.Formats.data());

			uint32_t presentModeCount = 0;
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
			result.PresentModes.resize(presentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, result.PresentModes.data());

			return result;
		}

		static bool AreExtensionsSupported(VkPhysicalDevice device, const std::vector<const char*>& extensions)
		{
			uint32_t count = 0;
			vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
			std::vector<VkExtensionProperties> props(count);
			vkEnumerateDeviceExtensionProperties(device, nullptr, &count, props.data());

			std::set<std::string> requestedExtensions(extensions.begin(), extensions.end());

			for (auto& prop : props)
				requestedExtensions.erase(prop.extensionName);

			return requestedExtensions.empty();
		}

		static bool CheckForNonUniformIndexing(VkPhysicalDevice physicalDevice)
		{
			VkPhysicalDeviceVulkan12Features features12 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
			VkPhysicalDeviceFeatures2 features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
			features.pNext = &features12;
			vkGetPhysicalDeviceFeatures2(physicalDevice, &features);
			
			return features12.shaderSampledImageArrayNonUniformIndexing;
		}

		static bool DoesSupportBindlessTextures(VkPhysicalDevice physicalDevice)
		{
			VkPhysicalDeviceDescriptorIndexingFeatures indexingFeatures{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT, nullptr };
			VkPhysicalDeviceFeatures2 deviceFeatures{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &indexingFeatures };
			vkGetPhysicalDeviceFeatures2(physicalDevice, &deviceFeatures);
			return indexingFeatures.descriptorBindingPartiallyBound && indexingFeatures.runtimeDescriptorArray && indexingFeatures.descriptorBindingSampledImageUpdateAfterBind;
		}

		static bool CheckForAnisotropy(VkPhysicalDevice physicalDevice)
		{
			VkPhysicalDeviceFeatures supportedFeatures;
			vkGetPhysicalDeviceFeatures(physicalDevice, &supportedFeatures);

			return supportedFeatures.samplerAnisotropy;
		}

		static SupportedTextureCompressions CheckForTextureCompressionSupport(VkPhysicalDevice physicalDevice)
		{
			VkPhysicalDeviceFeatures supportedFeatures;
			vkGetPhysicalDeviceFeatures(physicalDevice, &supportedFeatures);
			
			SupportedTextureCompressions result;
			result.bTextureCompressionASTC_LDR = supportedFeatures.textureCompressionASTC_LDR;
			result.bTextureCompressionBC = supportedFeatures.textureCompressionBC;
			result.bTextureCompressionETC2 = supportedFeatures.textureCompressionETC2;

			return result;
		}

		static bool IsDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface, bool bRequirePresent, const std::vector<const char*>& extensions,
			QueueFamilyIndices* outFamilyIndices, SwapchainSupportDetails* outSwapchainSupportDetails)
		{
			*outFamilyIndices = FindQueueFamilies(device, surface, bRequirePresent);
			bool bSupportsIndirectIndexing = CheckForNonUniformIndexing(device);
			bool bExtensionsSupported = AreExtensionsSupported(device, extensions);
			bool bSupportsBindlessTextures = DoesSupportBindlessTextures(device);
			bool bSuitable = bSupportsIndirectIndexing && bExtensionsSupported && bSupportsBindlessTextures && outFamilyIndices->IsComplete(bRequirePresent);

			bool bSwapchainAdequate = true;
			if (bRequirePresent && bSuitable)
			{
				*outSwapchainSupportDetails = QuerySwapchainSupport(device, surface);
				bSwapchainAdequate = outSwapchainSupportDetails->Formats.size() > 0 && outSwapchainSupportDetails->PresentModes.size() > 0;
			}

			return bSwapchainAdequate && bSuitable;
		}
	}

	/////////////////////
	// Physical Device
	/////////////////////
	VulkanPhysicalDevice::VulkanPhysicalDevice(VkSurfaceKHR surface, bool bRequirePresentSupport)
		: m_RequiresPresentQueue(bRequirePresentSupport)
	{
		VkInstance instance = VulkanContext::GetInstance();
		uint32_t count = 0;
		vkEnumeratePhysicalDevices(instance, &count, nullptr);
		std::vector<VkPhysicalDevice> physicalDevices(count);
		vkEnumeratePhysicalDevices(instance, &count, physicalDevices.data());

		assert(count != 0);

		m_DeviceExtensions.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
		m_DeviceExtensions.push_back(VK_KHR_IMAGELESS_FRAMEBUFFER_EXTENSION_NAME);
		m_DeviceExtensions.push_back(VK_KHR_MULTIVIEW_EXTENSION_NAME);
		m_DeviceExtensions.push_back(VK_EXT_POST_DEPTH_COVERAGE_EXTENSION_NAME);
		if (bRequirePresentSupport)
			m_DeviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

		SwapchainSupportDetails supportDetails;
		for (auto& device : physicalDevices)
		{
			vkGetPhysicalDeviceProperties(device, &m_Properties);
			if (Utils::IsDeviceSuitable(device, surface, bRequirePresentSupport, m_DeviceExtensions, &m_FamilyIndices, &supportDetails))
			{
				m_PhysicalDevice = device;
				if (m_Properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
					break;
			}
		}

		if (!m_PhysicalDevice)
		{
			EG_RENDERER_CRITICAL("Didn't find a suitalbe GPU!");
			EG_RENDERER_CRITICAL("GPU has to support at least {}", VulkanContext::GetVulkanAPIVersionStr());
			EG_RENDERER_CRITICAL("Required extensions:");
			for (auto& extension : m_DeviceExtensions)
				EG_RENDERER_CRITICAL("\t{}", extension);
			std::exit(-1);
		}

		// Get driver props
		{
			VkPhysicalDeviceProperties2 props2;
			props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
			props2.pNext = &m_DriverProperties;
			vkGetPhysicalDeviceProperties2(m_PhysicalDevice, &props2);
		}

		vkGetPhysicalDeviceMemoryProperties(m_PhysicalDevice, &m_MemoryProperties);
		if (Utils::AreExtensionsSupported(m_PhysicalDevice, std::vector<const char*>{ VK_EXT_CONSERVATIVE_RASTERIZATION_EXTENSION_NAME }))
		{
			m_SupportedFeatures.bSupportsConservativeRasterization = true;
			m_DeviceExtensions.push_back(VK_EXT_CONSERVATIVE_RASTERIZATION_EXTENSION_NAME);
		}
		auto supportedTextureCompressions = Utils::CheckForTextureCompressionSupport(m_PhysicalDevice);

		m_SupportedFeatures.bAnisotropy = Utils::CheckForAnisotropy(m_PhysicalDevice);
		m_SupportedFeatures.bTextureCompressionASTC_LDR = supportedTextureCompressions.bTextureCompressionASTC_LDR;
		m_SupportedFeatures.bTextureCompressionBC = supportedTextureCompressions.bTextureCompressionBC;
		m_SupportedFeatures.bTextureCompressionETC2 = supportedTextureCompressions.bTextureCompressionETC2;

		m_DepthFormat = FindDepthFormat();
	}

	SwapchainSupportDetails VulkanPhysicalDevice::QuerySwapchainSupportDetails(VkSurfaceKHR surface) const
	{
		return Utils::QuerySwapchainSupport(m_PhysicalDevice, surface);
    }

	ImageFormat VulkanPhysicalDevice::FindDepthFormat() const
	{
		static const std::vector<VkFormat> depthFormats =
		{
			VK_FORMAT_D32_SFLOAT,
			VK_FORMAT_D16_UNORM,
			VK_FORMAT_D32_SFLOAT_S8_UINT,
			VK_FORMAT_D24_UNORM_S8_UINT,
			VK_FORMAT_D16_UNORM_S8_UINT,
		};

		for (auto& format : depthFormats)
		{
			VkFormatProperties props;
			vkGetPhysicalDeviceFormatProperties(m_PhysicalDevice, format, &props);

			if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
				return VulkanToImageFormat(format);
		}

		return ImageFormat::Unknown;
	}

	bool VulkanPhysicalDevice::IsMipGenerationSupported(ImageFormat format) const
	{
		VkFormatProperties props{};
		vkGetPhysicalDeviceFormatProperties(m_PhysicalDevice, ImageFormatToVulkan(format), &props);
		return props.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
	}

	/////////////////////
	// Logical Device
	/////////////////////
	VulkanDevice::VulkanDevice(const std::unique_ptr<VulkanPhysicalDevice>& physicalDevice, VkPhysicalDeviceFeatures2 enabledFeatures)
		: m_PhysicalDevice(physicalDevice.get())
	{
		constexpr float queuePriority = 1.f;
		auto& queueFamilyIndices = physicalDevice->GetFamilyIndices();
		const auto& deviceExtensions = physicalDevice->GetDeviceExtensions();

		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos(1);
		queueCreateInfos[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfos[0].queueCount = 1;
		queueCreateInfos[0].queueFamilyIndex = queueFamilyIndices.GraphicsFamily;
		queueCreateInfos[0].pQueuePriorities = &queuePriority;

		const bool bPresentSameAsGraphics = queueFamilyIndices.GraphicsFamily == queueFamilyIndices.PresentFamily;
		const bool bComputeSameAsGraphics = queueFamilyIndices.GraphicsFamily == queueFamilyIndices.ComputeFamily;
		const bool bTransferSameAsGraphics = queueFamilyIndices.GraphicsFamily == queueFamilyIndices.TransferFamily;
		const bool bTransferSameAsCompute = queueFamilyIndices.ComputeFamily == queueFamilyIndices.TransferFamily;
		const bool bRequiresPresentQueue = physicalDevice->RequiresPresentQueue();

		VkDeviceQueueCreateInfo additionalQueueCI{};
		additionalQueueCI.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		additionalQueueCI.queueCount = 1;
		additionalQueueCI.queueFamilyIndex = queueFamilyIndices.PresentFamily;
		additionalQueueCI.pQueuePriorities = &queuePriority;
		if (bRequiresPresentQueue && !bPresentSameAsGraphics)
		{
			additionalQueueCI.queueFamilyIndex = queueFamilyIndices.PresentFamily;
			queueCreateInfos.push_back(additionalQueueCI);
		}
		if (!bComputeSameAsGraphics)
		{
			additionalQueueCI.queueFamilyIndex = queueFamilyIndices.ComputeFamily;
			queueCreateInfos.push_back(additionalQueueCI);
		}
		if (!bTransferSameAsGraphics && !bTransferSameAsCompute)
		{
			additionalQueueCI.queueFamilyIndex = queueFamilyIndices.TransferFamily;
			queueCreateInfos.push_back(additionalQueueCI);
		}

		VkPhysicalDeviceMultiviewFeaturesKHR physicalDeviceMultiviewFeatures{};
		physicalDeviceMultiviewFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES_KHR;
		physicalDeviceMultiviewFeatures.multiview = VK_TRUE;
		physicalDeviceMultiviewFeatures.pNext = &enabledFeatures;

		VkDeviceCreateInfo deviceCI{};
		deviceCI.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		deviceCI.pQueueCreateInfos = queueCreateInfos.data();
		deviceCI.queueCreateInfoCount = (uint32_t)queueCreateInfos.size();
		deviceCI.enabledExtensionCount = (uint32_t)deviceExtensions.size();
		deviceCI.ppEnabledExtensionNames = deviceExtensions.data();
		deviceCI.pNext = &physicalDeviceMultiviewFeatures;

		VK_CHECK(vkCreateDevice(physicalDevice->GetVulkanPhysicalDevice(), &deviceCI, nullptr, &m_Device));
		vkGetDeviceQueue(m_Device, queueFamilyIndices.GraphicsFamily, 0, &m_GraphicsQueue);
		vkGetDeviceQueue(m_Device, queueFamilyIndices.ComputeFamily, 0, &m_ComputeQueue);
		vkGetDeviceQueue(m_Device, queueFamilyIndices.TransferFamily, 0, &m_TransferQueue);
		if (bRequiresPresentQueue)
		{
			if (!bPresentSameAsGraphics)
				vkGetDeviceQueue(m_Device, queueFamilyIndices.PresentFamily, 0, &m_PresentQueue);
			else
				m_PresentQueue = m_GraphicsQueue;
		}
	}

	VulkanDevice::~VulkanDevice()
	{
		vkDestroyDevice(m_Device, nullptr);
	}
}
