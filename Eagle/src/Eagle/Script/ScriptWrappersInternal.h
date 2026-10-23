#pragma once

// Private helpers shared by the ScriptWrappers*.cpp files. Don't include it anywhere else.

namespace Eagle 
{
	extern MonoImage* s_AppAssemblyImage;
}

namespace Eagle::Script::Utils
{
	inline void SetMaterialTexture(const Ref<Material>& material, GUID guid, void (Material::*textureSetter)(const Ref<AssetTexture2D>&))
	{
		Ref<Asset> asset;
		Material* rawMaterial = material.get();

		if (AssetManager::Get(guid, &asset))
		{
			if (auto asset2D = Cast<AssetTexture2D>(asset))
				(rawMaterial->*textureSetter)(asset2D);
			else
			{
				EG_CORE_ERROR("[ScriptEngine] Failed to update material. Provided asset is not a texture 2D");
				(rawMaterial->*textureSetter)(nullptr);
			}
		}
		else
			(rawMaterial->*textureSetter)(nullptr);
	};

	inline void SetMaterial(const Ref<Material>& material,
		GUID albedoTexture, GUID metalnessTexture, GUID normalTexture, GUID roughnessTexture, GUID aoTexture, GUID emissiveTexture, GUID opacityTexture, GUID opacityMaskTexture,
		const glm::vec3* albedo, float metalness, float roughness, float ao, const glm::vec3* emissive, float opacity, float opacityMask,
		bool bUseAlbedoTexture, bool bUseMetalnessTexture, bool bUseRoughnessTexture, bool bUseAOTexture, bool bUseEmissiveTexture, bool bUseOpacityTexture, bool bUseOpacityMaskTexture,
		const glm::vec4* tint, const glm::vec3* emissiveIntensity, const glm::vec2* tilingFactor, MaterialBlendMode blendMode, bool bDoubleSided,
		Material::TextureChannel metalnessTextureChannel, Material::TextureChannel roughnessTextureChannel, Material::TextureChannel aoTextureChannel,
		Material::TextureChannel opacityTextureChannel, Material::TextureChannel opacityMaskTextureChannel)
	{
		// Update textures
		{
			SetMaterialTexture(material, albedoTexture, &Material::SetAlbedoAsset);
			SetMaterialTexture(material, metalnessTexture, &Material::SetMetalnessAsset);
			SetMaterialTexture(material, normalTexture, &Material::SetNormalAsset);
			SetMaterialTexture(material, roughnessTexture, &Material::SetRoughnessAsset);
			SetMaterialTexture(material, aoTexture, &Material::SetAOAsset);
			SetMaterialTexture(material, emissiveTexture, &Material::SetEmissiveAsset);
			SetMaterialTexture(material, opacityTexture, &Material::SetOpacityAsset);
			SetMaterialTexture(material, opacityMaskTexture, &Material::SetOpacityMaskAsset);
		}

		material->SetMetalnessTextureChannel(metalnessTextureChannel);
		material->SetRoughnessTextureChannel(roughnessTextureChannel);
		material->SetAOTextureChannel(aoTextureChannel);
		material->SetOpacityTextureChannel(opacityTextureChannel);
		material->SetOpacityMaskTextureChannel(opacityMaskTextureChannel);

		// Update raw values
		{
			material->SetAlbedo(*albedo);
			material->SetMetalness(metalness);
			material->SetRoughness(roughness);
			material->SetAO(ao);
			material->SetEmissive(*emissive);
			material->SetOpacity(opacity);
			material->SetOpacityMask(opacityMask);

			material->SetRawAlbedoUsed(!bUseAlbedoTexture);
			material->SetRawMetalnessUsed(!bUseMetalnessTexture);
			material->SetRawRoughnessUsed(!bUseRoughnessTexture);
			material->SetRawAOUsed(!bUseAOTexture);
			material->SetRawEmissiveUsed(!bUseEmissiveTexture);
			material->SetRawOpacityUsed(!bUseOpacityTexture);
			material->SetRawOpacityMaskUsed(!bUseOpacityMaskTexture);
		}

		material->SetTintColor(*tint);
		material->SetEmissiveIntensity(*emissiveIntensity);
		material->SetTilingFactor(*tilingFactor);
		material->SetBlendMode(blendMode);
		material->SetDoubleSided(bDoubleSided);
	}

