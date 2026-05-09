#pragma once

#include "Eagle/Asset/Asset.h"

#include <glm/glm.hpp>

namespace Eagle
{
	class Material : virtual public std::enable_shared_from_this<Material>
	{
	public:
		enum class TextureChannel
		{
			R = 0, G = 1, B = 2, A = 3
		};

		virtual ~Material() = default;

		void SetAlbedoAsset(const Ref<AssetTexture2D>& asset)      { if (m_AlbedoAsset == asset)      return; m_AlbedoAsset = asset;      OnMaterialChanged(); }
		void SetMetalnessAsset(const Ref<AssetTexture2D>& asset)   { if (m_MetallnessAsset == asset)  return; m_MetallnessAsset = asset;  OnMaterialChanged(); }
		void SetNormalAsset(const Ref<AssetTexture2D>& asset)      { if (m_NormalAsset == asset)      return; m_NormalAsset = asset;      OnMaterialChanged(); }
		void SetRoughnessAsset(const Ref<AssetTexture2D>& asset)   { if (m_RoughnessAsset == asset)   return; m_RoughnessAsset = asset;   OnMaterialChanged(); }
		void SetAOAsset(const Ref<AssetTexture2D>& asset)          { if (m_AOAsset == asset)          return; m_AOAsset = asset;          OnMaterialChanged(); }
		void SetEmissiveAsset(const Ref<AssetTexture2D>& asset)    { if (m_EmissiveAsset == asset)    return; m_EmissiveAsset = asset;    OnMaterialChanged(); }
		void SetOpacityAsset(const Ref<AssetTexture2D>& asset)     { if (m_OpacityAsset == asset)     return; m_OpacityAsset = asset;     OnMaterialChanged(); }
		void SetOpacityMaskAsset(const Ref<AssetTexture2D>& asset) { if (m_OpacityMaskAsset == asset) return; m_OpacityMaskAsset = asset; OnMaterialChanged(); }

		// Can be used to select a channel to use for a texture read
		void SetMetalnessTextureChannel(TextureChannel channel)   { if (m_MetalnessTextureChannel == channel)   return; m_MetalnessTextureChannel = channel;   OnMaterialChanged(); }
		void SetRoughnessTextureChannel(TextureChannel channel)   { if (m_RoughnessTextureChannel == channel)   return; m_RoughnessTextureChannel = channel;   OnMaterialChanged(); }
		void SetAOTextureChannel(TextureChannel channel)          { if (m_AOTextureChannel == channel)          return; m_AOTextureChannel = channel;          OnMaterialChanged(); }
		void SetOpacityTextureChannel(TextureChannel channel)     { if (m_OpacityTextureChannel == channel)     return; m_OpacityTextureChannel = channel;     OnMaterialChanged(); }
		void SetOpacityMaskTextureChannel(TextureChannel channel) { if (m_OpacityMaskTextureChannel == channel) return; m_OpacityMaskTextureChannel = channel; OnMaterialChanged(); }

		void SetAlbedo(const glm::vec3& value)   { m_Albedo.first      = value;                       if (m_Albedo.second)      OnMaterialChanged(); }
		void SetMetalness(float value)           { m_Metalness.first   = glm::clamp(value, 0.f, 1.f); if (m_Metalness.second)   OnMaterialChanged(); }
		void SetRoughness(float value)           { m_Roughness.first   = glm::clamp(value, 0.f, 1.f); if (m_Roughness.second)   OnMaterialChanged(); }
		void SetAO(float value)                  { m_AO.first          = glm::clamp(value, 0.f, 1.f); if (m_AO.second)          OnMaterialChanged(); }
		void SetEmissive(const glm::vec3& value) { m_Emissive.first    = value;                       if (m_Emissive.second)    OnMaterialChanged(); }
		void SetOpacity(float value)             { m_Opacity.first     = glm::clamp(value, 0.f, 1.f); if (m_Opacity.second)     OnMaterialChanged(); }
		void SetOpacityMask(float value)         { m_OpacityMask.first = glm::clamp(value, 0.f, 1.f); if (m_OpacityMask.second) OnMaterialChanged(); }

		void SetRawAlbedoUsed(bool bUse)      { if (m_Albedo.second      == bUse) return; m_Albedo.second      = bUse; OnMaterialChanged();}
		void SetRawMetalnessUsed(bool bUse)   { if (m_Metalness.second   == bUse) return; m_Metalness.second   = bUse; OnMaterialChanged();}
		void SetRawRoughnessUsed(bool bUse)   { if (m_Roughness.second   == bUse) return; m_Roughness.second   = bUse; OnMaterialChanged();}
		void SetRawAOUsed(bool bUse)          { if (m_AO.second          == bUse) return; m_AO.second          = bUse; OnMaterialChanged();}
		void SetRawEmissiveUsed(bool bUse)    { if (m_Emissive.second    == bUse) return; m_Emissive.second    = bUse; OnMaterialChanged();}
		void SetRawOpacityUsed(bool bUse)     { if (m_Opacity.second     == bUse) return; m_Opacity.second     = bUse; OnMaterialChanged();}
		void SetRawOpacityMaskUsed(bool bUse) { if (m_OpacityMask.second == bUse) return; m_OpacityMask.second = bUse; OnMaterialChanged();}

		void SetTintColor(const glm::vec4& tintColor)         { m_TintColor = tintColor;         OnMaterialChanged(); }
		void SetEmissiveIntensity(const glm::vec3& intensity) { m_EmissiveIntensity = intensity; OnMaterialChanged(); }
		void SetTilingFactor(float tiling)                    { m_TilingFactor = tiling;         OnMaterialChanged(); }
		void SetBlendMode(MaterialBlendMode blendMode)
		{
			if (blendMode == m_BlendMode)
				return;

			m_BlendMode = blendMode;
			OnMaterialChanged(true);
		}

