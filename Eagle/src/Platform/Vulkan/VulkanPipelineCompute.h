#pragma once

#include "VulkanPipeline.h"
#include "Eagle/Renderer/PipelineCompute.h"

namespace Eagle
{
	class VulkanPipelineCompute : virtual public VulkanPipeline, virtual public PipelineCompute
	{
	public:
		VulkanPipelineCompute(const PipelineComputeState& state, const Ref<PipelineCompute>& parentPipeline = nullptr);
		virtual ~VulkanPipelineCompute();

		void* GetPipelineHandle() const override { return m_ComputePipeline; }
		void* GetPipelineLayoutHandle() const override { return m_PipelineLayout; }

		void Recreate() override;

	private:
		void Create(VkPipeline parentPipeline);

	private:
		VkPipeline m_ComputePipeline = VK_NULL_HANDLE;
		VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;

		friend class VulkanCommandBuffer;
	};
}
