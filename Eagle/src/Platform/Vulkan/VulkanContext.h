#pragma once

#include "Vulkan.h"
#include "VulkanDevice.h"

#include "Eagle/Renderer/RendererContext.h"
#include "Eagle/Renderer/RenderManager.h"

namespace Eagle
{
	struct VulkanFunctions
	{
		PFN_vkSetDebugUtilsObjectNameEXT setDebugUtilsObjectNameEXT = nullptr;
		PFN_vkCmdBeginDebugUtilsLabelEXT cmdBeginDebugUtilsLabelEXT = nullptr;
		PFN_vkCmdEndDebugUtilsLabelEXT cmdEndDebugUtilsLabelEXT = nullptr;
	};

	class VulkanContext : public RendererContext
	{
	public:
		VulkanContext();
		~VulkanContext();

		// @ surface. Can be VK_NULL_HANDLE
		void InitDevices(VkSurfaceKHR surface, bool bRequireSurface);
		const VulkanDevice* GetContextDevice() { return m_Device.get(); }

		static const char* GetVulkanAPIVersionStr() { return "Vulkan 1.3"; }
		static uint32_t GetVulkanAPIVersion() { return VK_API_VERSION_1_3; }
		static VkInstance GetInstance() { return s_VulkanInstance; }
		static VulkanContext& Get() { return *s_VulkanContext; }
		static const VulkanDevice* GetDevice() { return VulkanContext::Get().m_Device.get(); }

		void WaitIdle() const override { m_Device->WaitIdle(); }
		ImageFormat GetDepthFormat() const override { return m_PhysicalDevice->GetDepthFormat(); }
		virtual const GPUMemoryStats GetMemoryStats() const override;

		static VulkanFunctions& GetFunctions() { return VulkanContext::Get().m_Functions; }

		static void AddResourceDebugName(void* resourceID, const std::string& name, VkObjectType objectType)
		{
			auto& context = VulkanContext::Get();
			{
				std::scoped_lock lock(context.m_Mutex);
				context.m_ResourcesDebugNames[resourceID] = name;
			}

			if (context.m_Functions.setDebugUtilsObjectNameEXT)
			{
				VkDebugUtilsObjectNameInfoEXT nameInfo = {};
				nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
				nameInfo.objectType = objectType;
				nameInfo.pObjectName = name.c_str();
				nameInfo.objectHandle = (size_t)resourceID;
				context.m_Functions.setDebugUtilsObjectNameEXT(context.m_Device->GetVulkanDevice(), &nameInfo);
			}
		}

		static void RemoveResourceDebugName(void* resourceID)
		{
			auto& context = VulkanContext::Get();
			std::scoped_lock lock(context.m_Mutex);
			context.m_ResourcesDebugNames.erase(resourceID);
		}

		static const std::string& GetResourceDebugName(void* resourceID)
		{
			std::scoped_lock lock(Get().m_Mutex);
			auto it = VulkanContext::Get().m_ResourcesDebugNames.find(resourceID);
			if (it == VulkanContext::Get().m_ResourcesDebugNames.end())
			{
				static const std::string s_Unknown = "<Unknown Name>";
				return s_Unknown;
			}
			return it->second;
		}

	private:
		void InitFunctions();

	private:
		std::unordered_map<void*, std::string> m_ResourcesDebugNames;
		VulkanFunctions m_Functions{};
		Scope<VulkanPhysicalDevice> m_PhysicalDevice;
		Scope<VulkanDevice> m_Device;
		std::mutex m_Mutex;
		static VkInstance s_VulkanInstance;
		static VulkanContext* s_VulkanContext;
	};
}
