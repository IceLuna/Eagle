#include "egpch.h"
#include "PipelineCompute.h"

#include "Eagle/Renderer/RenderManager.h"

#include "Platform/Vulkan/VulkanPipelineCompute.h"

namespace Eagle
{
	Ref<PipelineCompute> PipelineCompute::Create(const PipelineComputeState& state, const Ref<PipelineCompute>& parentPipeline)
	{
		Ref<PipelineCompute> result;

		switch (RenderManager::GetAPI())
		{
			case RendererAPIType::Vulkan: result = MakeRef<VulkanPipelineCompute>(state, parentPipeline);
				break;
			default:
				EG_CORE_ASSERT(false, "Unknown API");
		}

		if (result && state.ComputeShader)
			RenderManager::RegisterShaderDependency(state.ComputeShader, result);

		return result;
	}
}
