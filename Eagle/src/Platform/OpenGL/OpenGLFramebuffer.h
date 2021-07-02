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
		virtual int ReadPixel(uint32_t attachmentIndex, int x, int y) const override;

		virtual void ClearColorAttachment(uint32_t attachmentIndex, int value) override;

		virtual const FramebufferSpecification& GetSpecification() const override { return m_Specification; }

		virtual uint32_t GetColorAttachment(uint32_t index = 0) const override { EG_CORE_ASSERT(index < m_ColorAttachments.size(), "Invalid Index"); return m_ColorAttachments[index]; }
		virtual uint32_t GetDepthAttachment(uint32_t index = 0) const override { EG_CORE_ASSERT(index < m_DepthAttachments.size(), "Invalid Index"); return m_DepthAttachments[index]; }

	private:
		void Invalidate();
		void FreeMemory();

	private:
		FramebufferSpecification m_Specification;
		uint32_t m_RendererID;

		std::vector<FramebufferTextureSpecification> m_ColorAttachmentSpecifications;
		std::vector <FramebufferTextureSpecification> m_DepthAttachmentSpecifications;

		std::vector<uint32_t> m_ColorAttachments;
		std::vector <uint32_t> m_DepthAttachments;
	};
}
