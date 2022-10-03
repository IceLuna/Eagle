#pragma once

namespace Eagle
{
	class RendererContext
	{
	public:
		RendererContext() = default;
		virtual ~RendererContext() = default;

		static Ref<RendererContext> Create();
	};
}
