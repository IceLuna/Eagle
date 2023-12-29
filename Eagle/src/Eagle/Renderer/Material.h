#pragma once

#include "Eagle/Asset/Asset.h"
#include "../../Eagle-Editor/assets/shaders/defines.h"

#include <glm/glm.hpp>

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

		void SetAlbedoTexture(const Ref<AssetTexture2D>& texture)      { if (m_AlbedoTexture == texture)      return; m_AlbedoTexture = texture;      OnMaterialChanged(); }
		void SetMetallnessTexture(const Ref<AssetTexture2D>& texture)  { if (m_MetallnessTexture == texture)  return; m_MetallnessTexture = texture;  OnMaterialChanged(); }
		void SetNormalTexture(const Ref<AssetTexture2D>& texture)      { if (m_NormalTexture == texture)      return; m_NormalTexture = texture;      OnMaterialChanged(); }
		void SetRoughnessTexture(const Ref<AssetTexture2D>& texture)   { if (m_RoughnessTexture == texture)   return; m_RoughnessTexture = texture;   OnMaterialChanged(); }
		void SetAOTexture(const Ref<AssetTexture2D>& texture)          { if (m_AOTexture == texture)          return; m_AOTexture = texture;          OnMaterialChanged(); }
		void SetEmissiveTexture(const Ref<AssetTexture2D>& texture)    { if (m_EmissiveTexture == texture)    return; m_EmissiveTexture = texture;    OnMaterialChanged(); }
		void SetOpacityTexture(const Ref<AssetTexture2D>& texture)     { if (m_OpacityTexture == texture)     return; m_OpacityTexture = texture;     OnMaterialChanged(); }
		void SetOpacityMaskTexture(const Ref<AssetTexture2D>& texture) { if (m_OpacityMaskTexture == texture) return; m_OpacityMaskTexture = texture; OnMaterialChanged(); }

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

		// TODO: fix me (rename to Get*Asset)
		const Ref<AssetTexture2D>& GetAlbedoTexture() const { return m_AlbedoTexture; }
		const Ref<AssetTexture2D>& GetMetallnessTexture() const { return m_MetallnessTexture; }
		const Ref<AssetTexture2D>& GetNormalTexture() const { return m_NormalTexture; }
		const Ref<AssetTexture2D>& GetRoughnessTexture() const { return m_RoughnessTexture; }
		const Ref<AssetTexture2D>& GetAOTexture() const { return m_AOTexture; }
		const Ref<AssetTexture2D>& GetEmissiveTexture() const { return m_EmissiveTexture; }
		const Ref<AssetTexture2D>& GetOpacityTexture() const { return m_OpacityTexture; }
		const Ref<AssetTexture2D>& GetOpacityMaskTexture() const { return m_OpacityMaskTexture; }

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
		Ref<AssetTexture2D> m_AlbedoTexture;
		Ref<AssetTexture2D> m_NormalTexture;
		Ref<AssetTexture2D> m_MetallnessTexture;
		Ref<AssetTexture2D> m_RoughnessTexture;
		Ref<AssetTexture2D> m_AOTexture;
		Ref<AssetTexture2D> m_EmissiveTexture;
		Ref<AssetTexture2D> m_OpacityTexture;
		Ref<AssetTexture2D> m_OpacityMaskTexture;

		glm::vec4 m_TintColor = glm::vec4(1.0);
		glm::vec3 m_EmissiveIntensity = glm::vec3(1.f);
		float m_TilingFactor = 1.f;
		BlendMode m_BlendMode = BlendMode::Opaque;
	};
}
