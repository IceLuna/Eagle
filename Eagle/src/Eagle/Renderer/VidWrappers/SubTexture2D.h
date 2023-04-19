#pragma once

#include "Texture.h"

namespace Eagle
{
	class SubTexture2D
	{
	public:
		SubTexture2D(const Ref<Texture2D>& texture, const glm::vec2& min, const glm::vec2& max);

		static Ref<SubTexture2D> CreateFromCoords(const Ref<Texture2D>& texture, const glm::vec2& coords, const glm::vec2& cellSize, const glm::vec2& spriteSize = {1, 1});

		void UpdateCoords(const glm::vec2& coords, const glm::vec2& cellSize, const glm::vec2& spriteSize = { 1, 1 });

		inline const Ref<Texture2D>& GetTexture() const { return m_Texture; }
		inline const glm::vec2* GetTexCoords() const { return m_TextureCoords; }

	private:
		Ref<Texture2D> m_Texture;
		glm::vec2 m_TextureCoords[4];
	};
}