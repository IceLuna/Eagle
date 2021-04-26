#pragma once

#include <glm/glm.hpp>
#include "Texture.h"

namespace Eagle
{
	class Material
	{
	public:
		Material()
		{
			Ref<Texture> texture;
			if(TextureLibrary::Get("assets/textures/missingtexture.png", &texture) == false)
			{
				texture = Texture2D::Create("assets/textures/missingtexture.png");
			}
			DiffuseTexture = texture;
			SpecularTexture = Texture2D::BlackTexture;
		}

	public:
		Ref<Texture> DiffuseTexture;
		Ref<Texture> SpecularTexture;

		float TilingFactor = 1.f;
		float Shininess = 32.f;
	};
}
