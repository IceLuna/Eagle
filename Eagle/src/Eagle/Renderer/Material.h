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
			SpecularTexture = texture;
		}

	public:
		float Shininess = 32.f;

		Ref<Texture> DiffuseTexture;
		Ref<Texture> SpecularTexture;
	};
}
