#pragma once

namespace Eagle
{
	class RendererContext
	{
	public:
		RendererContext() = default;
		virtual ~RendererContext() = default;

		virtual void WaitIdle() const = 0;

		static Ref<RendererContext> Create();
	};
}
