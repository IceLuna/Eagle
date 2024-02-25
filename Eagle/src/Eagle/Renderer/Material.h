#pragma once

#include "Eagle/Asset/Asset.h"

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

		void SetAlbedoAsset(const Ref<AssetTexture2D>& asset)      { if (m_AlbedoAsset == asset)      return; m_AlbedoAsset = asset;      OnMaterialChanged(); }
		void SetMetallnessAsset(const Ref<AssetTexture2D>& asset)  { if (m_MetallnessAsset == asset)  return; m_MetallnessAsset = asset;  OnMaterialChanged(); }
		void SetNormalAsset(const Ref<AssetTexture2D>& asset)      { if (m_NormalAsset == asset)      return; m_NormalAsset = asset;      OnMaterialChanged(); }
		void SetRoughnessAsset(const Ref<AssetTexture2D>& asset)   { if (m_RoughnessAsset == asset)   return; m_RoughnessAsset = asset;   OnMaterialChanged(); }
		void SetAOAsset(const Ref<AssetTexture2D>& asset)          { if (m_AOAsset == asset)          return; m_AOAsset = asset;          OnMaterialChanged(); }
		void SetEmissiveAsset(const Ref<AssetTexture2D>& asset)    { if (m_EmissiveAsset == asset)    return; m_EmissiveAsset = asset;    OnMaterialChanged(); }
		void SetOpacityAsset(const Ref<AssetTexture2D>& asset)     { if (m_OpacityAsset == asset)     return; m_OpacityAsset = asset;     OnMaterialChanged(); }
		void SetOpacityMaskAsset(const Ref<AssetTexture2D>& asset) { if (m_OpacityMaskAsset == asset) return; m_OpacityMaskAsset = asset; OnMaterialChanged(); }

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

		const Ref<AssetTexture2D>& GetAlbedoAsset() const { return m_AlbedoAsset; }
		const Ref<AssetTexture2D>& GetMetallnessAsset() const { return m_MetallnessAsset; }
		const Ref<AssetTexture2D>& GetNormalAsset() const { return m_NormalAsset; }
		const Ref<AssetTexture2D>& GetRoughnessAsset() const { return m_RoughnessAsset; }
		const Ref<AssetTexture2D>& GetAOAsset() const { return m_AOAsset; }
		const Ref<AssetTexture2D>& GetEmissiveAsset() const { return m_EmissiveAsset; }
		const Ref<AssetTexture2D>& GetOpacityAsset() const { return m_OpacityAsset; }
		const Ref<AssetTexture2D>& GetOpacityMaskAsset() const { return m_OpacityMaskAsset; }

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
		Ref<AssetTexture2D> m_AlbedoAsset;
		Ref<AssetTexture2D> m_NormalAsset;
		Ref<AssetTexture2D> m_MetallnessAsset;
		Ref<AssetTexture2D> m_RoughnessAsset;
		Ref<AssetTexture2D> m_AOAsset;
		Ref<AssetTexture2D> m_EmissiveAsset;
		Ref<AssetTexture2D> m_OpacityAsset;
		Ref<AssetTexture2D> m_OpacityMaskAsset;

		glm::vec4 m_TintColor = glm::vec4(1.0);
		glm::vec3 m_EmissiveIntensity = glm::vec3(1.f);
		float m_TilingFactor = 1.f;
		BlendMode m_BlendMode = BlendMode::Opaque;
	};
}
