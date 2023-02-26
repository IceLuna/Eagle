#include "egpch.h"
#include "Sampler.h"

#include "Platform/Vulkan/VulkanSampler.h"

namespace Eagle
{
	Ref<Sampler> Sampler::PointSampler;
	Ref<Sampler> Sampler::TrilinearSampler;

	Ref<Sampler> Sampler::Create(FilterMode filterMode, AddressMode addressMode, CompareOperation compareOp, float minLod, float maxLod, float maxAnisotropy)
	{
		switch (RendererContext::Current())
		{
			case RendererAPIType::Vulkan: return MakeRef<VulkanSampler>(filterMode, addressMode, compareOp, minLod, maxLod, maxAnisotropy);
		}

		EG_CORE_ASSERT(false, "Unknown renderer API");
		return nullptr;
	}
}