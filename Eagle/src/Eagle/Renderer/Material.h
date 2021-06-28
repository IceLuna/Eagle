#pragma once

#include <glm/glm.hpp>
#include "Texture.h"
#include "Shader.h"

namespace Eagle
{
	class Material
	{
	public:
		Material()
		{
			Ref<Texture> texture;
			if(TextureLibrary::Get("assets/textures/Editor/missingtexture.png", &texture) == false)
			{
				texture = Texture2D::Create("assets/textures/Editor/missingtexture.png");
			}
			DiffuseTexture = texture;
			SpecularTexture = Texture2D::BlackTexture;
		}

		static Ref<Material> Create() { return MakeRef<Material>(); }

		Material(const Material&) = default;
		Material(Material&&) = default;

		Material& operator= (const Material&) = default;
		Material& operator= (Material&&) = default;

	public:
		Ref<Shader> Shader;
		Ref<Texture> DiffuseTexture;
		Ref<Texture> SpecularTexture;

		float TilingFactor = 1.f;
		float Shininess = 32.f;
	};
}
