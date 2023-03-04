#pragma once

#include <glm/glm.hpp>
#include "VidWrappers/Texture.h"
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

		void SetAlbedoTexture(const Ref<Texture2D>& texture) { m_AlbedoTexture = texture; }
		void SetMetallnessTexture(const Ref<Texture2D>& texture) { m_MetallnessTexture = texture; }
		void SetNormalTexture(const Ref<Texture2D>& texture) { m_NormalTexture = texture; }
		void SetRoughnessTexture(const Ref<Texture2D>& texture) { m_RoughnessTexture = texture; }
		void SetAOTexture(const Ref<Texture2D>& texture) { m_AOTexture = texture; }

		const Ref<Texture2D>& GetAlbedoTexture() const { return m_AlbedoTexture; }
		const Ref<Texture2D>& GetMetallnessTexture() const { return m_MetallnessTexture; }
		const Ref<Texture2D>& GetNormalTexture() const { return m_NormalTexture; }
		const Ref<Texture2D>& GetRoughnessTexture() const { return m_RoughnessTexture; }
		const Ref<Texture2D>& GetAOTexture() const { return m_AOTexture; }

		static Ref<Material> Create() { return MakeRef<Material>(); }
		static Ref<Material> Create(const Ref<Material>& other) { return MakeRef<Material>(other); }

	private:
		Ref<Texture2D> m_AlbedoTexture;
		Ref<Texture2D> m_NormalTexture;
		Ref<Texture2D> m_MetallnessTexture;
		Ref<Texture2D> m_RoughnessTexture;
		Ref<Texture2D> m_AOTexture;

	public:
		glm::vec4 TintColor = glm::vec4(1.0);
		float TilingFactor = 1.f;
	};
}
