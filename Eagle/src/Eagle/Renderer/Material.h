#pragma once

#include "Eagle/Asset/Asset.h"

#include <glm/glm.hpp>

namespace Eagle
{
	enum class MaterialTextureChannel
	{
		R = 0, G = 1, B = 2, A = 3
	};

	struct MaterialSettings
	{
		Ref<AssetTexture2D> AlbedoAsset;
		Ref<AssetTexture2D> NormalAsset;
		Ref<AssetTexture2D> MetalnessAsset;
		Ref<AssetTexture2D> RoughnessAsset;
		Ref<AssetTexture2D> AOAsset;
		Ref<AssetTexture2D> EmissiveAsset;
		Ref<AssetTexture2D> OpacityAsset;
		Ref<AssetTexture2D> OpacityMaskAsset;

		// The channel of the texture that's read
		MaterialTextureChannel MetalnessChannel   = MaterialTextureChannel::R;
		MaterialTextureChannel RoughnessChannel   = MaterialTextureChannel::R;
		MaterialTextureChannel AOChannel          = MaterialTextureChannel::R;
		MaterialTextureChannel OpacityChannel     = MaterialTextureChannel::R;
		MaterialTextureChannel OpacityMaskChannel = MaterialTextureChannel::R;

		// Raw values. They're used instead of the textures if their `bRaw...` is set
		glm::vec3 Albedo = glm::vec3(0.f);
		float Metalness = 0.f;   // [0; 1]
		glm::vec3 Emissive = glm::vec3(0.f);
		float Roughness = 0.5f;  // [0; 1]
		float AO = 1.f;          // [0; 1]
		float Opacity = 0.5f;    // [0; 1]
		float OpacityMask = 1.f; // [0; 1]

		bool bRawAlbedo = true;
		bool bRawMetalness = true;
		bool bRawEmissive = true;
		bool bRawRoughness = true;
		bool bRawAO = true;
		bool bRawOpacity = true;
		bool bRawOpacityMask = true;

		glm::vec4 TintColor = glm::vec4(1.f);
		glm::vec3 EmissiveIntensity = glm::vec3(1.f);
		glm::vec2 TilingFactor = glm::vec2(1.f);
		MaterialBlendMode BlendMode = MaterialBlendMode::Opaque;
		bool bDoubleSided = false;

		bool operator==(const MaterialSettings& other) const = default;
	};

	class Material : virtual public std::enable_shared_from_this<Material>
	{
	public:
		using TextureChannel = MaterialTextureChannel;

		virtual ~Material() = default;

		const MaterialSettings& GetSettings() const { return m_Settings; }
		void SetSettings(const MaterialSettings& settings);

		void SetAlbedoAsset(const Ref<AssetTexture2D>& asset)      { if (m_Settings.AlbedoAsset == asset)      return; m_Settings.AlbedoAsset = asset;      OnMaterialChanged(); }
		void SetMetalnessAsset(const Ref<AssetTexture2D>& asset)   { if (m_Settings.MetalnessAsset == asset)   return; m_Settings.MetalnessAsset = asset;   OnMaterialChanged(); }
		void SetNormalAsset(const Ref<AssetTexture2D>& asset)      { if (m_Settings.NormalAsset == asset)      return; m_Settings.NormalAsset = asset;      OnMaterialChanged(); }
		void SetRoughnessAsset(const Ref<AssetTexture2D>& asset)   { if (m_Settings.RoughnessAsset == asset)   return; m_Settings.RoughnessAsset = asset;   OnMaterialChanged(); }
		void SetAOAsset(const Ref<AssetTexture2D>& asset)          { if (m_Settings.AOAsset == asset)          return; m_Settings.AOAsset = asset;          OnMaterialChanged(); }
		void SetEmissiveAsset(const Ref<AssetTexture2D>& asset)    { if (m_Settings.EmissiveAsset == asset)    return; m_Settings.EmissiveAsset = asset;    OnMaterialChanged(); }
		void SetOpacityAsset(const Ref<AssetTexture2D>& asset)     { if (m_Settings.OpacityAsset == asset)     return; m_Settings.OpacityAsset = asset;     OnMaterialChanged(); }
		void SetOpacityMaskAsset(const Ref<AssetTexture2D>& asset) { if (m_Settings.OpacityMaskAsset == asset) return; m_Settings.OpacityMaskAsset = asset; OnMaterialChanged(); }