	inline void GetMaterial(const Ref<Material>& material,
		GUID* outAlbedoTexture, GUID* outMetalnessTexture, GUID* outNormalTexture, GUID* outRoughnessTexture, GUID* outAOTexture, GUID* outEmissiveTexture, GUID* outOpacityTexture, GUID* outOpacityMaskTexture,
		glm::vec3* albedo, float* metalness, float* roughness, float* ao, glm::vec3* emissive, float* opacity, float* opacityMask,
		bool* bUseAlbedoTexture, bool* bUseMetalnessTexture, bool* bUseRoughnessTexture, bool* bUseAOTexture, bool* bUseEmissiveTexture, bool* bUseOpacityTexture, bool* bUseOpacityMaskTexture,
		glm::vec4* outTint, glm::vec3* outEmissiveIntensity, glm::vec2* outTilingFactor, MaterialBlendMode* outBlendMode, bool* bDoubleSided,
		Material::TextureChannel* outMetalnessTextureChannel, Material::TextureChannel* outRoughnessTextureChannel, Material::TextureChannel* outAOTextureChannel,
		Material::TextureChannel* outOpacityTextureChannel, Material::TextureChannel* outOpacityMaskTextureChannel)
	{
		const GUID null(0, 0);

		// Get textures
		{
			// Albedo
			{
				const auto& asset = material->GetAlbedoAsset();
				*outAlbedoTexture = asset ? asset->GetGUID() : null;
			}
			// Metalness
			{
				const auto& asset = material->GetMetalnessAsset();
				*outMetalnessTexture = asset ? asset->GetGUID() : null;
			}
			// Normal
			{
				const auto& asset = material->GetNormalAsset();
				*outNormalTexture = asset ? asset->GetGUID() : null;
			}
			// Roughness
			{
				const auto& asset = material->GetRoughnessAsset();
				*outRoughnessTexture = asset ? asset->GetGUID() : null;
			}
			// AO
			{
				const auto& asset = material->GetAOAsset();
				*outAOTexture = asset ? asset->GetGUID() : null;
			}
			// Emissive
			{
				const auto& asset = material->GetEmissiveAsset();
				*outEmissiveTexture = asset ? asset->GetGUID() : null;
			}
			// Opacity
			{
				const auto& asset = material->GetOpacityAsset();
				*outOpacityTexture = asset ? asset->GetGUID() : null;
			}
			// Opacity Mask
			{
				const auto& asset = material->GetOpacityMaskAsset();
				*outOpacityMaskTexture = asset ? asset->GetGUID() : null;
			}
		}
		
		*outMetalnessTextureChannel = material->GetMetalnessTextureChannel();
		*outRoughnessTextureChannel = material->GetRoughnessTextureChannel();
		*outAOTextureChannel = material->GetAOTextureChannel();
		*outOpacityTextureChannel = material->GetOpacityTextureChannel();
		*outOpacityMaskTextureChannel = material->GetOpacityMaskTextureChannel();

		// Get raw values
		{
			*albedo = material->GetAlbedo();
			*metalness = material->GetMetalness();
			*roughness = material->GetRoughness();
			*ao = material->GetAO();
			*emissive = material->GetEmissive();
			*opacity = material->GetOpacity();
			*opacityMask = material->GetOpacityMask();

			*bUseAlbedoTexture = !material->IsRawAlbedoUsed();
			*bUseMetalnessTexture = !material->IsRawMetalnessUsed();
			*bUseRoughnessTexture = !material->IsRawRoughnessUsed();
			*bUseAOTexture = !material->IsRawAOUsed();
			*bUseEmissiveTexture = !material->IsRawEmissiveUsed();
			*bUseOpacityTexture = !material->IsRawOpacityUsed();
			*bUseOpacityMaskTexture = !material->IsRawOpacityMaskUsed();
		}
		
		*outTint = material->GetTintColor();
		*outEmissiveIntensity = material->GetEmissiveIntensity();
		*outTilingFactor = material->GetTilingFactor();
		*outBlendMode = material->GetBlendMode();
		*bDoubleSided = material->IsDoubleSided();
	}

	inline MonoClass* GetClassFromReflectionType(void* type)
	{
		if (!type)
			return nullptr;

		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);
		return monoType ? mono_class_from_mono_type(monoType) : nullptr;
	}

	inline ScriptUserComponents* GetUserComponents(Entity& entity)
	{
		return entity.HasComponent<ScriptUserComponents>() ? &entity.GetComponent<ScriptUserComponents>() : nullptr;
	}
}
