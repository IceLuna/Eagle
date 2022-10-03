#include "egpch.h"
#include "PipelineGraphics.h"

#include "Renderer.h"
#include "Platform/Vulkan/VulkanPipelineGraphics.h"

namespace Eagle
{
	Ref<PipelineGraphics> PipelineGraphics::Create(const PipelineGraphicsState& state, const Ref<PipelineGraphics>& parentPipeline)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPIType::Vulkan: return MakeRef<VulkanPipelineGraphics>(state, parentPipeline);
		}

		EG_CORE_ASSERT(false, "Unknown API");
		return nullptr;
	}
}
