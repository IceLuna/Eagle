#pragma once

#include <glm/glm.hpp>
#include "Texture.h"
#include "Shader.h"
#include "DescriptorManager.h"
#include "../../Eagle-Editor/assets/shaders/defines.h"

namespace Eagle
{
	class Material
	{
	public:
		Material() = default;
		Material(const Material& other) = default;
		Material(const Ref<Material>& other);
		Material(Material&&) = default;

		Material& operator= (const Material&) = default;
		Material& operator= (Material&&) = default;

		void SetDiffuseTexture(const Ref<Texture2D>& texture) { m_DiffuseTexture = texture; }
		void SetSpecularTexture(const Ref<Texture2D>& texture) { m_SpecularTexture = texture; }
		void SetNormalTexture(const Ref<Texture2D>& texture) { m_NormalTexture = texture; }

		const Ref<Texture2D>& GetDiffuseTexture() const { return m_DiffuseTexture; }
		const Ref<Texture2D>& GetSpecularTexture() const { return m_SpecularTexture; }
		const Ref<Texture2D>& GetNormalTexture() const { return m_NormalTexture; }

		static Ref<Material> Create() { return MakeRef<Material>(); }
		static Ref<Material> Create(const Ref<Material>& other) { return MakeRef<Material>(other); }

	private:
		Ref<Texture2D> m_DiffuseTexture;
		Ref<Texture2D> m_SpecularTexture;
		Ref<Texture2D> m_NormalTexture;

	public:
		glm::vec4 TintColor = glm::vec4(1.0);
		float TilingFactor = 1.f;
		float Shininess = 32.f;

		static constexpr uint32_t s_DiffuseBindIdx = 0;
		static constexpr uint32_t s_SpecularBindIdx = 1;
		static constexpr uint32_t s_NormalBindIdx = 2;
	};
}
