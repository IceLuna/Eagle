#include "egpch.h"
#include "VulkanPipelineCompute.h"
#include "VulkanContext.h"
#include "VulkanShader.h"
#include "VulkanUtils.h"
#include "VulkanPipelineCache.h"

namespace Eagle
{
	VulkanPipelineCompute::VulkanPipelineCompute(const PipelineComputeState& state, const Ref<PipelineCompute>& parentPipeline)
		: PipelineCompute(state)
	{
		assert(m_State.ComputeShader->GetType() == ShaderType::Compute);

		VkPipeline vkParentPipeline = parentPipeline ? (VkPipeline)parentPipeline->GetPipelineHandle() : VK_NULL_HANDLE;
		Create(vkParentPipeline);
	}

	VulkanPipelineCompute::~VulkanPipelineCompute()
	{
		RenderManager::SubmitResourceFree([pipeline = m_ComputePipeline, pipelineLayout = m_PipelineLayout]()
		{
			VkDevice device = VulkanContext::GetDevice()->GetVulkanDevice();

			vkDestroyPipeline(device, pipeline, nullptr);
			vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
		});
	}

	void VulkanPipelineCompute::SetState(const PipelineComputeState& state)
	{
		if (m_State.ComputeShader != state.ComputeShader)
		{
			const Ref<Pipeline> thisPipeline = shared_from_this();
			RenderManager::RemoveShaderDependency(m_State.ComputeShader.get(), thisPipeline);
			RenderManager::RegisterShaderDependency(state.ComputeShader.get(), thisPipeline);
		}
		m_State = state;

		Recreate();
	}

	void VulkanPipelineCompute::Recreate()
	{
		VkDevice device = VulkanContext::GetDevice()->GetVulkanDevice();
		VkPipeline prevThisPipeline = m_ComputePipeline;
		VkPipelineLayout prevThisPipelineLayout = m_PipelineLayout;

		Create(prevThisPipeline);

		RenderManager::SubmitResourceFree([pipeline = prevThisPipeline, pipelineLayout = prevThisPipelineLayout]()
		{
			VkDevice device = VulkanContext::GetDevice()->GetVulkanDevice();

			vkDestroyPipeline(device, pipeline, nullptr);
			vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
		});
	}
	
	void VulkanPipelineCompute::Create(VkPipeline parentPipeline)
	{
		for (auto& set : m_DescriptorSets)
			set.clear();

		// Mark each descriptor as dirty so that there's no need to call
		// `pipeline->Set*` (for example, pipeline->SetBuffer) after pipeline reloading
		for (auto& perFrameData : m_DescriptorSetData)
			for (auto& data : perFrameData)
				data.second.MakeDirty();

		VkDevice device = VulkanContext::GetDevice()->GetVulkanDevice();
		Ref<VulkanShader> computeShader = Cast<VulkanShader>(m_State.ComputeShader);
		// Pipeline layout
		{
			m_SetBindings = computeShader->GetLayoutSetBindings();
			const uint32_t setsCount = (uint32_t)m_SetBindings.size();

			ClearSetLayouts();
			m_SetLayouts.resize(setsCount);
			for (uint32_t i = 0; i < setsCount; ++i)
			{
				VkDescriptorSetLayoutCreateInfo layoutInfo{};
				layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
				layoutInfo.bindingCount = (uint32_t)m_SetBindings[i].size();
				layoutInfo.pBindings = m_SetBindings[i].data();

				VK_CHECK(vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &m_SetLayouts[i]));
			}

			std::vector<VkPushConstantRange> pushConstants;
			for (auto& range : computeShader->GetPushConstantRanges())
			{
				VkPushConstantRange vkRange;
				vkRange.stageFlags = ShaderTypeToVulkan(range.ShaderStage);
				vkRange.offset = range.Offset;
				vkRange.size = range.Size;
				pushConstants.push_back(vkRange);
			}

			VkPipelineLayoutCreateInfo pipelineLayoutCI{};
			pipelineLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			pipelineLayoutCI.setLayoutCount = setsCount;
			pipelineLayoutCI.pSetLayouts = m_SetLayouts.data();
			pipelineLayoutCI.pushConstantRangeCount = (uint32_t)pushConstants.size();
			pipelineLayoutCI.pPushConstantRanges = pushConstants.data();

			VK_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &m_PipelineLayout));
		}

		VkPipelineShaderStageCreateInfo shaderStage = computeShader->GetPipelineShaderStageInfo();
		VkSpecializationInfo specializationInfo{};
		std::vector<VkSpecializationMapEntry> mapEntries;
		if (m_State.ComputeSpecializationInfo.Data && (m_State.ComputeSpecializationInfo.Size > 0))
		{
			const size_t mapEntriesCount = m_State.ComputeSpecializationInfo.MapEntries.size();
			mapEntries.reserve(mapEntriesCount);
			for (auto& entry : m_State.ComputeSpecializationInfo.MapEntries)
				mapEntries.emplace_back(VkSpecializationMapEntry{ entry.ConstantID, entry.Offset, entry.Size });

			specializationInfo.pData = m_State.ComputeSpecializationInfo.Data;
			specializationInfo.dataSize = m_State.ComputeSpecializationInfo.Size;
			specializationInfo.mapEntryCount = (uint32_t)mapEntriesCount;
			specializationInfo.pMapEntries = mapEntries.data();
			shaderStage.pSpecializationInfo = &specializationInfo;
		}

		VkComputePipelineCreateInfo info{};
		info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		info.stage = shaderStage;
		info.layout = m_PipelineLayout;
		info.basePipelineIndex = -1;
		info.basePipelineHandle = parentPipeline;
		VK_CHECK(vkCreateComputePipelines(device, VulkanPipelineCache::GetCache(), 1, &info, nullptr, &m_ComputePipeline));
	}
}
