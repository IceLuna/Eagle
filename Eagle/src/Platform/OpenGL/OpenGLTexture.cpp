#include "egpch.h"
#include "OpenGLTexture.h"

#include <stb_image.h>
#include "Eagle/Utils/Utils.h"

namespace Eagle
{
	OpenGLTexture2D::OpenGLTexture2D(uint32_t width, uint32_t height)
	: m_Width(width), m_Height(height), m_Channels(0u), m_RendererID(0u), m_InternalFormat(GL_RGBA8), m_DataFormat(GL_RGBA)
	{
		glCreateTextures(GL_TEXTURE_2D, 1, &m_RendererID);
		m_NonSRGBRendererID = m_RendererID;
		glTextureStorage2D(m_RendererID, 1, m_InternalFormat, m_Width, m_Height);

		glTextureParameteri(m_RendererID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTextureParameteri(m_RendererID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_T, GL_REPEAT);
	}

	OpenGLTexture2D::OpenGLTexture2D(uint32_t width, uint32_t height, const void* data) : OpenGLTexture2D(width, height)
	{
		SetData(data);
	}

	OpenGLTexture2D::OpenGLTexture2D(const std::filesystem::path& path, bool bLoadAsSRGB)
	:	Texture2D(path, bLoadAsSRGB), m_Width(0u), m_Height(0u), m_Channels(0u), m_RendererID(0u), m_InternalFormat(0), m_DataFormat(0)
	{ 
		Reload(true);
	}

	OpenGLTexture2D::~OpenGLTexture2D()
	{
		if (bLoadedTexture)
			stbi_image_free((void*)m_Data);
		FreeMemory();
	}

	void OpenGLTexture2D::Bind(uint32_t slot) const
	{
		glBindTextureUnit(slot, m_RendererID);
	}

	void OpenGLTexture2D::SetData(const void* data)
	{
		bLoadedTexture = false;
		m_Data = data;
		glTextureSubImage2D(m_RendererID, 0, 0, 0, m_Width, m_Height, m_DataFormat, GL_UNSIGNED_BYTE, data);
	}

	void OpenGLTexture2D::SetSRGB(bool bSRGB)
	{
		if (bSRGB != m_IsSRGB)
		{
			m_IsSRGB = bSRGB;
			Reload(false);
		}
	}

	bool OpenGLTexture2D::operator==(const Texture& other) const
	{
		return (m_RendererID == ((OpenGLTexture2D&)other).m_RendererID);
	}
	
	void OpenGLTexture2D::Reload(bool bFirstInitialization)
	{
		if (bFirstInitialization)
		{
			int width, height, channels;

			stbi_set_flip_vertically_on_load(1);
			std::wstring pathString = m_Path.wstring();

			char cpath[2048];
			WideCharToMultiByte(65001 /* UTF8 */, 0, pathString.c_str(), -1, cpath, 2048, NULL, NULL);
			stbi_uc* data = stbi_load(cpath, &width, &height, &channels, 0);

			m_Width = width;
			m_Height = height;
			m_Channels = channels;

			if (!data)
			{
				EG_CORE_ERROR("Could not load the texture : {0}", m_Path);
			}
			EG_CORE_ASSERT(data, "Could not load the texture: {0}", m_Path);

			GLenum internalFormat = 0, dataFormat = 0;

			if (channels == 4)
			{
				internalFormat = m_IsSRGB ? GL_SRGB8_ALPHA8 : GL_RGBA8;
				dataFormat = GL_RGBA;
			}
			else if (channels == 3)
			{
				internalFormat = m_IsSRGB ? GL_SRGB8 : GL_RGB8;
				dataFormat = GL_RGB;
			}
			else
			{
				EG_CORE_ASSERT(false, "Unsupported texture format!");
			}

			m_InternalFormat = internalFormat;
			m_DataFormat = dataFormat;

			glCreateTextures(GL_TEXTURE_2D, 1, &m_RendererID);
			glTextureStorage2D(m_RendererID, 1, internalFormat, m_Width, m_Height);

			glTextureParameteri(m_RendererID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTextureParameteri(m_RendererID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

			glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_T, GL_REPEAT);

			glTextureSubImage2D(m_RendererID, 0, 0, 0, m_Width, m_Height, dataFormat, GL_UNSIGNED_BYTE, data);

			#if EG_EDITOR
			{
				if (m_IsSRGB)
				{
					GLenum nonSRGBInternalFormat = 0;
					if (channels == 4)
						nonSRGBInternalFormat = GL_RGBA8;
					else if (channels == 3)
						nonSRGBInternalFormat = GL_RGB8;
					glCreateTextures(GL_TEXTURE_2D, 1, &m_NonSRGBRendererID);
					glTextureStorage2D(m_NonSRGBRendererID, 1, nonSRGBInternalFormat, m_Width, m_Height);

					glTextureParameteri(m_NonSRGBRendererID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
					glTextureParameteri(m_NonSRGBRendererID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

					glTextureParameteri(m_NonSRGBRendererID, GL_TEXTURE_WRAP_S, GL_REPEAT);
					glTextureParameteri(m_NonSRGBRendererID, GL_TEXTURE_WRAP_T, GL_REPEAT);

					glTextureSubImage2D(m_NonSRGBRendererID, 0, 0, 0, m_Width, m_Height, dataFormat, GL_UNSIGNED_BYTE, data);
				}
				else
					m_NonSRGBRendererID = m_RendererID;
			}
			#else
				m_NonSRGBRendererID = m_RendererID;
			#endif

			m_Data = data;
			bLoadedTexture = true;
		}
		else
		{
			FreeMemory();

			if (m_Channels == 4)
			{
				m_InternalFormat = m_IsSRGB ? GL_SRGB8_ALPHA8 : GL_RGBA8;
				m_DataFormat = GL_RGBA;
			}
			else if (m_Channels == 3)
			{
				m_InternalFormat = m_IsSRGB ? GL_SRGB8 : GL_RGB8;
				m_DataFormat = GL_RGB;
			}

			glCreateTextures(GL_TEXTURE_2D, 1, &m_RendererID);
			glTextureStorage2D(m_RendererID, 1, m_InternalFormat, m_Width, m_Height);

			glTextureParameteri(m_RendererID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTextureParameteri(m_RendererID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

			glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_T, GL_REPEAT);

			glTextureSubImage2D(m_RendererID, 0, 0, 0, m_Width, m_Height, m_DataFormat, GL_UNSIGNED_BYTE, m_Data);

			#if EG_EDITOR
			{
				if (m_IsSRGB)
				{
					GLenum nonSRGBInternalFormat = 0;
					if (m_Channels == 4)
						nonSRGBInternalFormat = GL_RGBA8;
					else if (m_Channels == 3)
						nonSRGBInternalFormat = GL_RGB8;
					glCreateTextures(GL_TEXTURE_2D, 1, &m_NonSRGBRendererID);
					glTextureStorage2D(m_NonSRGBRendererID, 1, nonSRGBInternalFormat, m_Width, m_Height);

					glTextureParameteri(m_NonSRGBRendererID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
					glTextureParameteri(m_NonSRGBRendererID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

					glTextureParameteri(m_NonSRGBRendererID, GL_TEXTURE_WRAP_S, GL_REPEAT);
					glTextureParameteri(m_NonSRGBRendererID, GL_TEXTURE_WRAP_T, GL_REPEAT);

					glTextureSubImage2D(m_NonSRGBRendererID, 0, 0, 0, m_Width, m_Height, m_DataFormat, GL_UNSIGNED_BYTE, m_Data);
				}
				else
					m_NonSRGBRendererID = m_RendererID;
			}
			#else
				m_NonSRGBRendererID = m_RendererID;
			#endif
		}
	}

	void OpenGLTexture2D::FreeMemory()
	{
		glDeleteTextures(1, &m_RendererID);
		#if EG_EDITOR
			if (m_IsSRGB)
				glDeleteTextures(1, &m_NonSRGBRendererID);
		#endif
	}
}