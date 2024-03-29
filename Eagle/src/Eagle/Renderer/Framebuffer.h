#pragma once

#include "Eagle/Core/Core.h"

namespace Eagle
{
	enum class FramebufferTextureFormat
	{
		None,

		//Colors
		RGB16F,
		RGB32F,
		RGBA16F,
		RGBA32F,
		RGBA8,
		RED_INTEGER,

		//Depth
		DEPTH16,
		DEPTH24,
		DEPTH32F,
		DEPTH24STENCIL8
	};

	struct FramebufferTextureSpecification
	{
		FramebufferTextureFormat TextureFormat = FramebufferTextureFormat::None;
		bool bCubeMap = false;
		//TODO: Add Filters & Wraps
	};

	struct FramebufferSpecification
	{
		std::vector<FramebufferTextureSpecification> Attachments;
		uint32_t Width = 0, Height = 0;
		uint32_t Samples = 1;
		bool SwapChainTarget = false;
	};

	class Framebuffer
	{
	public:
		virtual ~Framebuffer() = default;

		virtual void Bind() = 0;
		virtual void BindColorTexture(uint32_t slot, uint32_t index) = 0;
		virtual void BindDepthTexture(uint32_t slot, uint32_t index) = 0;
		virtual void Unbind() = 0;

		virtual void Resize(uint32_t width, uint32_t height) = 0;
		virtual int ReadPixel(uint32_t attachmentIndex, int x, int y) const = 0;

		virtual void ClearColorAttachment(uint32_t attachmentIndex, int value) = 0;
		virtual void CopyDepthBufferFrom(const Ref<Framebuffer>& source) = 0;
		virtual uint32_t GetRendererID() const = 0;

		virtual const FramebufferSpecification& GetSpecification() const = 0;

		virtual uint32_t GetColorAttachment(uint32_t index = 0) const = 0;
		virtual uint32_t GetDepthAttachment(uint32_t index = 0) const = 0;

		static Ref<Framebuffer> Create(const FramebufferSpecification& spec);
	};
}