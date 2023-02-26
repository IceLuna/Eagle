#include "egpch.h"
#include "PipelineGraphics.h"

#include "Renderer.h"
#include "Platform/Vulkan/VulkanPipelineGraphics.h"

namespace Eagle
{
	Ref<PipelineGraphics> PipelineGraphics::Create(const PipelineGraphicsState& state, const Ref<PipelineGraphics>& parentPipeline)
	{
		Ref<PipelineGraphics> result;
		switch (Renderer::GetAPI())
		{
			case RendererAPIType::Vulkan: result = MakeRef<VulkanPipelineGraphics>(state, parentPipeline);
				break;
			default:
				EG_CORE_ASSERT(false, "Unknown API");
		}

		if (result)
		{
			if (state.VertexShader)
				Renderer::RegisterShaderDependency(state.VertexShader, result);
			if (state.FragmentShader)
				Renderer::RegisterShaderDependency(state.FragmentShader, result);
			if (state.GeometryShader)
				Renderer::RegisterShaderDependency(state.GeometryShader, result);
		}

		return result;
	}
}
