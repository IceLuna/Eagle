#include "egpch.h"
#include "VulkanDescriptorManager.h"
#include "VulkanContext.h"
#include "VulkanPipeline.h"
#include "VulkanUtils.h"

namespace Eagle
{
//-------------------
// DESCRIPTOR MANAGER
//-------------------
    VulkanDescriptorManager::VulkanDescriptorManager(uint32_t numDescriptors, uint32_t maxSets)
    {
        const VkDescriptorPoolSize poolSizes[] =
        {
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, numDescriptors },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, numDescriptors },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, numDescriptors },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, numDescriptors },
            { VK_DESCRIPTOR_TYPE_SAMPLER, numDescriptors },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, numDescriptors }
        };

        EG_CORE_ASSERT(!m_Device);
        m_Device = VulkanContext::GetDevice()->GetVulkanDevice();

        VkDescriptorPoolCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        info.poolSizeCount = sizeof(poolSizes) / sizeof(VkDescriptorPoolSize);
        info.pPoolSizes = poolSizes;
        info.maxSets = maxSets;
        info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        VK_CHECK(vkCreateDescriptorPool(m_Device, &info, nullptr, &m_DescriptorPool));
    }

    VulkanDescriptorManager::~VulkanDescriptorManager()
    {
        if (m_DescriptorPool)
        {
            RenderManager::SubmitResourceFree([device = m_Device, pool = m_DescriptorPool]()
            {
                    vkDestroyDescriptorPool(device, pool, nullptr);
            });
            m_DescriptorPool = VK_NULL_HANDLE;
        }

        m_Device = VK_NULL_HANDLE;
    }

    Ref<DescriptorSet> VulkanDescriptorManager::CopyDescriptorSet(const Ref<DescriptorSet>& src)
    {
        const VulkanDescriptorSet& descriptor = *(const VulkanDescriptorSet*)src.get();
        return MakeRef<VulkanDescriptorSet>(descriptor);
    }

    Ref<DescriptorSet> VulkanDescriptorManager::AllocateDescriptorSet(const Ref<Pipeline>& pipeline, uint32_t set)
    {
        return MakeRef<VulkanDescriptorSet>(pipeline, m_DescriptorPool, set);
    }

    void VulkanDescriptorManager::WriteDescriptors(const Ref<Pipeline>& pipeline, const std::vector<DescriptorWriteData>& writeDatas)
    {
        std::vector<VkDescriptorBufferInfo> buffers;
        std::vector<VkDescriptorImageInfo> images;
        std::vector<VkWriteDescriptorSet> vkWriteDescriptorSets;
        size_t buffersInfoCount = 0;
        size_t imagesInfoCount = 0;
        Ref<VulkanPipeline> vulkanPipeline = Cast<VulkanPipeline>(pipeline);
        EG_ASSERT(vulkanPipeline);

        for (auto& writeData : writeDatas)
        {
            uint32_t set = writeData.DescriptorSet->GetSetIndex();
            const auto& setBindings = vulkanPipeline->GetSetBindings(set);
            const auto& setBindingsData = writeData.DescriptorSetData->GetBindings();

            for (auto& binding : setBindings)
            {
                const auto& bindingData = setBindingsData.at(binding.binding);

                if (IsBufferType(binding.descriptorType))
                    buffersInfoCount += binding.descriptorCount + bindingData.BufferBindings.size();
                else if (IsImageType(binding.descriptorType))
                    imagesInfoCount += binding.descriptorCount + bindingData.ImageBindings.size();
                else if (IsSamplerType(binding.descriptorType))
                    imagesInfoCount += bindingData.ImageBindings[0].SamplerHandle ? 1 : 0;
            }
        }

        buffers.reserve(buffersInfoCount);
        images.reserve(imagesInfoCount);

        for (auto& writeData : writeDatas)
        {
            uint32_t set = writeData.DescriptorSet->GetSetIndex();
            const auto& setBindings = vulkanPipeline->GetSetBindings(set);
            const auto& setBindingsData = writeData.DescriptorSetData->GetBindings();

            for (auto& binding : setBindings)
            {
                const auto& bindingData = setBindingsData.at(binding.binding);

                if (IsBufferType(binding.descriptorType))
                {
                    if (bindingData.BufferBindings.empty())
                        continue;

                    const size_t startIdx = buffers.size();
                    for (auto& buffer : bindingData.BufferBindings)
                    {
                        VkBuffer vkBuffer = (VkBuffer)buffer.BufferHandle;
                        if (vkBuffer == VK_NULL_HANDLE)
                            EG_RENDERER_ERROR("Invalid buffer binding for binding {0}, set {1}", binding.binding, set);

                        buffers.push_back({ vkBuffer, buffer.Offset, buffer.Range });
                    }

                    uint32_t descriptorCount = std::min(binding.descriptorCount, (uint32_t)bindingData.BufferBindings.size());
                    VkWriteDescriptorSet& writeDescriptorSet = vkWriteDescriptorSets.emplace_back();
                    writeDescriptorSet = {};
                    writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    writeDescriptorSet.descriptorCount = descriptorCount;
                    writeDescriptorSet.descriptorType = binding.descriptorType;
                    writeDescriptorSet.dstBinding = binding.binding;
                    writeDescriptorSet.dstSet = (VkDescriptorSet)writeData.DescriptorSet->GetHandle();
                    writeDescriptorSet.pBufferInfo = &buffers[startIdx];
                }
                else if (IsImageType(binding.descriptorType) || IsSamplerType(binding.descriptorType))
                {
                    if (bindingData.ImageBindings.empty())
                        continue;

                    const size_t startIdx = images.size();
                    if (IsSamplerType(binding.descriptorType))
                    {
                        images.push_back({ (VkSampler)bindingData.ImageBindings[0].SamplerHandle, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED });
                    }
                    else
                    {
                        for (auto& image : bindingData.ImageBindings)
                        {
                            VkSampler sampler = (VkSampler)image.SamplerHandle;
                            VkImageView imageView = (VkImageView)image.ImageViewHandle;
                            VkImageLayout imageLayout;
                            if ((binding.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE) || (binding.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER))
                                imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                            else
                                imageLayout = VK_IMAGE_LAYOUT_GENERAL;

                            images.push_back({ sampler, imageView, imageLayout });
                        }
                    }

                    uint32_t descriptorCount = std::min(binding.descriptorCount, (uint32_t)bindingData.ImageBindings.size());
                    VkWriteDescriptorSet& writeDescriptorSet = vkWriteDescriptorSets.emplace_back();
                    writeDescriptorSet = {};
                    writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    writeDescriptorSet.descriptorCount = descriptorCount;
                    writeDescriptorSet.descriptorType = binding.descriptorType;
                    writeDescriptorSet.dstBinding = binding.binding;
                    writeDescriptorSet.dstSet = (VkDescriptorSet)writeData.DescriptorSet->GetHandle();
                    writeDescriptorSet.pImageInfo = &images[startIdx];
                }
                else
                {
                    EG_RENDERER_ERROR("Unknown binding");
                    EG_CORE_ASSERT(false);
                }
            }

            writeData.DescriptorSetData->OnFlushed();
        }

        vkUpdateDescriptorSets(VulkanContext::GetDevice()->GetVulkanDevice(), (uint32_t)vkWriteDescriptorSets.size(), vkWriteDescriptorSets.data(), 0, nullptr);
    }

