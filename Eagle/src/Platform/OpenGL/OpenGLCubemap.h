#pragma once

#include "Eagle/Renderer/Cubemap.h"

namespace Eagle
{
	class OpenGLCubemap : public Cubemap
	{
	public:
		OpenGLCubemap(const std::array<Ref<Texture>, 6> textures);
		virtual ~OpenGLCubemap();

		virtual void Bind(uint32_t slot = 0) const override;

		virtual uint32_t GetRendererID() const override { return m_RendererID; }

		virtual const Ref<Texture>& GetRightTexture()	const override { return m_Textures[0]; }
		virtual const Ref<Texture>& GetLeftTexture()	const override { return m_Textures[1]; }
		virtual const Ref<Texture>& GetTopTexture()		const override { return m_Textures[2]; }
		virtual const Ref<Texture>& GetBottomTexture()	const override { return m_Textures[3]; }
		virtual const Ref<Texture>& GetFrontTexture()	const override { return m_Textures[4]; }
		virtual const Ref<Texture>& GetBackTexture()	const override { return m_Textures[5]; } 

	private:
		uint32_t m_RendererID = 0;
	};
}
