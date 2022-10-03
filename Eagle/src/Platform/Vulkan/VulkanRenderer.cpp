#include "egpch.h"
#include "VulkanRenderer.h"

#include "Eagle/Renderer/Renderer.h"
#include "Eagle/Renderer/Buffer.h"

#include "VulkanContext.h"
#include "VulkanDevice.h"
#include "VulkanUtils.h"
#include "VulkanSwapchain.h"

namespace Eagle
{
	struct VulkanRendererData
	{
		RendererCapabilities RendererCapabilities;
	};

	static VulkanRendererData* s_VulkanData = nullptr;

	void VulkanRenderer::Init()
	{
		s_VulkanData = new VulkanRendererData();
		const auto& config = Renderer::GetConfig();

		auto& caps = s_VulkanData->RendererCapabilities;
		auto& props = VulkanContext::GetDevice()->GetPhysicalDevice()->GetProperties();
		caps.Vendor = VulkanVendorIDToString(props.vendorID);
		caps.Device = props.deviceName;
		caps.Version = std::to_string(props.driverVersion);

		Utils::DumpGPUInfo();
	}
	
	void VulkanRenderer::Shutdown()
	{
		delete s_VulkanData;
		s_VulkanData = nullptr;
	}

	void VulkanRenderer::BeginFrame()
	{
		
	}

	void VulkanRenderer::EndFrame()
	{
	}

	RendererCapabilities& VulkanRenderer::GetCapabilities()
	{
		return s_VulkanData->RendererCapabilities;
	}
}
