#include "egpch.h"
#include "VulkanContext.h"
#include "VulkanPipelineCache.h"
#include "VulkanAllocator.h"
#include "VulkanUtils.h"

namespace Eagle
{

#ifdef EG_DEBUG
	static constexpr bool s_EnableValidation = true;
#else
	static constexpr bool s_EnableValidation = false;
#endif

	namespace Utils
	{
		static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
			VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
			void* pUserData)
		{
			if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
				EG_RENDERER_INFO("{}", pCallbackData->pMessage);
			else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
				EG_RENDERER_WARN("{}", pCallbackData->pMessage);
			else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
				EG_RENDERER_ERROR("{}", pCallbackData->pMessage);
			else
				EG_RENDERER_TRACE("{}", pCallbackData->pMessage);

			// Parse for image handle
			{
				static const char* s_ImagePrefixes[] = { "using image (", "use image ", "expects image ", "of VkImage (", "For image " , "with image " };
				const char* imageHandleStr = nullptr;
				for (const char* prefix : s_ImagePrefixes)
				{
					imageHandleStr = strstr(pCallbackData->pMessage, prefix);
					if (imageHandleStr != nullptr)
					{
						// Found handle
						imageHandleStr += strlen(prefix);

						uint64_t imageHandle = uint64_t(-1);
						sscanf_s(imageHandleStr, "%zu", &imageHandle);

						VkImage image = (VkImage)(imageHandle);
						std::string imageName = VulkanContext::GetResourceDebugName(image);

						if (!imageName.empty())
						{
							EG_RENDERER_ERROR("Image in question: ID {0}, Name = {1}", imageHandle, imageName);
							break;
						}
					}
				}
			}
			// Parse for buffer handle
			{
				static const char* s_BufferPrefixes[] = { "buffer ", "bound Buffer ", "Buffer (" };
				const char* bufferHandleStr = nullptr;
				for (const char* prefix : s_BufferPrefixes)
				{
					bufferHandleStr = strstr(pCallbackData->pMessage, prefix);
					if (bufferHandleStr != nullptr)
					{
						// Found handle
						bufferHandleStr += strlen(prefix);

						uint64_t bufferHandle = uint64_t(-1);
						sscanf_s(bufferHandleStr, "%zu", &bufferHandle);

						VkBuffer buffer = (VkBuffer)(bufferHandle);
						std::string bufferName = VulkanContext::GetResourceDebugName(buffer);

						if (!bufferName.empty())
						{
							EG_RENDERER_ERROR("Buffer in question: ID {0}, Name = {1}", bufferHandle, bufferName);
							break;
						}
					}
				}
			}

			if (std::string(pCallbackData->pMessage).find("You are adding") != std::string::npos)
				__debugbreak();

			return VK_FALSE; //To not abort caller
		}

