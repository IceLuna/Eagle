#include "egpch.h"
#include "PipelineGraphics.h"

#include "Eagle/Renderer/RenderManager.h"
#include "Platform/Vulkan/VulkanPipelineGraphics.h"

namespace Eagle
{
	Ref<PipelineGraphics> PipelineGraphics::Create(const PipelineGraphicsState& state, const Ref<PipelineGraphics>& parentPipeline)
	{
		Ref<PipelineGraphics> result;
		switch (RenderManager::GetAPI())
		{
			case RendererAPIType::Vulkan: result = MakeRef<VulkanPipelineGraphics>(state, parentPipeline);
				break;
			default:
				EG_CORE_ASSERT(false, "Unknown API");
		}

		if (result)
		{
			if (state.VertexShader)
				RenderManager::RegisterShaderDependency(state.VertexShader, result);
			if (state.FragmentShader)
				RenderManager::RegisterShaderDependency(state.FragmentShader, result);
			if (state.GeometryShader)
				RenderManager::RegisterShaderDependency(state.GeometryShader, result);
		}

		return result;
	}
}
