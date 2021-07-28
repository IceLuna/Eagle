#pragma once

#include "Eagle/Renderer/Texture.h"
#include <glad/glad.h>

namespace Eagle
{
	class OpenGLTexture2D : public Texture2D
	{
	public:
		OpenGLTexture2D(uint32_t width, uint32_t height);
		OpenGLTexture2D(uint32_t width, uint32_t height, const void* data);
		OpenGLTexture2D(const std::filesystem::path& path, bool bLoadAsSRGB);
		virtual ~OpenGLTexture2D();

		virtual uint32_t GetWidth() const override { return m_Width; }
		virtual uint32_t GetHeight() const override { return m_Height; }
		virtual glm::vec2 GetSize() const override { return {m_Width, m_Height}; }
		virtual uint32_t GetRendererID() const override { return m_RendererID; }
		virtual uint32_t GetNonSRGBRendererID() const override { return m_NonSRGBRendererID; }
		virtual uint32_t GetDataFormat() const override { return m_DataFormat; }
		virtual uint32_t GetInternalFormat() const override { return m_InternalFormat; }
		virtual const void* GetData() const override { return m_Data; }
		virtual uint32_t GetChannels() const override { return m_Channels; }

		virtual void Bind(uint32_t slot = 0) const override;

		virtual void SetData(const void* data) override;
		virtual void SetSRGB(bool bSRGB) override;

		virtual bool operator== (const Texture& other) const override;

	private:
		void Reload(bool bFirstInitialization);
		void FreeMemory();

	private:
		const void* m_Data;
		uint32_t m_Width, m_Height, m_Channels;
		uint32_t m_RendererID;
		uint32_t m_NonSRGBRendererID;

		GLenum m_InternalFormat;
		GLenum m_DataFormat;
		bool bLoadedTexture;
	};
}