		void SetDoubleSided(bool bDoubleSided)
		{
			if (this->bDoubleSided == bDoubleSided)
				return;

			this->bDoubleSided = bDoubleSided;
			OnMaterialChanged(true);
		}

		const Ref<AssetTexture2D>& GetAlbedoAsset() const { return m_AlbedoAsset; }
		const Ref<AssetTexture2D>& GetMetalnessAsset() const { return m_MetallnessAsset; }
		const Ref<AssetTexture2D>& GetNormalAsset() const { return m_NormalAsset; }
		const Ref<AssetTexture2D>& GetRoughnessAsset() const { return m_RoughnessAsset; }
		const Ref<AssetTexture2D>& GetAOAsset() const { return m_AOAsset; }
		const Ref<AssetTexture2D>& GetEmissiveAsset() const { return m_EmissiveAsset; }
		const Ref<AssetTexture2D>& GetOpacityAsset() const { return m_OpacityAsset; }
		const Ref<AssetTexture2D>& GetOpacityMaskAsset() const { return m_OpacityMaskAsset; }

		TextureChannel GetMetalnessTextureChannel() const { return m_MetalnessTextureChannel; }
		TextureChannel GetRoughnessTextureChannel() const { return m_RoughnessTextureChannel; }
		TextureChannel GetAOTextureChannel() const { return m_AOTextureChannel; }
		TextureChannel GetOpacityTextureChannel() const { return m_OpacityTextureChannel; }
		TextureChannel GetOpacityMaskTextureChannel() const { return m_OpacityMaskTextureChannel; }

		glm::vec3 GetAlbedo() const { return m_Albedo.first; }
		float GetMetalness() const { return m_Metalness.first; }
		float GetRoughness() const { return m_Roughness.first; }
		float GetAO() const { return m_AO.first; }
		glm::vec3 GetEmissive() const { return m_Emissive.first; }
		float GetOpacity() const { return m_Opacity.first; }
		float GetOpacityMask() const { return m_OpacityMask.first; }

		bool IsRawAlbedoUsed() const { return m_Albedo.second; }
		bool IsRawMetalnessUsed() const { return m_Metalness.second; }
		bool IsRawRoughnessUsed() const { return m_Roughness.second; }
		bool IsRawAOUsed() const { return m_AO.second; }
		bool IsRawEmissiveUsed() const { return m_Emissive.second; }
		bool IsRawOpacityUsed() const { return m_Opacity.second; }
		bool IsRawOpacityMaskUsed() const { return m_OpacityMask.second; }

		const glm::vec4& GetTintColor() const { return m_TintColor; }
		const glm::vec3& GetEmissiveIntensity() const { return m_EmissiveIntensity; }
		float GetTilingFactor() const { return m_TilingFactor; }
		MaterialBlendMode GetBlendMode() const { return m_BlendMode; }
		bool IsDoubleSided() const { return bDoubleSided; }

		void AddOnModifiedCallback(const GUID& id, const std::function<void()>& func)
		{
			std::scoped_lock lock(m_Mutex);
			m_Callbacks[id] = func;
		}

		void RemoveOnModifiedCallback(const GUID& id)
		{
			std::scoped_lock lock(m_Mutex);
			m_Callbacks.erase(id);
		}

		static Ref<Material> Create();
		static Ref<Material> Create(const Ref<Material>& other);

	protected:
		Material() = default;
		Material(const Ref<Material>& other);

		Material(const Material& other) = delete;
		Material(Material&&) = delete;

		Material& operator= (const Material&) = delete;
		Material& operator= (Material&&) = delete;

		void OnMaterialChanged(bool bRenderModeChanged = false);

	private:
		std::mutex m_Mutex;
		std::unordered_map<GUID, std::function<void()>> m_Callbacks;

		Ref<AssetTexture2D> m_AlbedoAsset;
		Ref<AssetTexture2D> m_NormalAsset;
		Ref<AssetTexture2D> m_MetallnessAsset;
		Ref<AssetTexture2D> m_RoughnessAsset;
		Ref<AssetTexture2D> m_AOAsset;
		Ref<AssetTexture2D> m_EmissiveAsset;
		Ref<AssetTexture2D> m_OpacityAsset;
		Ref<AssetTexture2D> m_OpacityMaskAsset;

		// Can be used to select a channel to use for a texture read
		TextureChannel m_MetalnessTextureChannel   = TextureChannel::R;
		TextureChannel m_RoughnessTextureChannel   = TextureChannel::R;
		TextureChannel m_AOTextureChannel          = TextureChannel::R;
		TextureChannel m_OpacityTextureChannel     = TextureChannel::R;
		TextureChannel m_OpacityMaskTextureChannel = TextureChannel::R;

		// Bool indicates if raw values should be used instead of an asset
		std::pair<glm::vec3, bool> m_Albedo      = { glm::vec3(0.f), true };
		std::pair<float, bool>     m_Metalness   = { 0.f,            true };
		std::pair<glm::vec3, bool> m_Emissive    = { glm::vec3(0.f), true };
		std::pair<float, bool>     m_Roughness   = { 0.5f,           true };
		std::pair<float, bool>     m_AO          = { 1.0f,           true };
		std::pair<float, bool>     m_Opacity     = { 0.5f,           true };
		std::pair<float, bool>     m_OpacityMask = { 1.0f,           true };

		glm::vec4 m_TintColor = glm::vec4(1.0);
		glm::vec3 m_EmissiveIntensity = glm::vec3(1.f);
		float m_TilingFactor = 1.f;
		MaterialBlendMode m_BlendMode = MaterialBlendMode::Opaque;
		bool bDoubleSided = false;
	};
}