		// Can be used to select a channel to use for a texture read
		void SetMetalnessTextureChannel(TextureChannel channel)   { if (m_Settings.MetalnessChannel == channel)   return; m_Settings.MetalnessChannel = channel;   OnMaterialChanged(); }
		void SetRoughnessTextureChannel(TextureChannel channel)   { if (m_Settings.RoughnessChannel == channel)   return; m_Settings.RoughnessChannel = channel;   OnMaterialChanged(); }
		void SetAOTextureChannel(TextureChannel channel)          { if (m_Settings.AOChannel == channel)          return; m_Settings.AOChannel = channel;          OnMaterialChanged(); }
		void SetOpacityTextureChannel(TextureChannel channel)     { if (m_Settings.OpacityChannel == channel)     return; m_Settings.OpacityChannel = channel;     OnMaterialChanged(); }
		void SetOpacityMaskTextureChannel(TextureChannel channel) { if (m_Settings.OpacityMaskChannel == channel) return; m_Settings.OpacityMaskChannel = channel; OnMaterialChanged(); }

		void SetAlbedo(const glm::vec3& value)   { m_Settings.Albedo      = value;                       if (m_Settings.bRawAlbedo)      OnMaterialChanged(); }
		void SetMetalness(float value)           { m_Settings.Metalness   = glm::clamp(value, 0.f, 1.f); if (m_Settings.bRawMetalness)   OnMaterialChanged(); }
		void SetRoughness(float value)           { m_Settings.Roughness   = glm::clamp(value, 0.f, 1.f); if (m_Settings.bRawRoughness)   OnMaterialChanged(); }
		void SetAO(float value)                  { m_Settings.AO          = glm::clamp(value, 0.f, 1.f); if (m_Settings.bRawAO)          OnMaterialChanged(); }
		void SetEmissive(const glm::vec3& value) { m_Settings.Emissive    = value;                       if (m_Settings.bRawEmissive)    OnMaterialChanged(); }
		void SetOpacity(float value)             { m_Settings.Opacity     = glm::clamp(value, 0.f, 1.f); if (m_Settings.bRawOpacity)     OnMaterialChanged(); }
		void SetOpacityMask(float value)         { m_Settings.OpacityMask = glm::clamp(value, 0.f, 1.f); if (m_Settings.bRawOpacityMask) OnMaterialChanged(); }

		void SetRawAlbedoUsed(bool bUse)      { if (m_Settings.bRawAlbedo      == bUse) return; m_Settings.bRawAlbedo      = bUse; OnMaterialChanged(); }
		void SetRawMetalnessUsed(bool bUse)   { if (m_Settings.bRawMetalness   == bUse) return; m_Settings.bRawMetalness   = bUse; OnMaterialChanged(); }
		void SetRawRoughnessUsed(bool bUse)   { if (m_Settings.bRawRoughness   == bUse) return; m_Settings.bRawRoughness   = bUse; OnMaterialChanged(); }
		void SetRawAOUsed(bool bUse)          { if (m_Settings.bRawAO          == bUse) return; m_Settings.bRawAO          = bUse; OnMaterialChanged(); }
		void SetRawEmissiveUsed(bool bUse)    { if (m_Settings.bRawEmissive    == bUse) return; m_Settings.bRawEmissive    = bUse; OnMaterialChanged(); }
		void SetRawOpacityUsed(bool bUse)     { if (m_Settings.bRawOpacity     == bUse) return; m_Settings.bRawOpacity     = bUse; OnMaterialChanged(); }
		void SetRawOpacityMaskUsed(bool bUse) { if (m_Settings.bRawOpacityMask == bUse) return; m_Settings.bRawOpacityMask = bUse; OnMaterialChanged(); }

