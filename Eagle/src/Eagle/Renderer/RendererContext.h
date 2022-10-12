#pragma once

#include "Eagle/Renderer/RendererUtils.h"

namespace Eagle
{
	class RendererContext
	{
	public:
		RendererContext() = default;
		virtual ~RendererContext() = default;

		virtual void WaitIdle() const = 0;
		virtual ImageFormat GetDepthFormat() const = 0;

		static Ref<RendererContext> Create();
	};
}
