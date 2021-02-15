#include "egpch.h"
#include "OpenGLFramebuffer.h"

#include <glad/glad.h>

namespace Eagle
{
	namespace Utils
	{
		static GLenum TextureTarget(bool multisample)
		{
			return multisample ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
		}

		static void CreateTextures(bool multisampled, uint32_t* outID, size_t count)
		{
			glCreateTextures(TextureTarget(multisampled), (GLsizei)count, outID);
		}

		static void AttachColorTexture(uint32_t id, uint32_t samples, GLenum internalFormat, GLenum format, uint32_t width, uint32_t height, int index)
		{
			bool multisampled = samples > 1;

			if (multisampled)
			{
				glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, samples, internalFormat, width, height, GL_FALSE);
			}
			else
			{
				glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, GL_UNSIGNED_BYTE, nullptr);
				
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			}

			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + index, TextureTarget(multisampled), id, 0);
		}

		static void AttachDepthTexture(uint32_t id, uint32_t samples, GLenum format, GLenum attachmentType, uint32_t width, uint32_t height)
		{
			bool multisampled = samples > 1;

			if (multisampled)
			{
				glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, samples, format, width, height, GL_FALSE);
			}
			else
			{
				glTexStorage2D(GL_TEXTURE_2D, 1, format, width, height);

				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			}

			glFramebufferTexture2D(GL_FRAMEBUFFER, attachmentType, TextureTarget(multisampled), id, 0);
		}
		
		static void BindTexture(bool multisampled, uint32_t id)
		{
			glBindTexture(TextureTarget(multisampled), id);
		}

		static bool IsDepthAttachment(FramebufferTextureFormat format)
		{
			switch (format)
			{
				case FramebufferTextureFormat::DEPTH24STENCIL8: return true;
			}
			return false;
		}
	}

	OpenGLFramebuffer::OpenGLFramebuffer(const FramebufferSpecification& spec)
	: m_Specification(spec)
	{
		for (auto& attachment : spec.Attachments)
		{
			if (Utils::IsDepthAttachment(attachment.TextureFormat))
			{
				m_DepthAttachmentSpecification = attachment;
			}
			else
			{
				m_ColorAttachmentSpecifications.emplace_back(attachment);
			}
		}

		Invalidate();
	}

	OpenGLFramebuffer::~OpenGLFramebuffer()
	{
		FreeMemory();
	}

	void OpenGLFramebuffer::Invalidate()
	{
		static bool wasInitialized = false;

		if (wasInitialized)
		{
			FreeMemory();
		}

		bool multisampled = m_Specification.Samples > 1;

		glCreateFramebuffers(1, &m_RendererID);
		glBindFramebuffer(GL_FRAMEBUFFER, m_RendererID);

		//Color Attachments
		if (m_ColorAttachmentSpecifications.size())
		{
			m_ColorAttachments.resize(m_ColorAttachmentSpecifications.size());
			Utils::CreateTextures(multisampled, m_ColorAttachments.data(), m_ColorAttachments.size());

			for (uint32_t i = 0; i < m_ColorAttachments.size(); ++i)
			{
				Utils::BindTexture(multisampled, m_ColorAttachments[i]);

				auto& attachmentSpec = m_ColorAttachmentSpecifications[i];
				switch (attachmentSpec.TextureFormat)
				{
					case FramebufferTextureFormat::RGBA8: 
						Utils::AttachColorTexture(m_ColorAttachments[i], m_Specification.Samples, GL_RGBA8, GL_RGBA,
													m_Specification.Width, m_Specification.Height, i);
						break;
					case FramebufferTextureFormat::RED_INTEGER:
						Utils::AttachColorTexture(m_ColorAttachments[i], m_Specification.Samples, GL_R32I, GL_RED_INTEGER,
							m_Specification.Width, m_Specification.Height, i);
						break;
				}
			}
		}

		//Depth Attachment
		if (m_DepthAttachmentSpecification.TextureFormat != FramebufferTextureFormat::None)
		{
			Utils::CreateTextures(multisampled, &m_DepthAttachment, 1);
			Utils::BindTexture(multisampled, m_DepthAttachment);

			switch (m_DepthAttachmentSpecification.TextureFormat)
			{
				case FramebufferTextureFormat::DEPTH24STENCIL8:
					Utils::AttachDepthTexture(m_DepthAttachment, m_Specification.Samples, GL_DEPTH24_STENCIL8, 
												GL_DEPTH_STENCIL_ATTACHMENT, m_Specification.Width, m_Specification.Height);
					break;
			}
		}

		if (m_ColorAttachments.size() > 1)
		{
			EG_CORE_ASSERT(m_ColorAttachments.size() <= 4, "Eagle supports only 4 RT for now");
			GLenum buffers[4] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3 };
			glDrawBuffers((GLsizei)m_ColorAttachments.size(), buffers);
		}
		else if (m_ColorAttachments.empty())
		{
			glDrawBuffer(GL_NONE);
		}

		EG_CORE_ASSERT((glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE), "Framebuffer is incomplete!");

		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		wasInitialized = true;
	}

	void OpenGLFramebuffer::Bind()
	{
		glBindFramebuffer(GL_FRAMEBUFFER, m_RendererID);
	}

	void OpenGLFramebuffer::Unbind()
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void OpenGLFramebuffer::Resize(uint32_t width, uint32_t height)
	{
		m_Specification.Width = width;
		m_Specification.Height = height;

		Invalidate();
	}

	int OpenGLFramebuffer::ReadPixel(uint32_t attachmentIndex, int x, int y) const
	{
		EG_CORE_ASSERT(attachmentIndex < m_ColorAttachments.size(), "Invalid Index");

		glReadBuffer(GL_COLOR_ATTACHMENT0 + attachmentIndex);
		int pixelData = -1;
		glReadPixels(x, y, 1, 1, GL_RED_INTEGER, GL_INT, &pixelData);
		return pixelData;
	}

	void OpenGLFramebuffer::ClearColorAttachment(uint32_t attachmentIndex, int value)
	{
		EG_CORE_ASSERT(attachmentIndex < m_ColorAttachments.size(), "Invalid Index");

		switch (m_ColorAttachmentSpecifications[attachmentIndex].TextureFormat)
		{
			case FramebufferTextureFormat::RED_INTEGER:
				glClearTexImage(m_ColorAttachments[attachmentIndex], 0, GL_RED_INTEGER, GL_INT, &value);
				break;

			case FramebufferTextureFormat::RGBA8:
				glClearTexImage(m_ColorAttachments[attachmentIndex], 0, GL_RGBA, GL_INT, &value);
				break;
		}
	}
	
	void OpenGLFramebuffer::FreeMemory()
	{
		glDeleteFramebuffers(1, &m_RendererID);
		glDeleteTextures((GLsizei)m_ColorAttachments.size(), m_ColorAttachments.data());
		glDeleteTextures(1, &m_DepthAttachment);

		m_ColorAttachments.clear();
		m_DepthAttachment = 0;
	}
}