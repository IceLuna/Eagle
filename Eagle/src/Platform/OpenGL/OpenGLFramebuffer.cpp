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

		static void CreateTextures(bool multisampled, const std::vector<FramebufferTextureSpecification>& specs, std::vector<uint32_t>* outTextures)
		{
			std::vector<uint32_t>& textures = *outTextures; 
			for (size_t i = 0; i < textures.size(); ++i)
			{
				if (specs[i].bCubeMap)
				{
					glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &textures[i]);
				}
				else
				{
					glCreateTextures(TextureTarget(multisampled), 1, &textures[i]);
				}
			}
		}

		static void AttachColorTexture(uint32_t id, uint32_t samples, GLenum internalFormat, GLenum format, GLenum type, uint32_t width, uint32_t height, int index)
		{
			bool multisampled = samples > 1;

			if (multisampled)
			{
				glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, samples, internalFormat, width, height, GL_FALSE);
			}
			else
			{
				glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, type, nullptr);
				
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			}

			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + index, TextureTarget(multisampled), id, 0);
		}

		static void AttachDepthTexture(uint32_t id, uint32_t samples, GLenum internalFormat, GLenum attachmentType, uint32_t width, uint32_t height)
		{
			bool multisampled = samples > 1;

			if (multisampled)
			{
				glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, samples, internalFormat, width, height, GL_FALSE);
			}
			else
			{
				float borderColor[] = { 1.f, 1.f, 1.f, 1.f };
				glTexStorage2D(GL_TEXTURE_2D, 1, internalFormat, width, height);

				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
				glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
			}

			glFramebufferTexture2D(GL_FRAMEBUFFER, attachmentType, TextureTarget(multisampled), id, 0);
		}

		static void AttachDepthCubeMap(uint32_t id, GLenum internalFormat, GLenum attachmentType, uint32_t width, uint32_t height)
		{
			for (int i = 0; i < 6; ++i)
			{
				glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, internalFormat, width, height,
					0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
			}

			glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

			glFramebufferTexture(GL_FRAMEBUFFER, attachmentType, id, 0);
		}

		static void BindTexture(bool multisampled, uint32_t id, bool bCubemap)
		{
			if (bCubemap)
				glBindTexture(GL_TEXTURE_CUBE_MAP, id);
			else
				glBindTexture(TextureTarget(multisampled), id);
		}

		static bool IsDepthAttachment(FramebufferTextureFormat format)
		{
			switch (format)
			{
				case FramebufferTextureFormat::DEPTH24STENCIL8:
				case FramebufferTextureFormat::DEPTH16:
				case FramebufferTextureFormat::DEPTH24:
				case FramebufferTextureFormat::DEPTH32F: return true;
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
				m_DepthAttachmentSpecifications.emplace_back(attachment);
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
			Utils::CreateTextures(multisampled, m_ColorAttachmentSpecifications, &m_ColorAttachments);

			for (uint32_t i = 0; i < m_ColorAttachments.size(); ++i)
			{
				Utils::BindTexture(multisampled, m_ColorAttachments[i], m_ColorAttachmentSpecifications[i].bCubeMap);

				auto& attachmentSpec = m_ColorAttachmentSpecifications[i];
				switch (attachmentSpec.TextureFormat)
				{
					case FramebufferTextureFormat::RGB16F:
						Utils::AttachColorTexture(m_ColorAttachments[i], m_Specification.Samples, GL_RGB16F, GL_RGB, GL_FLOAT,
							m_Specification.Width, m_Specification.Height, i);
						break;
					case FramebufferTextureFormat::RGBA8: 
						Utils::AttachColorTexture(m_ColorAttachments[i], m_Specification.Samples, GL_RGB16F, GL_RGB, GL_UNSIGNED_BYTE,
													m_Specification.Width, m_Specification.Height, i);
						break;
					case FramebufferTextureFormat::RED_INTEGER:
						Utils::AttachColorTexture(m_ColorAttachments[i], m_Specification.Samples, GL_R32I, GL_RED_INTEGER, GL_UNSIGNED_BYTE,
							m_Specification.Width, m_Specification.Height, i);
						break;
				}
			}
		}

		//Depth Attachment
		if (m_DepthAttachmentSpecifications.size())
		{
			m_DepthAttachments.resize(m_DepthAttachmentSpecifications.size());
			Utils::CreateTextures(multisampled, m_DepthAttachmentSpecifications, &m_DepthAttachments);

			for (uint32_t i = 0; i < m_DepthAttachments.size(); ++i)
			{
				Utils::BindTexture(multisampled, m_DepthAttachments[i], m_DepthAttachmentSpecifications [i].bCubeMap);

				auto& attachmentSpec = m_DepthAttachmentSpecifications[i];
				if (m_DepthAttachmentSpecifications[i].bCubeMap)
				{
					switch (attachmentSpec.TextureFormat)
					{
					case FramebufferTextureFormat::DEPTH24STENCIL8:
						Utils::AttachDepthCubeMap(m_DepthAttachments[i], GL_DEPTH24_STENCIL8,
							GL_DEPTH_STENCIL_ATTACHMENT, m_Specification.Width, m_Specification.Height);
						break;
					case FramebufferTextureFormat::DEPTH16:
						Utils::AttachDepthCubeMap(m_DepthAttachments[i], GL_DEPTH_COMPONENT16,
							GL_DEPTH_ATTACHMENT, m_Specification.Width, m_Specification.Height);
						break;
					case FramebufferTextureFormat::DEPTH24:
						Utils::AttachDepthCubeMap(m_DepthAttachments[i], GL_DEPTH_COMPONENT24,
							GL_DEPTH_ATTACHMENT, m_Specification.Width, m_Specification.Height);
						break;
					case FramebufferTextureFormat::DEPTH32F:
						Utils::AttachDepthCubeMap(m_DepthAttachments[i], GL_DEPTH_COMPONENT32F,
							GL_DEPTH_ATTACHMENT, m_Specification.Width, m_Specification.Height);
						break;
					}
				}
				else
				{
					switch (attachmentSpec.TextureFormat)
					{
					case FramebufferTextureFormat::DEPTH24STENCIL8:
						Utils::AttachDepthTexture(m_DepthAttachments[i], m_Specification.Samples, GL_DEPTH24_STENCIL8,
							GL_DEPTH_STENCIL_ATTACHMENT, m_Specification.Width, m_Specification.Height);
						break;
					case FramebufferTextureFormat::DEPTH16:
						Utils::AttachDepthTexture(m_DepthAttachments[i], m_Specification.Samples, GL_DEPTH_COMPONENT16,
							GL_DEPTH_ATTACHMENT, m_Specification.Width, m_Specification.Height);
						break;
					case FramebufferTextureFormat::DEPTH24:
						Utils::AttachDepthTexture(m_DepthAttachments[i], m_Specification.Samples, GL_DEPTH_COMPONENT24,
							GL_DEPTH_ATTACHMENT, m_Specification.Width, m_Specification.Height);
						break;
					case FramebufferTextureFormat::DEPTH32F:
						Utils::AttachDepthTexture(m_DepthAttachments[i], m_Specification.Samples, GL_DEPTH_COMPONENT32F,
							GL_DEPTH_ATTACHMENT, m_Specification.Width, m_Specification.Height);
						break;
					}
				}
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
			glReadBuffer(GL_NONE);
		}

		EG_CORE_ASSERT((glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE), "Framebuffer is incomplete!");

		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		wasInitialized = true;
	}

	void OpenGLFramebuffer::Bind()
	{
		glBindFramebuffer(GL_FRAMEBUFFER, m_RendererID);
	}

	void OpenGLFramebuffer::BindColorTexture(uint32_t slot, uint32_t index)
	{
		glBindTextureUnit(slot, m_ColorAttachments[index]);
	}

	void OpenGLFramebuffer::BindDepthTexture(uint32_t slot, uint32_t index)
	{
		glBindTextureUnit(slot, m_DepthAttachments[index]);
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
		glDeleteTextures((GLsizei)m_DepthAttachments.size(), m_DepthAttachments.data());

		m_ColorAttachments.clear();
		m_DepthAttachments.clear();
	}
}