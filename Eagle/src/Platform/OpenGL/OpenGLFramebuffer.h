#pragma once

#include "Eagle/Renderer/Framebuffer.h"

namespace Eagle
{
	class OpenGLFramebuffer : public Framebuffer
	{
	public:
		OpenGLFramebuffer(const FramebufferSpecification& spec);
		virtual ~OpenGLFramebuffer();

		virtual void Bind() override;
		virtual void Unbind() override;

		virtual void Resize(uint32_t width, uint32_t height) override;

		virtual const FramebufferSpecification& GetSpecification() const override { return m_Specification; }

		virtual uint32_t GetColorAttachment() const override { return m_ColorAttachment; }
		virtual uint32_t GetDepthAttachment() const override { return m_DepthAttachment; }

	private:
		void Invalidate();
		void FreeMemory();

	private:
		FramebufferSpecification m_Specification;
		uint32_t m_RendererID;
		uint32_t m_ColorAttachment, m_DepthAttachment;
	};
}