		static void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT* outCI)
		{
			outCI->sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
			outCI->messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
				VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
				VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
			outCI->messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
				| VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
			outCI->pfnUserCallback = DebugCallback;
			outCI->pUserData = nullptr;
			outCI->pNext = nullptr;
			outCI->flags = 0;
		}

		static bool AreLayersSupported(const std::vector<const char*>& layers)
		{
			uint32_t count = 0;
			vkEnumerateInstanceLayerProperties(&count, nullptr);
			std::vector<VkLayerProperties> layerProps(count);
			vkEnumerateInstanceLayerProperties(&count, layerProps.data());

			std::set<std::string> layersSet(layers.begin(), layers.end());

			for (auto& layerProp : layerProps)
			{
				if (layersSet.find(layerProp.layerName) != layersSet.end())
					layersSet.erase(layerProp.layerName);

				if (layersSet.empty())
					return true;
			}

			EG_RENDERER_WARN("Some layers are not supported:");
			for (auto& layer : layersSet)
				EG_CORE_WARN("\t{0}", layer);

			return false;
		}

		static VkInstance CreateInstance(const std::vector<const char*>& extensions)
		{
			const std::vector<const char*> validationLayers{ "VK_LAYER_KHRONOS_validation" };

			VkApplicationInfo appInfo{};
			appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
			appInfo.apiVersion = VulkanContext::GetVulkanAPIVersion();
			appInfo.applicationVersion = VK_MAKE_VERSION(EG_VERSION_MAJOR, EG_VERSION_MINOR, EG_VERSION_PATCH);
			appInfo.engineVersion = VK_MAKE_VERSION(EG_VERSION_MAJOR, EG_VERSION_MINOR, EG_VERSION_PATCH);
			appInfo.pApplicationName = "Eagle Engine"; // TODO: Get project name
			appInfo.pEngineName = "Eagle Engine";

			VkInstanceCreateInfo instanceCI{};
			instanceCI.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
			instanceCI.pApplicationInfo = &appInfo;
			instanceCI.enabledExtensionCount = (uint32_t)extensions.size();
			instanceCI.ppEnabledExtensionNames = extensions.data();
			VkDebugUtilsMessengerCreateInfoEXT debugCI{};
			if (s_EnableValidation && AreLayersSupported(validationLayers))
			{
				PopulateDebugMessengerCreateInfo(&debugCI);
				instanceCI.enabledLayerCount = (uint32_t)validationLayers.size();
				instanceCI.ppEnabledLayerNames = validationLayers.data();
				instanceCI.pNext = &debugCI;
			}

			VkInstance result = nullptr;
			VK_CHECK(vkCreateInstance(&instanceCI, nullptr, &result));
			return result;
		}

		static std::vector<const char*> GetRequiredExtensions()
		{
			std::vector<const char*> instanceExtensions = { VK_KHR_SURFACE_EXTENSION_NAME, "VK_KHR_win32_surface" };
			if constexpr (s_EnableValidation)
			{
				instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
			}
			else
			{
#if EG_GPU_MARKERS // enable debug utils if markers requested
				instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif
			}
			return instanceExtensions;
		}

		static VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
			const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pMessenger)
		{
			auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
			if (func)
				return func(instance, pCreateInfo, pAllocator, pMessenger);
			else
			{
				EG_RENDERER_ERROR("Failed to get vkCreateDebugUtilsMessengerEXT");
				return VK_ERROR_EXTENSION_NOT_PRESENT;
			}
		}

		static void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT messenger, const VkAllocationCallbacks* pAllocator)
		{
			auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
			if (func)
				func(instance, messenger, pAllocator);
			else
				EG_RENDERER_ERROR("Failed to get vkDestroyDebugUtilsMessengerEXT");
		}

		static VkDebugUtilsMessengerEXT CreateDebugUtils(VkInstance instance)
		{
			if constexpr (!s_EnableValidation)
				return VK_NULL_HANDLE;

			VkDebugUtilsMessengerCreateInfoEXT ci;
			PopulateDebugMessengerCreateInfo(&ci);

			VkDebugUtilsMessengerEXT result{};
			VK_CHECK(CreateDebugUtilsMessengerEXT(instance, &ci, nullptr, &result));
			return result;
		}
	}

	VkInstance VulkanContext::s_VulkanInstance = nullptr;
	VulkanContext* VulkanContext::s_VulkanContext = nullptr;
	static VkDebugUtilsMessengerEXT s_DebugMessenger = VK_NULL_HANDLE;

	VulkanContext::VulkanContext()
	{
		EG_CORE_ASSERT(!s_VulkanInstance); // Instance already created

		s_VulkanInstance = Utils::CreateInstance(Utils::GetRequiredExtensions());
		s_DebugMessenger = Utils::CreateDebugUtils(s_VulkanInstance);
		s_VulkanContext = this;
	}

	VulkanContext::~VulkanContext()
	{
		VulkanAllocator::Shutdown();
		VulkanPipelineCache::Shutdown();
		m_Device.reset();
		m_PhysicalDevice.reset();

		if (s_DebugMessenger)
		{
			Utils::DestroyDebugUtilsMessengerEXT(s_VulkanInstance, s_DebugMessenger, nullptr);
			s_DebugMessenger = VK_NULL_HANDLE;
		}

		vkDestroyInstance(s_VulkanInstance, nullptr);
		s_VulkanInstance = nullptr;
	}

	void VulkanContext::InitDevices(VkSurfaceKHR surface, bool bRequireSurface)
	{
		EG_CORE_ASSERT(m_PhysicalDevice == nullptr);
		m_PhysicalDevice = VulkanPhysicalDevice::Select(surface, bRequireSurface);

		VkPhysicalDeviceVulkan12Features deviceFeatures12 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
		deviceFeatures12.descriptorIndexing = VK_TRUE;
		deviceFeatures12.runtimeDescriptorArray = VK_TRUE;
		deviceFeatures12.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
		deviceFeatures12.shaderStorageBufferArrayNonUniformIndexing = VK_TRUE;
		deviceFeatures12.descriptorBindingVariableDescriptorCount = VK_TRUE;
		deviceFeatures12.descriptorBindingPartiallyBound = VK_TRUE;
		deviceFeatures12.imagelessFramebuffer = VK_TRUE;

		const bool bSupportsAnisotropy = m_PhysicalDevice->GetSupportedFeatures().bAnisotropy;
		VkPhysicalDeviceFeatures2 features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
		features.features.wideLines = VK_TRUE;
		features.features.independentBlend = VK_TRUE;
		features.features.samplerAnisotropy = bSupportsAnisotropy;
		features.pNext = &deviceFeatures12;

		m_Device = VulkanDevice::Create(m_PhysicalDevice, features);

		InitFunctions();
		VulkanPipelineCache::Init();
		VulkanAllocator::Init();

		// Init caps. Dump GPU Info
		auto& caps = m_Caps;
		auto& props = m_PhysicalDevice->GetProperties();
		caps.Vendor = VulkanVendorIDToString(props.vendorID);
		caps.Device = props.deviceName;
		caps.DriverVersion = std::to_string(props.driverVersion);
		caps.ApiVersion = std::to_string(props.apiVersion);
		caps.MaxAnisotropy = bSupportsAnisotropy ? props.limits.maxSamplerAnisotropy : 1.f;
		caps.MaxSamples = props.limits.maxDescriptorSetSamplers;
		Utils::DumpGPUInfo();
	}

	const GPUMemoryStats VulkanContext::GetMemoryStats() const
	{
		return VulkanAllocator::GetStats();
	}

	void VulkanContext::InitFunctions()
	{
		VkDevice device = m_Device->GetVulkanDevice();
		m_Functions.setDebugUtilsObjectNameEXT = (PFN_vkSetDebugUtilsObjectNameEXT)(void*)vkGetDeviceProcAddr(device, "vkSetDebugUtilsObjectNameEXT");
		m_Functions.cmdBeginDebugUtilsLabelEXT = (PFN_vkCmdBeginDebugUtilsLabelEXT)vkGetDeviceProcAddr(device, "vkCmdBeginDebugUtilsLabelEXT");
		m_Functions.cmdEndDebugUtilsLabelEXT = (PFN_vkCmdEndDebugUtilsLabelEXT)vkGetDeviceProcAddr(device, "vkCmdEndDebugUtilsLabelEXT");
	}
}