//-------------------
//  DESCRIPTOR SET
//-------------------
    VulkanDescriptorSet::VulkanDescriptorSet(const Ref<Pipeline>& pipeline, VkDescriptorPool pool, uint32_t set)
        : DescriptorSet(set)
        , m_Device(VulkanContext::GetDevice()->GetVulkanDevice())
        , m_DescriptorPool(pool)
    {
        Ref<VulkanPipeline> vulkanPipeline = Cast<VulkanPipeline>(pipeline);
        EG_ASSERT(vulkanPipeline);
        m_SetLayout = vulkanPipeline->GetDescriptorSetLayout(m_SetIndex);

        VkDescriptorSetAllocateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        info.descriptorPool = pool;
        info.descriptorSetCount = 1;
        info.pSetLayouts = &m_SetLayout;

        VK_CHECK(vkAllocateDescriptorSets(m_Device, &info, &m_DescriptorSet));
    }

    VulkanDescriptorSet::VulkanDescriptorSet(const VulkanDescriptorSet& other)
        : DescriptorSet(other.m_SetIndex)
        , m_Device(other.m_Device)
        , m_DescriptorPool(other.m_DescriptorPool)
        , m_SetLayout(other.m_SetLayout)
    {
        VkDescriptorSetAllocateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        info.descriptorPool = m_DescriptorPool;
        info.descriptorSetCount = 1;
        info.pSetLayouts = &m_SetLayout;

        VK_CHECK(vkAllocateDescriptorSets(m_Device, &info, &m_DescriptorSet));
    }
    
    VulkanDescriptorSet::~VulkanDescriptorSet()
    {
        if (m_DescriptorSet)
        {
            RenderManager::SubmitResourceFree([device = m_Device, pool = m_DescriptorPool, set = m_DescriptorSet]() {
                vkFreeDescriptorSets(device, pool, 1, &set);
            });
            m_DescriptorSet = VK_NULL_HANDLE;
        }
    }
}
