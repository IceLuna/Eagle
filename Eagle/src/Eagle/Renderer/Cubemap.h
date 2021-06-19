#pragma once

#include "Texture.h"
#include "Eagle/Core/Core.h"

namespace Eagle
{
	class Cubemap
	{
	public:
		Cubemap(const std::array<Ref<Texture>, 6> textures) : m_Textures(textures) {}
		virtual ~Cubemap() = default;

		virtual void Bind(uint32_t slot = 0) const = 0;

		virtual uint32_t GetRendererID() const = 0;

		virtual const Ref<Texture>& GetRightTexture()	const = 0;
		virtual const Ref<Texture>& GetLeftTexture()	const = 0;
		virtual const Ref<Texture>& GetTopTexture()		const = 0;
		virtual const Ref<Texture>& GetBottomTexture()	const = 0;
		virtual const Ref<Texture>& GetFrontTexture()	const = 0;
		virtual const Ref<Texture>& GetBackTexture()	const = 0;

		const std::array<Ref<Texture>, 6>& GetTextures() const { return m_Textures; }

		static Ref<Cubemap> Create(const std::array<Ref<Texture>, 6> textures);

	protected:
		std::array<Ref<Texture>, 6> m_Textures;
	};
}
