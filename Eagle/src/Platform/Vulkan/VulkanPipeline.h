#pragma once

#include "Eagle/Renderer/VidWrappers/Pipeline.h"

#include "Vulkan.h"

namespace Eagle
{
	class VulkanPipeline : virtual public Pipeline
	{
	public:
		const std::vector<VkDescriptorSetLayoutBinding>& GetSetBindings(uint32_t set) const { return m_SetBindings[set]; }
		VkDescriptorSetLayout GetDescriptorSetLayout(uint32_t set) const { assert(set < m_SetLayouts.size()); return m_SetLayouts[set]; }
		virtual ~VulkanPipeline()
		{
			ClearSetLayouts();
		}

		void ClearSetLayouts();

	protected:
		std::vector<VkDescriptorSetLayout> m_SetLayouts;
		std::vector<std::vector<VkDescriptorSetLayoutBinding>> m_SetBindings; // Not owned! Set -> Bindings
	};
}
