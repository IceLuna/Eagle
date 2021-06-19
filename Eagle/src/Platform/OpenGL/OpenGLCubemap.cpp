#include "egpch.h"
#include "OpenGLCubemap.h"

#include <glad/glad.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace Eagle
{
	OpenGLCubemap::OpenGLCubemap(const std::array<Ref<Texture>, 6> textures) : Cubemap(textures)
	{
		glGenTextures(1, &m_RendererID);
		glBindTexture(GL_TEXTURE_CUBE_MAP, m_RendererID);

		int i = 0;
		for (auto& texture : textures)
		{
			uint32_t width = texture->GetWidth();
			uint32_t height = texture->GetHeight();
			uint32_t channels = texture->GetChannels();
			const void* data = texture->GetData();
			uint32_t size = width * height * channels;

			//Copying texture and flipping it because cubemaps require not flipped textures.
			uint8_t* tempData = new uint8_t[size];
			memcpy_s(tempData, size, data, size);
			stbi__vertical_flip(tempData, width, height, channels * sizeof(stbi_uc));

			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, texture->GetInternalFormat(), width, height, 
						0, texture->GetDataFormat(), GL_UNSIGNED_BYTE, tempData);
			
			delete[] tempData;
			++i;
		}

		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	}

	OpenGLCubemap::~OpenGLCubemap()
	{
		glDeleteTextures(1, &m_RendererID);
	}

	void OpenGLCubemap::Bind(uint32_t slot) const
	{
		glBindTextureUnit(slot, m_RendererID);
	}
}
