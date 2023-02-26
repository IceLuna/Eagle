#include "egpch.h"
#include "PipelineCompute.h"

#include "Renderer.h"
#include "Platform/Vulkan/VulkanPipelineCompute.h"

namespace Eagle
{
	Ref<PipelineCompute> PipelineCompute::Create(const PipelineComputeState& state, const Ref<PipelineCompute>& parentPipeline)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPIType::Vulkan: return MakeRef<VulkanPipelineCompute>(state, parentPipeline);
		}

		EG_CORE_ASSERT(false, "Unknown API");
		return nullptr;
	}
}
