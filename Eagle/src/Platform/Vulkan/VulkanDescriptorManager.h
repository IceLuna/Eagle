#pragma once

#include "Vulkan.h"
#include "Eagle/Renderer/DescriptorManager.h"

namespace Eagle
{
	class VulkanDescriptorManager : public DescriptorManager
	{
	public:
		VulkanDescriptorManager(uint32_t numDescriptors, uint32_t maxSets);
		~VulkanDescriptorManager();

		Ref<DescriptorSet> CopyDescriptorSet(const Ref<DescriptorSet>& src) override;
		Ref<DescriptorSet> AllocateDescriptorSet(const Ref<Pipeline>& pipeline, uint32_t set) override;
		static void WriteDescriptors(const Ref<Pipeline>& pipeline, const std::vector<DescriptorWriteData>& writeDatas);

	private:
		VkDevice m_Device = VK_NULL_HANDLE;
		VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
	};

	class VulkanDescriptorSet : public DescriptorSet
	{
	public:
		VulkanDescriptorSet(const Ref<Pipeline>& pipeline, VkDescriptorPool pool, uint32_t set);
		VulkanDescriptorSet(const VulkanDescriptorSet& other);
		virtual ~VulkanDescriptorSet();

		void* GetHandle() const { return m_DescriptorSet; }

	private:
		VkDevice m_Device = VK_NULL_HANDLE;
		VkDescriptorSet m_DescriptorSet = VK_NULL_HANDLE;
		VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
		VkDescriptorSetLayout m_SetLayout = VK_NULL_HANDLE;
		friend class VulkanDescriptorManager;
	};
}
