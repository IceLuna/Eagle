#pragma once

#include "Eagle/Renderer/Texture.h"
#include <glad/glad.h>

namespace Eagle
{
	class OpenGLTexture2D : public Texture2D
	{
	public:
		OpenGLTexture2D(uint32_t width, uint32_t height);
		OpenGLTexture2D(const std::string& path);
		virtual ~OpenGLTexture2D();

		virtual uint32_t GetWidth() const override { return m_Width; };
		virtual uint32_t GetHeight() const override { return m_Height; };

		virtual void Bind(uint32_t slot = 0) const override;

		virtual void SetData(const void* data) const override;

		virtual bool operator== (const Texture& other) const override;

	private:
		std::string m_Path;
		uint32_t m_Width, m_Height, m_Channels;
		uint32_t m_RendererID;

		GLenum m_InternalFormat;
		GLenum m_DataFormat;
	};
}