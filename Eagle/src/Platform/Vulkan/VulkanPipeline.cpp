#include "egpch.h"
#include "VulkanPipeline.h"
#include "VulkanContext.h"

#include "Eagle/Renderer/Renderer.h"

void Eagle::VulkanPipeline::ClearSetLayouts()
{
	Renderer::SubmitResourceFree([setLayouts = m_SetLayouts]()
	{
		VkDevice device = VulkanContext::GetDevice()->GetVulkanDevice();
		for (auto& layout : setLayouts)
			vkDestroyDescriptorSetLayout(device, layout, nullptr);
	});
	m_SetLayouts.clear();
}
