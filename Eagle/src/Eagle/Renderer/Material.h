#pragma once

#include <glm/glm.hpp>
#include "VidWrappers/Texture.h"
#include "../../Eagle-Editor/assets/shaders/defines.h"

namespace Eagle
{
	class Material : virtual public std::enable_shared_from_this<Material>
	{
	public:
		enum class BlendMode
		{
			Opaque, Translucent, Masked
		};

		virtual ~Material() = default;

		void SetAlbedoTexture(const Ref<Texture2D>& texture)      { if (m_AlbedoTexture == texture)      return; m_AlbedoTexture = texture;      OnMaterialChanged(); }
		void SetMetallnessTexture(const Ref<Texture2D>& texture)  { if (m_MetallnessTexture == texture)  return; m_MetallnessTexture = texture;  OnMaterialChanged(); }
		void SetNormalTexture(const Ref<Texture2D>& texture)      { if (m_NormalTexture == texture)      return; m_NormalTexture = texture;      OnMaterialChanged(); }
		void SetRoughnessTexture(const Ref<Texture2D>& texture)   { if (m_RoughnessTexture == texture)   return; m_RoughnessTexture = texture;   OnMaterialChanged(); }
		void SetAOTexture(const Ref<Texture2D>& texture)          { if (m_AOTexture == texture)          return; m_AOTexture = texture;          OnMaterialChanged(); }
		void SetEmissiveTexture(const Ref<Texture2D>& texture)    { if (m_EmissiveTexture == texture)    return; m_EmissiveTexture = texture;    OnMaterialChanged(); }
		void SetOpacityTexture(const Ref<Texture2D>& texture)     { if (m_OpacityTexture == texture)     return; m_OpacityTexture = texture;     OnMaterialChanged(); }
		void SetOpacityMaskTexture(const Ref<Texture2D>& texture) { if (m_OpacityMaskTexture == texture) return; m_OpacityMaskTexture = texture; OnMaterialChanged(); }

		void SetTintColor(const glm::vec4& tintColor)         { m_TintColor = tintColor;         OnMaterialChanged(); }
		void SetEmissiveIntensity(const glm::vec3& intensity) { m_EmissiveIntensity = intensity; OnMaterialChanged(); }
		void SetTilingFactor(float tiling)                    { m_TilingFactor = tiling;         OnMaterialChanged(); }
		void SetBlendMode(BlendMode blendMode)
		{
			if (blendMode == m_BlendMode)
				return;

			m_BlendMode = blendMode;
			OnMaterialChanged();
		}

		const Ref<Texture2D>& GetAlbedoTexture() const { return m_AlbedoTexture; }
		const Ref<Texture2D>& GetMetallnessTexture() const { return m_MetallnessTexture; }
		const Ref<Texture2D>& GetNormalTexture() const { return m_NormalTexture; }
		const Ref<Texture2D>& GetRoughnessTexture() const { return m_RoughnessTexture; }
		const Ref<Texture2D>& GetAOTexture() const { return m_AOTexture; }
		const Ref<Texture2D>& GetEmissiveTexture() const { return m_EmissiveTexture; }
		const Ref<Texture2D>& GetOpacityTexture() const { return m_OpacityTexture; }
		const Ref<Texture2D>& GetOpacityMaskTexture() const { return m_OpacityMaskTexture; }

		const glm::vec4& GetTintColor() const { return m_TintColor; }
		const glm::vec3& GetEmissiveIntensity() const { return m_EmissiveIntensity; }
		float GetTilingFactor() const { return m_TilingFactor; }
		BlendMode GetBlendMode() const { return m_BlendMode; }

		static Ref<Material> Create();
		static Ref<Material> Create(const Ref<Material>& other);

	protected:
		Material() = default;
		Material(const Ref<Material>& other);

		Material(const Material& other) = delete;
		Material(Material&&) = delete;

		Material& operator= (const Material&) = delete;
		Material& operator= (Material&&) = delete;

		void OnMaterialChanged();

	private:
		Ref<Texture2D> m_AlbedoTexture;
		Ref<Texture2D> m_NormalTexture;
		Ref<Texture2D> m_MetallnessTexture;
		Ref<Texture2D> m_RoughnessTexture;
		Ref<Texture2D> m_AOTexture;
		Ref<Texture2D> m_EmissiveTexture;
		Ref<Texture2D> m_OpacityTexture;
		Ref<Texture2D> m_OpacityMaskTexture;

		glm::vec4 m_TintColor = glm::vec4(1.0);
		glm::vec3 m_EmissiveIntensity = glm::vec3(1.f);
		float m_TilingFactor = 1.f;
		BlendMode m_BlendMode = BlendMode::Opaque;
	};
}
