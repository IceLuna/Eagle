#include "egpch.h"
#include "DescriptorManager.h"
#include "Eagle/Renderer/RendererContext.h"
#include "Platform/Vulkan/VulkanDescriptorManager.h"

namespace Eagle
{
	void DescriptorManager::WriteDescriptors(const Ref<Pipeline>& pipeline, const std::vector<DescriptorWriteData>& writeDatas)
	{
		switch (RendererContext::Current())
		{
			case RendererAPIType::Vulkan: return VulkanDescriptorManager::WriteDescriptors(pipeline, writeDatas);
		}

		EG_CORE_ASSERT(false, "Unknown renderer API");
	}

	Ref<DescriptorManager> DescriptorManager::Create(uint32_t numDescriptors, uint32_t maxSets)
	{
		EG_CORE_ASSERT(numDescriptors <= DescriptorManager::MaxNumDescriptors);
		EG_CORE_ASSERT(maxSets <= DescriptorManager::MaxSets);

		switch (RendererContext::Current())
		{
			case RendererAPIType::Vulkan: return MakeRef<VulkanDescriptorManager>(numDescriptors, maxSets);
		}

		EG_CORE_ASSERT(false, "Unknown renderer API");
		return nullptr;
	}
}
