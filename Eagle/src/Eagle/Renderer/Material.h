#pragma once

#include <glm/glm.hpp>
#include "Texture.h"
#include "Shader.h"

namespace Eagle
{
	class Material
	{
	public:
		Material() = default;
		Material(const Material&) = default;
		Material(Material&&) = default;

		Material(const Ref<Material>& other)
		: Shader(other->Shader)
		, DiffuseTexture(other->DiffuseTexture)
		, SpecularTexture(other->SpecularTexture)
		, NormalTexture(other->NormalTexture)
		, TintColor(other->TintColor)
		, TilingFactor(other->TilingFactor)
		, Shininess(other->Shininess)
		{}

		Material& operator= (const Material&) = default;
		Material& operator= (Material&&) = default;

		static Ref<Material> Create() { return MakeRef<Material>(); }
		static Ref<Material> Create(const Ref<Material>& other) { return MakeRef<Material>(other); }

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
