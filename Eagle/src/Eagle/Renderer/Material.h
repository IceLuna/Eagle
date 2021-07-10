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
		Ref<Texture> NormalTexture;

		glm::vec4 TintColor = glm::vec4(1.0);
		float TilingFactor = 1.f;
		float Shininess = 32.f;
	};
}
