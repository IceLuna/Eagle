#pragma once

#include "Eagle/Core/Core.h"

namespace Eagle
{
	struct FramebufferSpecification
	{
		uint32_t Width, Height;
		uint32_t Samples = 1;

		bool SwapChainTarget = false;
	};

	class Framebuffer
	{
	public:
		virtual ~Framebuffer() = default;

		virtual void Bind() = 0;
		virtual void Unbind() = 0;

		virtual void Invalidate() = 0;

		virtual FramebufferSpecification& GetSpecification() = 0;
		virtual const FramebufferSpecification& GetSpecification() const = 0;

		virtual uint32_t GetColorAttachment() const = 0;
		virtual uint32_t GetDepthAttachment() const = 0;

		static Ref<Framebuffer> Create(const FramebufferSpecification& spec);
	};
}