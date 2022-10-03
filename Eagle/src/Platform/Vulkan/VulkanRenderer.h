#pragma once

#include "Eagle/Renderer/RendererAPI.h"
#include "Vulkan.h"

namespace Eagle
{
	class VulkanRenderer : public RendererAPI
	{
	public:
		virtual void Init() override;
		virtual void Shutdown() override;

		virtual void BeginFrame() override;
		virtual void EndFrame() override;

		virtual RendererCapabilities& GetCapabilities() override;
	};
}
