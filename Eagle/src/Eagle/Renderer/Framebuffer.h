#pragma once

#include "Eagle/Core/Core.h"

namespace Eagle
{
	struct FramebufferSpecification
	{
		uint32_t Width, Height;
		uint32_t Samples;
		bool SwapChainTarget;

		FramebufferSpecification(uint32_t width, uint32_t height, uint32_t samples = 1, bool swapChainTarget = false)
		: Width(width), Height(height), Samples(samples), SwapChainTarget(swapChainTarget) {}
	};

	class Framebuffer
	{
	public:
		virtual ~Framebuffer() = default;

		virtual void Bind() = 0;
		virtual void Unbind() = 0;

		virtual void Resize(uint32_t width, uint32_t height) = 0;

		virtual const FramebufferSpecification& GetSpecification() const = 0;

		virtual uint32_t GetColorAttachment() const = 0;
		virtual uint32_t GetDepthAttachment() const = 0;

		static Ref<Framebuffer> Create(const FramebufferSpecification& spec);
	};
}