		void SetTintColor(const glm::vec4& tintColor)         { m_Settings.TintColor = tintColor;         OnMaterialChanged(); }
		void SetEmissiveIntensity(const glm::vec3& intensity) { m_Settings.EmissiveIntensity = intensity; OnMaterialChanged(); }
		void SetTilingFactor(glm::vec2 tiling)                { m_Settings.TilingFactor = tiling;         OnMaterialChanged(); }
		void SetBlendMode(MaterialBlendMode blendMode)
		{
			if (blendMode == m_Settings.BlendMode)
				return;

			m_Settings.BlendMode = blendMode;
			OnMaterialChanged(true);
		}

		void SetDoubleSided(bool bDoubleSided)
		{
			if (m_Settings.bDoubleSided == bDoubleSided)
				return;

			m_Settings.bDoubleSided = bDoubleSided;
			OnMaterialChanged(true);
		}

		const Ref<AssetTexture2D>& GetAlbedoAsset() const { return m_Settings.AlbedoAsset; }
		const Ref<AssetTexture2D>& GetMetalnessAsset() const { return m_Settings.MetalnessAsset; }
		const Ref<AssetTexture2D>& GetNormalAsset() const { return m_Settings.NormalAsset; }
		const Ref<AssetTexture2D>& GetRoughnessAsset() const { return m_Settings.RoughnessAsset; }
		const Ref<AssetTexture2D>& GetAOAsset() const { return m_Settings.AOAsset; }
		const Ref<AssetTexture2D>& GetEmissiveAsset() const { return m_Settings.EmissiveAsset; }
		const Ref<AssetTexture2D>& GetOpacityAsset() const { return m_Settings.OpacityAsset; }
		const Ref<AssetTexture2D>& GetOpacityMaskAsset() const { return m_Settings.OpacityMaskAsset; }

		TextureChannel GetMetalnessTextureChannel() const { return m_Settings.MetalnessChannel; }
		TextureChannel GetRoughnessTextureChannel() const { return m_Settings.RoughnessChannel; }
		TextureChannel GetAOTextureChannel() const { return m_Settings.AOChannel; }
		TextureChannel GetOpacityTextureChannel() const { return m_Settings.OpacityChannel; }
		TextureChannel GetOpacityMaskTextureChannel() const { return m_Settings.OpacityMaskChannel; }

		glm::vec3 GetAlbedo() const { return m_Settings.Albedo; }
		float GetMetalness() const { return m_Settings.Metalness; }
		float GetRoughness() const { return m_Settings.Roughness; }
		float GetAO() const { return m_Settings.AO; }
		glm::vec3 GetEmissive() const { return m_Settings.Emissive; }
		float GetOpacity() const { return m_Settings.Opacity; }
		float GetOpacityMask() const { return m_Settings.OpacityMask; }

		bool IsRawAlbedoUsed() const { return m_Settings.bRawAlbedo; }
		bool IsRawMetalnessUsed() const { return m_Settings.bRawMetalness; }
		bool IsRawRoughnessUsed() const { return m_Settings.bRawRoughness; }
		bool IsRawAOUsed() const { return m_Settings.bRawAO; }
		bool IsRawEmissiveUsed() const { return m_Settings.bRawEmissive; }
		bool IsRawOpacityUsed() const { return m_Settings.bRawOpacity; }
		bool IsRawOpacityMaskUsed() const { return m_Settings.bRawOpacityMask; }

		const glm::vec4& GetTintColor() const { return m_Settings.TintColor; }
		const glm::vec3& GetEmissiveIntensity() const { return m_Settings.EmissiveIntensity; }
		glm::vec2 GetTilingFactor() const { return m_Settings.TilingFactor; }
		MaterialBlendMode GetBlendMode() const { return m_Settings.BlendMode; }
		bool IsDoubleSided() const { return m_Settings.bDoubleSided; }

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

		MaterialSettings m_Settings;
	};
}
