#include "egpch.h"
#include "SubTexture2D.h"

namespace Eagle
{
	SubTexture2D::SubTexture2D(const Ref<Texture2D>& texture, const glm::vec2& min, const glm::vec2& max)
	: m_Texture(texture)
	{
		m_TextureCoords[0] = {min.x, max.y};
		m_TextureCoords[1] = {max.x, max.y};
		m_TextureCoords[2] = {max.x, min.y};
		m_TextureCoords[3] = {min.x, min.y};
	}

	Ref<SubTexture2D> SubTexture2D::CreateFromCoords(const Ref<Texture2D>& texture, const glm::vec2& coords, const glm::vec2& cellSize, const glm::vec2& spriteSize)
	{
		if (!texture)
			return MakeRef<SubTexture2D>(texture, glm::vec2{0.f}, glm::vec2{1.f});

		const float textureWidth =  (float)texture->GetWidth();
		const float textureHeight = (float)texture->GetHeight();

		glm::vec2 min = { (coords.x * cellSize.x) / textureWidth, (coords.y * cellSize.y) / textureHeight };
		glm::vec2 max = { ((coords.x + spriteSize.x) * cellSize.x) / textureWidth, ((coords.y + spriteSize.y) * cellSize.y) / textureHeight };

		return MakeRef<SubTexture2D>(texture, min, max);
	}
	
	void SubTexture2D::UpdateCoords(const glm::vec2& coords, const glm::vec2& cellSize, const glm::vec2& spriteSize)
	{
		if (!m_Texture)
			return;

		const float textureWidth = (float)m_Texture->GetWidth();
		const float textureHeight = (float)m_Texture->GetHeight();

		const glm::vec2 min = { (coords.x * cellSize.x) / textureWidth, (coords.y * cellSize.y) / textureHeight };
		const glm::vec2 max = { ((coords.x + spriteSize.x) * cellSize.x) / textureWidth, ((coords.y + spriteSize.y) * cellSize.y) / textureHeight };

		m_TextureCoords[0] = { min.x, max.y };
		m_TextureCoords[1] = { max.x, max.y };
		m_TextureCoords[2] = { max.x, min.y };
		m_TextureCoords[3] = { min.x, min.y };
	}
}
