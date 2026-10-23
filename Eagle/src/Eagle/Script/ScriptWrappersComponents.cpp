#include "egpch.h"

#include "ScriptWrappers.h"
#include "ScriptEngineRegistry.h"
#include "ScriptUserComponents.h"

#include "Eagle/Asset/AssetManager.h"
#include "Eagle/Utils/Utils.h"
#include "Eagle/Classes/PSAutoDestroyScript.h"
#include "Eagle/Renderer/SceneRenderer.h"

#include <mono/jit/jit.h>

// Note: keep last
#include "ScriptWrappersInternal.h"

namespace Eagle
{
	//--------------Camera Component--------------
	void Script::Eagle_CameraComponent_SetIsPrimary(GUID entityID, bool val)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			entity.GetComponent<CameraComponent>().Primary = val;
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set 'IsPrimary'. Entity is null");
	}

	bool Script::Eagle_CameraComponent_GetIsPrimary(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			return entity.GetComponent<CameraComponent>().Primary;
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't read 'IsPrimary'. Entity is null");
			return false;
		}
	}

	float Script::Eagle_CameraComponent_GetPerspectiveVerticalFOV(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			return entity.GetComponent<CameraComponent>().Camera.GetPerspectiveVerticalFOV();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't read 'PerspectiveVerticalFOV'. Entity is null");
			return 0.f;
		}
	}

	void Script::Eagle_CameraComponent_SetPerspectiveVerticalFOV(GUID entityID, float value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			entity.GetComponent<CameraComponent>().Camera.SetPerspectiveVerticalFOV(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set 'PerspectiveVerticalFOV'. Entity is null");
	}

	float Script::Eagle_CameraComponent_GetPerspectiveNearClip(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			return entity.GetComponent<CameraComponent>().Camera.GetPerspectiveNearClip();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't read 'PerspectiveNearClip'. Entity is null");
			return 0.f;
		}
	}

	void Script::Eagle_CameraComponent_SetPerspectiveNearClip(GUID entityID, float value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			entity.GetComponent<CameraComponent>().Camera.SetPerspectiveNearClip(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set 'PerspectiveNearClip'. Entity is null");
	}

	float Script::Eagle_CameraComponent_GetPerspectiveFarClip(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			return entity.GetComponent<CameraComponent>().Camera.GetPerspectiveFarClip();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't read 'PerspectiveFarClip'. Entity is null");
			return 0.f;
		}
	}

	void Script::Eagle_CameraComponent_SetPerspectiveFarClip(GUID entityID, float value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			entity.GetComponent<CameraComponent>().Camera.SetPerspectiveFarClip(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set 'PerspectiveFarClip'. Entity is null");
	}

	float Script::Eagle_CameraComponent_GetShadowFarClip(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			return entity.GetComponent<CameraComponent>().Camera.GetShadowFarClip();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get 'ShadowFarClip'. Entity is null");
			return 0.f;
		}
	}

	void Script::Eagle_CameraComponent_SetShadowFarClip(GUID entityID, float value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			entity.GetComponent<CameraComponent>().Camera.SetShadowFarClip(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set 'ShadowFarClip'. Entity is null");
	}

	float Script::Eagle_CameraComponent_GetDirLightShadowFarClip(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			return entity.GetComponent<CameraComponent>().Camera.GetDirLightShadowFarClip();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get 'DirLightShadowFarClip'. Entity is null");
			return 0.f;
		}
	}

	void Script::Eagle_CameraComponent_SetDirLightShadowFarClip(GUID entityID, float value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			entity.GetComponent<CameraComponent>().Camera.SetDirLightShadowFarClip(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set 'DirLightShadowFarClip'. Entity is null");
	}

	float Script::Eagle_CameraComponent_GetCascadesSplitAlpha(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			return entity.GetComponent<CameraComponent>().Camera.GetCascadesSplitAlpha();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get 'CascadesSplitAlpha'. Entity is null");
			return 0.f;
		}
	}

	void Script::Eagle_CameraComponent_SetCascadesSplitAlpha(GUID entityID, float value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			entity.GetComponent<CameraComponent>().Camera.SetCascadesSplitAlpha(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set 'CascadesSplitAlpha'. Entity is null");
	}

	float Script::Eagle_CameraComponent_GetCascadesSmoothTransitionAlpha(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			return entity.GetComponent<CameraComponent>().Camera.GetCascadesSmoothTransitionAlpha();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get 'CascadesSmoothTransitionAlpha'. Entity is null");
			return 0.f;
		}
	}

	void Script::Eagle_CameraComponent_SetCascadesSmoothTransitionAlpha(GUID entityID, float value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			entity.GetComponent<CameraComponent>().Camera.SetCascadesSmoothTransitionAlpha(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set 'CascadesSmoothTransitionAlpha'. Entity is null");
	}

	CameraProjectionMode Script::Eagle_CameraComponent_GetCameraProjectionMode(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			return entity.GetComponent<CameraComponent>().Camera.GetProjectionMode();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't read 'ProjectionMode'. Entity is null");
			return CameraProjectionMode::Perspective;
		}
	}

	void Script::Eagle_CameraComponent_SetCameraProjectionMode(GUID entityID, CameraProjectionMode value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			entity.GetComponent<CameraComponent>().Camera.SetProjectionMode(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set 'ProjectionMode'. Entity is null");
	}

	float Script::Eagle_CameraComponent_GetAspectRatio(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			return entity.GetComponent<CameraComponent>().Camera.GetAspectRatio();

		EG_CORE_ERROR("[ScriptEngine] Couldn't call Camera Component 'GetAspectRatio'. Entity is null");
		return 0.f;
	}

	//--------------Reverb Component--------------
	bool Script::Eagle_ReverbComponent_IsActive(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			return entity.GetComponent<ReverbComponent>().IsActive();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'IsActive'. Entity is null");
			return false;
		}
	}

	void Script::Eagle_ReverbComponent_SetIsActive(GUID entityID, bool value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			entity.GetComponent<ReverbComponent>().SetActive(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call 'SetActive'. Entity is null");
	}

	ReverbPreset Script::Eagle_ReverbComponent_GetReverbPreset(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			return entity.GetComponent<ReverbComponent>().GetPreset();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get 'Preset'. Entity is null");
			return ReverbPreset::Generic;
		}
	}

	void Script::Eagle_ReverbComponent_SetReverbPreset(GUID entityID, ReverbPreset value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			entity.GetComponent<ReverbComponent>().SetPreset(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set 'Preset'. Entity is null");
	}

	float Script::Eagle_ReverbComponent_GetMinDistance(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			return entity.GetComponent<ReverbComponent>().GetMinDistance();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't read 'MinDistance'. Entity is null");
			return 0.f;
		}
	}

	void Script::Eagle_ReverbComponent_SetMinDistance(GUID entityID, float value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			entity.GetComponent<ReverbComponent>().SetMinDistance(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set 'MinDistance'. Entity is null");
	}

	float Script::Eagle_ReverbComponent_GetMaxDistance(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			return entity.GetComponent<ReverbComponent>().GetMaxDistance();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't read 'MaxDistance'. Entity is null");
			return 0.f;
		}
	}

	void Script::Eagle_ReverbComponent_SetMaxDistance(GUID entityID, float value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);

		if (entity)
			entity.GetComponent<ReverbComponent>().SetMaxDistance(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set 'MaxDistance'. Entity is null");
	}

	//--------------Text Component--------------
	MonoString* Script::Eagle_TextComponent_GetText(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return mono_string_new(mono_domain_get(), entity.GetComponent<TextComponent>().GetText().c_str());
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get Text of Text Component. Entity is null");
			return nullptr;
		}
	}

	void Script::Eagle_TextComponent_SetText(GUID entityID, MonoString* value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<TextComponent>().SetText(MonoStringHandler(value).c_str());
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set Text of Text Component. Entity is null");
	}

	void Script::Eagle_TextComponent_GetColor(GUID entityID, glm::vec3* outValue)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			*outValue = entity.GetComponent<TextComponent>().GetColor();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get color of Text Component. Entity is null");
	}

	void Script::Eagle_TextComponent_SetColor(GUID entityID, const glm::vec3* value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<TextComponent>().SetColor(*value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set color of Text Component. Entity is null");
	}

	float Script::Eagle_TextComponent_GetOpacity(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<TextComponent>().GetOpacity();

		EG_CORE_ERROR("[ScriptEngine] Couldn't get opacity of Text Component. Entity is null");
		return 1.f;
	}

	void Script::Eagle_TextComponent_SetOpacity(GUID entityID, float value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<TextComponent>().SetOpacity(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set opacity of Text Component. Entity is null");
	}

	float Script::Eagle_TextComponent_GetLineSpacing(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<TextComponent>().GetLineSpacing();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get line spacing of Text Component. Entity is null");
			return 0.f;
		}
	}

	void Script::Eagle_TextComponent_SetLineSpacing(GUID entityID, float value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<TextComponent>().SetLineSpacing(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set line spacing of Text Component. Entity is null");
	}

	float Script::Eagle_TextComponent_GetLetterSpacing(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<TextComponent>().GetLetterSpacing();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get letter spacing of Text Component. Entity is null");
			return 0.f;
		}
	}

	void Script::Eagle_TextComponent_SetLetterSpacing(GUID entityID, float value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<TextComponent>().SetLetterSpacing(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set letter spacing of Text Component. Entity is null");
	}

	float Script::Eagle_TextComponent_GetMaxWidth(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<TextComponent>().GetMaxWidth();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get max width of Text Component. Entity is null");
			return 0.f;
		}
	}

	void Script::Eagle_TextComponent_SetMaxWidth(GUID entityID, float value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<TextComponent>().SetMaxWidth(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set max width of Text Component. Entity is null");
	}

	TextAlignment Script::Eagle_TextComponent_GetAlignment(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<TextComponent>().GetAlignment();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get alignment of Text Component. Entity is null");
			return TextAlignment::Left;
		}
	}

	void Script::Eagle_TextComponent_SetAlignment(GUID entityID, TextAlignment value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<TextComponent>().SetAlignment(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set alignment of Text Component. Entity is null");
	}

	bool Script::Eagle_TextComponent_GetIsLit(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<TextComponent>().IsLit();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get `IsLit` of Text Component. Entity is null");
			return false;
		}
	}

	void Script::Eagle_TextComponent_SetIsLit(GUID entityID, bool value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<TextComponent>().SetIsLit(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set `IsLit` of Text Component. Entity is null");
	}
	
	bool Script::Eagle_TextComponent_DoesCastShadows(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<TextComponent>().DoesCastShadows();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `DoesCastShadows` of Text Component. Entity is null");
			return false;
		}
	}

	void Script::Eagle_TextComponent_SetCastsShadows(GUID entityID, bool value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<TextComponent>().SetCastsShadows(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetCastsShadows` of Text Component. Entity is null");
	}
	
	bool Script::Eagle_TextComponent_DoesReceiveDecals(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<TextComponent>().DoesReceiveDecals();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `DoesReceiveDecals` of Text Component. Entity is null");
			return false;
		}
	}

	void Script::Eagle_TextComponent_SetVisible(GUID entityID, bool value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<TextComponent>().SetVisible(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetVisible` of Text Component. Entity is null");
	}

	bool Script::Eagle_TextComponent_IsVisible(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<TextComponent>().IsVisible();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `IsVisible` of Text Component. Entity is null");
			return false;
		}
	}

	void Script::Eagle_TextComponent_SetDoubleSided(GUID entityID, bool value)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<TextComponent>().SetDoubleSided(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetDoubleSided` of Text Component. Entity is null");
	}

	bool Script::Eagle_TextComponent_IsDoubleSided(GUID entityID)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<TextComponent>().IsDoubleSided();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `IsDoubleSided` of Text Component. Entity is null");
			return false;
		}
	}

	void Script::Eagle_TextComponent_SetReceivesDecals(GUID entityID, bool value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<TextComponent>().SetReceivesDecals(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetReceivesDecals` of Text Component. Entity is null");
	}

	GUID Script::Eagle_TextComponent_GetFont(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get TextComponent font. Entity is null");
			return GUID(0, 0);
		}

		const auto& component = entity.GetComponent<TextComponent>();
		const auto& asset = component.GetFontAsset();
		return asset ? asset->GetGUID() : GUID(0, 0);
	}

	void Script::Eagle_TextComponent_SetFont(GUID entityID, GUID assetID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set TextComponent font. Entity is null");
			return;
		}

		auto& component = entity.GetComponent<TextComponent>();
		if (assetID.IsNull())
		{
			component.SetFontAsset(nullptr);
			return;
		}

		Ref<Asset> asset;
		if (!AssetManager::Get(assetID, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set TextComponent texture. Couldn't find an asset");
			return;
		}

		Ref<AssetFont> fontAsset = Cast<AssetFont>(asset);
		if (!fontAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set TextComponent texture. Provided asset is not a font asset");
			return;
		}

		component.SetFontAsset(fontAsset);
	}

	void Script::Eagle_TextComponent_GetMaterial(GUID entityID, GUID* outAssetID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get Text Component material. Entity is null");
			return;
		}

		const auto& component = entity.GetComponent<TextComponent>();
		const auto& materialAsset = component.GetMaterialAsset();
		*outAssetID = materialAsset ? materialAsset->GetGUID() : GUID(0, 0);
	}

	void Script::Eagle_TextComponent_SetMaterial(GUID entityID, GUID assetID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set Text Component component material. Entity is null");
			return;
		}

		auto& component = entity.GetComponent<TextComponent>();

		if (assetID.IsNull())
		{
			component.SetMaterialAsset(nullptr);
			return;
		}

		Ref<Asset> asset;
		if (!AssetManager::Get(assetID, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set Text Component material. Couldn't find an asset");
			return;
		}

		Ref<AssetMaterial> materialAsset = Cast<AssetMaterial>(asset);
		if (!materialAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set Text Component material. Provided asset is not a material asset");
			return;
		}

		component.SetMaterialAsset(materialAsset);
	}

	//--------------Text2D Component--------------
	GUID Script::Eagle_Text2DComponent_GetFont(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get Text2DComponent font. Entity is null");
			return GUID(0, 0);
		}

		const auto& component = entity.GetComponent<Text2DComponent>();
		const auto& asset = component.GetFontAsset();
		return asset ? asset->GetGUID() : GUID(0, 0);
	}

	void Script::Eagle_Text2DComponent_SetFont(GUID entityID, GUID assetID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set Text2DComponent font. Entity is null");
			return;
		}

		auto& component = entity.GetComponent<Text2DComponent>();
		if (assetID.IsNull())
		{
			component.SetFontAsset(nullptr);
			return;
		}

		Ref<Asset> asset;
		if (!AssetManager::Get(assetID, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set Text2DComponent texture. Couldn't find an asset");
			return;
		}

		Ref<AssetFont> fontAsset = Cast<AssetFont>(asset);
		if (!fontAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set Text2DComponent texture. Provided asset is not a font asset");
			return;
		}

		component.SetFontAsset(fontAsset);
	}

	MonoString* Script::Eagle_Text2DComponent_GetText(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return mono_string_new(mono_domain_get(), entity.GetComponent<Text2DComponent>().GetText().c_str());
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get text of Text2D Component. Entity is null");
			return nullptr;
		}
	}

	void Script::Eagle_Text2DComponent_SetText(GUID entityID, MonoString* value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<Text2DComponent>().SetText(MonoStringHandler(value).c_str());
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set text of Text2D Component. Entity is null");
	}

	void Script::Eagle_Text2DComponent_GetColor(GUID entityID, glm::vec3* outColor)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			*outColor = entity.GetComponent<Text2DComponent>().GetColor();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get color of Text2D Component. Entity is null");
	}

	void Script::Eagle_Text2DComponent_SetColor(GUID entityID, const glm::vec3* color)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<Text2DComponent>().SetColor(*color);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set color of Text2D Component. Entity is null");
	}

	void Script::Eagle_Text2DComponent_GetTint(GUID entityID, glm::vec3* outTint)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			*outTint = entity.GetComponent<Text2DComponent>().GetTint();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get tint of Text2D Component. Entity is null");
	}

	void Script::Eagle_Text2DComponent_SetTint(GUID entityID, const glm::vec3* tint)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<Text2DComponent>().SetTint(*tint);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set tint of Text2D Component. Entity is null");
	}

	void Script::Eagle_Text2DComponent_GetAnchor(GUID entityID, glm::vec2* outValue)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			*outValue = entity.GetComponent<Text2DComponent>().GetAnchor();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get anchor of Text2D Component. Entity is null");
	}

	void Script::Eagle_Text2DComponent_SetAnchor(GUID entityID, const glm::vec2* value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<Text2DComponent>().SetAnchor(*value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set anchor of Text2D Component. Entity is null");
	}

	void Script::Eagle_Text2DComponent_GetPivot(GUID entityID, glm::vec2* outValue)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			*outValue = entity.GetComponent<Text2DComponent>().GetPivot();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get pivot of Text2D Component. Entity is null");
	}

	void Script::Eagle_Text2DComponent_SetPivot(GUID entityID, const glm::vec2* value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<Text2DComponent>().SetPivot(*value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set pivot of Text2D Component. Entity is null");
	}

	void Script::Eagle_Text2DComponent_GetOffset(GUID entityID, glm::vec2* outValue)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			*outValue = entity.GetComponent<Text2DComponent>().GetOffset();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get offset of Text2D Component. Entity is null");
	}

	void Script::Eagle_Text2DComponent_SetOffset(GUID entityID, const glm::vec2* value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<Text2DComponent>().SetOffset(*value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set offset of Text2D Component. Entity is null");
	}

	float Script::Eagle_Text2DComponent_GetFontSize(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<Text2DComponent>().GetFontSize();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get font size of Text2D Component. Entity is null");
			return 0.f;
		}
	}

	void Script::Eagle_Text2DComponent_SetFontSize(GUID entityID, float value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<Text2DComponent>().SetFontSize(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set font size of Text2D Component. Entity is null");
	}

	TextAlignment Script::Eagle_Text2DComponent_GetAlignment(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<Text2DComponent>().GetAlignment();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get alignment of Text2D Component. Entity is null");
			return TextAlignment::Left;
		}
	}

	void Script::Eagle_Text2DComponent_SetAlignment(GUID entityID, TextAlignment value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<Text2DComponent>().SetAlignment(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set alignment of Text2D Component. Entity is null");
	}

	float Script::Eagle_Text2DComponent_GetRotation(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<Text2DComponent>().GetRotation();

		EG_CORE_ERROR("[ScriptEngine] Couldn't get rotation of Text2D Component. Entity is null");
		return 0.f;
	}

	void Script::Eagle_Text2DComponent_SetRotation(GUID entityID, float value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<Text2DComponent>().SetRotation(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set rotation of Text2D Component. Entity is null");
	}

	float Script::Eagle_Text2DComponent_GetLineSpacing(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<Text2DComponent>().GetLineSpacing();

		EG_CORE_ERROR("[ScriptEngine] Couldn't get line spacing of Text2D Component. Entity is null");
		return 0.f;
	}

	void Script::Eagle_Text2DComponent_SetLineSpacing(GUID entityID, float value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<Text2DComponent>().SetLineSpacing(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set line spacing of Text2D Component. Entity is null");
	}

	float Script::Eagle_Text2DComponent_GetLetterSpacing(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<Text2DComponent>().GetLetterSpacing();

		EG_CORE_ERROR("[ScriptEngine] Couldn't get letter spacing of Text2D Component. Entity is null");
		return 0.f;
	}

	void Script::Eagle_Text2DComponent_SetLetterSpacing(GUID entityID, float value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<Text2DComponent>().SetLetterSpacing(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set letter spacing of Text2D Component. Entity is null");
	}

	float Script::Eagle_Text2DComponent_GetMaxWidth(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<Text2DComponent>().GetMaxWidth();

		EG_CORE_ERROR("[ScriptEngine] Couldn't get MaxWidth of Text2D Component. Entity is null");
		return 0.f;
	}
	
	void Script::Eagle_Text2DComponent_SetMaxWidth(GUID entityID, float value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<Text2DComponent>().SetMaxWidth(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set MaxWidth of Text2D Component. Entity is null");
	}

	void Script::Eagle_Text2DComponent_SetEffects(GUID entityID, const glm::vec4* outlineColor, float outlineWidth, const glm::vec4* glowColor, float glowSize,
		const glm::vec4* shadowColor, const glm::vec2* shadowOffset, float shadowSoftness, bool bOutline, bool bGlow, bool bShadow)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set effects of Text2D Component. Entity is null");
			return;
		}

		Text2DEffects effects;
		effects.OutlineColor = *outlineColor;
		effects.OutlineWidth = outlineWidth;
		effects.GlowColor = *glowColor;
		effects.GlowSize = glowSize;
		effects.ShadowColor = *shadowColor;
		effects.ShadowOffset = *shadowOffset;
		effects.ShadowSoftness = shadowSoftness;
		effects.bOutline = bOutline;
		effects.bGlow = bGlow;
		effects.bShadow = bShadow;
		entity.GetComponent<Text2DComponent>().SetEffects(effects);
	}

	void Script::Eagle_Text2DComponent_GetEffects(GUID entityID, glm::vec4* outOutlineColor, float* outOutlineWidth, glm::vec4* outGlowColor, float* outGlowSize,
		glm::vec4* outShadowColor, glm::vec2* outShadowOffset, float* outShadowSoftness, bool* outbOutline, bool* outbGlow, bool* outbShadow)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		const Text2DEffects effects = entity ? entity.GetComponent<Text2DComponent>().GetEffects() : Text2DEffects{};
		if (!entity)
			EG_CORE_ERROR("[ScriptEngine] Couldn't get effects of Text2D Component. Entity is null");

		*outOutlineColor = effects.OutlineColor;
		*outOutlineWidth = effects.OutlineWidth;
		*outGlowColor = effects.GlowColor;
		*outGlowSize = effects.GlowSize;
		*outShadowColor = effects.ShadowColor;
		*outShadowOffset = effects.ShadowOffset;
		*outShadowSoftness = effects.ShadowSoftness;
		*outbOutline = effects.bOutline;
		*outbGlow = effects.bGlow;
		*outbShadow = effects.bShadow;
	}

	float Script::Eagle_Text2DComponent_GetOpacity(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<Text2DComponent>().GetOpacity();

		EG_CORE_ERROR("[ScriptEngine] Couldn't get opacity of Text2D Component. Entity is null");
		return 1.f;
	}

	void Script::Eagle_Text2DComponent_SetOpacity(GUID entityID, float value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<Text2DComponent>().SetOpacity(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set opacity of Text2D Component. Entity is null");
	}

	void Script::Eagle_Text2DComponent_SetIsVisible(GUID entityID, bool value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<Text2DComponent>().SetIsVisible(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set 'IsVisible' of Text2D Component. Entity is null");
	}

	bool Script::Eagle_Text2DComponent_IsVisible(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<Text2DComponent>().IsVisible();

		EG_CORE_ERROR("[ScriptEngine] Couldn't get 'IsVisible' of Text2D Component. Entity is null");
		return true;
	}

	void Script::Eagle_Text2DComponent_SetPivotFollowsAnchor(GUID entityID, bool value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<Text2DComponent>().SetPivotFollowsAnchor(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set 'bPivotFollowsAnchor' of Text2D Component. Entity is null");
	}

	bool Script::Eagle_Text2DComponent_DoesPivotFollowAnchor(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<Text2DComponent>().DoesPivotFollowAnchor();

		EG_CORE_ERROR("[ScriptEngine] Couldn't get 'bPivotFollowsAnchor' of Text2D Component. Entity is null");
		return true;
	}

	bool Script::Eagle_Text2DComponent_IsHitTestable(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<Text2DComponent>().IsHitTestable();

		EG_CORE_ERROR("[ScriptEngine] Couldn't get 'bHitTestable' of Text2D Component. Entity is null");
		return false;
	}

	void Script::Eagle_Text2DComponent_SetIsHitTestable(GUID entityID, bool value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<Text2DComponent>().SetIsHitTestable(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set 'bHitTestable' of Text2D Component. Entity is null");
	}

	//--------------Image2D Component--------------
	void Script::Eagle_Image2DComponent_SetTexture(GUID entityID, GUID assetID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set Image2D component texture. Entity is null");
			return;
		}

		auto& component = entity.GetComponent<Image2DComponent>();
		if (assetID.IsNull())
		{
			component.SetTextureAsset(nullptr);
			return;
		}

		Ref<Asset> asset;
		if (!AssetManager::Get(assetID, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set Image2D component texture. Couldn't find an asset");
			return;
		}

		Ref<AssetTexture2D> textureAsset = Cast<AssetTexture2D>(asset);
		if (!textureAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set Image2D component texture. Provided asset is not a texture 2D asset");
			return;
		}

		component.SetTextureAsset(textureAsset);
	}

	GUID Script::Eagle_Image2DComponent_GetTexture(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get Image2D component texture. Entity is null");
			return GUID(0, 0);
		}

		const auto& component = entity.GetComponent<Image2DComponent>();
		const auto& asset = component.GetTextureAsset();
		return asset ? asset->GetGUID() : GUID(0, 0);
	}

	void Script::Eagle_Image2DComponent_GetTint(GUID entityID, glm::vec3* outValue)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			*outValue = entity.GetComponent<Image2DComponent>().GetTint();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get tint of Image2D Component. Entity is null");
	}

	void Script::Eagle_Image2DComponent_SetTint(GUID entityID, const glm::vec3* value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<Image2DComponent>().SetTint(*value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set tint of Image2D Component. Entity is null");
	}

	void Script::Eagle_Image2DComponent_GetAnchor(GUID entityID, glm::vec2* outValue)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			*outValue = entity.GetComponent<Image2DComponent>().GetAnchor();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get anchor of Image2D Component. Entity is null");
	}

	void Script::Eagle_Image2DComponent_SetAnchor(GUID entityID, const glm::vec2* value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<Image2DComponent>().SetAnchor(*value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set anchor of Image2D Component. Entity is null");
	}

	void Script::Eagle_Image2DComponent_GetPivot(GUID entityID, glm::vec2* outValue)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			*outValue = entity.GetComponent<Image2DComponent>().GetPivot();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get pivot of Image2D Component. Entity is null");
	}

	void Script::Eagle_Image2DComponent_SetPivot(GUID entityID, const glm::vec2* value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<Image2DComponent>().SetPivot(*value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set pivot of Image2D Component. Entity is null");
	}

	void Script::Eagle_Image2DComponent_GetOffset(GUID entityID, glm::vec2* outValue)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			*outValue = entity.GetComponent<Image2DComponent>().GetOffset();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get offset of Image2D Component. Entity is null");
	}

	void Script::Eagle_Image2DComponent_SetOffset(GUID entityID, const glm::vec2* value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<Image2DComponent>().SetOffset(*value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set offset of Image2D Component. Entity is null");
	}

	void Script::Eagle_Image2DComponent_GetSize(GUID entityID, glm::vec2* outValue)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			*outValue = entity.GetComponent<Image2DComponent>().GetSize();
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't get size of Image2D Component. Entity is null");
	}

	void Script::Eagle_Image2DComponent_SetSize(GUID entityID, const glm::vec2* value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<Image2DComponent>().SetSize(*value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set size of Image2D Component. Entity is null");
	}

	float Script::Eagle_Image2DComponent_GetRotation(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<Image2DComponent>().GetRotation();
			
		EG_CORE_ERROR("[ScriptEngine] Couldn't get rotation of Image2D Component. Entity is null");
		return 0.f;
	}

	void Script::Eagle_Image2DComponent_SetRotation(GUID entityID, float value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<Image2DComponent>().SetRotation(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set rotation of Image2D Component. Entity is null");
	}

	void Script::Eagle_Image2DComponent_SetOpacity(GUID entityID, float value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<Image2DComponent>().SetOpacity(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set opacity of Image2D Component. Entity is null");
	}

	float Script::Eagle_Image2DComponent_GetOpacity(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<Image2DComponent>().GetOpacity();

		EG_CORE_ERROR("[ScriptEngine] Couldn't get opacity of Image2D Component. Entity is null");
		return 0.f;
	}

	void Script::Eagle_Image2DComponent_SetIsVisible(GUID entityID, bool value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<Image2DComponent>().SetIsVisible(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set `IsVisible` of Image2D Component. Entity is null");
	}

	bool Script::Eagle_Image2DComponent_IsVisible(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<Image2DComponent>().IsVisible();

		EG_CORE_ERROR("[ScriptEngine] Couldn't get 'IsVisible' of Image2D Component. Entity is null");
		return false;
	}

	void Script::Eagle_Image2DComponent_SetPivotFollowsAnchor(GUID entityID, bool value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<Image2DComponent>().SetPivotFollowsAnchor(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set 'bPivotFollowsAnchor' of Image2D Component. Entity is null");
	}

	bool Script::Eagle_Image2DComponent_DoesPivotFollowAnchor(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<Image2DComponent>().DoesPivotFollowAnchor();

		EG_CORE_ERROR("[ScriptEngine] Couldn't get 'bPivotFollowsAnchor' of Image2D Component. Entity is null");
		return true;
	}

	bool Script::Eagle_Image2DComponent_IsHitTestable(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<Image2DComponent>().IsHitTestable();

		EG_CORE_ERROR("[ScriptEngine] Couldn't get 'bHitTestable' of Image2D Component. Entity is null");
		return false;
	}

	void Script::Eagle_Image2DComponent_SetIsHitTestable(GUID entityID, bool value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<Image2DComponent>().SetIsHitTestable(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set 'bHitTestable' of Image2D Component. Entity is null");
	}

	//--------------Billboard Component--------------
	void Script::Eagle_BillboardComponent_SetTexture(GUID entityID, GUID assetID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set billboard component texture. Entity is null");
			return;
		}

		auto& component = entity.GetComponent<BillboardComponent>();
		if (assetID.IsNull())
		{
			component.TextureAsset.reset();
			return;
		}

		Ref<Asset> asset;
		if (!AssetManager::Get(assetID, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set billboard component texture. Couldn't find an asset");
			return;
		}

		Ref<AssetTexture2D> textureAsset = Cast<AssetTexture2D>(asset);
		if (!textureAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set billboard component texture. Provided asset is not a texture 2D asset");
			return;
		}

		component.TextureAsset = textureAsset;
	}

	GUID Script::Eagle_BillboardComponent_GetTexture(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get billboard component texture. Entity is null");
			return GUID(0, 0);
		}

		const auto& component = entity.GetComponent<BillboardComponent>();
		return component.TextureAsset ? component.TextureAsset->GetGUID() : GUID(0, 0);
	}

	void Script::Eagle_BillboardComponent_SetVisible(GUID entityID, bool value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<BillboardComponent>().bVisible = value;
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't set `IsVisible` of Billboard Component. Entity is null");
	}

	bool Script::Eagle_BillboardComponent_IsVisible(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `IsVisible` of billboard component. Entity is null");
			return false;
		}

		return entity.GetComponent<BillboardComponent>().bVisible;
	}

	//--------------Sprite Component--------------
	void Script::Eagle_SpriteComponent_GetMaterial(GUID entityID, GUID* outAssetID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get static mesh component material. Entity is null");
			return;
		}

		const auto& component = entity.GetComponent<SpriteComponent>();
		const auto& materialAsset = component.GetMaterialAsset();
		*outAssetID = materialAsset ? materialAsset->GetGUID() : GUID(0, 0);
	}

	void Script::Eagle_SpriteComponent_SetMaterial(GUID entityID, GUID assetID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set sprite component material. Entity is null");
			return;
		}
		
		auto& component = entity.GetComponent<SpriteComponent>();
		if (assetID.IsNull())
		{
			component.SetMaterialAsset(nullptr);
			return;
		}

		Ref<Asset> asset;
		if (!AssetManager::Get(assetID, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set sprite component material. Couldn't find an asset");
			return;
		}

		Ref<AssetMaterial> materialAsset = Cast<AssetMaterial>(asset);
		if (!materialAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set sprite component material. Provided asset is not a material asset");
			return;
		}

		component.SetMaterialAsset(materialAsset);
	}

	void Script::Eagle_SpriteComponent_GetAtlasSpriteCoords(GUID entityID, glm::vec2* outValue)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get atlas sprite coords. Entity is null");
			return;
		}

		*outValue = entity.GetComponent<SpriteComponent>().GetAtlasSpriteCoords();
	}

	void Script::Eagle_SpriteComponent_SetAtlasSpriteCoords(GUID entityID, const glm::vec2* value)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set atlas sprite coords. Entity is null");
			return;
		}

		entity.GetComponent<SpriteComponent>().SetAtlasSpriteCoords(*value);
	}

	void Script::Eagle_SpriteComponent_GetAtlasSpriteSize(GUID entityID, glm::vec2* outValue)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get atlas sprite size. Entity is null");
			return;
		}

		*outValue = entity.GetComponent<SpriteComponent>().GetAtlasSpriteSize();
	}

	void Script::Eagle_SpriteComponent_SetAtlasSpriteSize(GUID entityID, const glm::vec2* value)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set atlas sprite size. Entity is null");
			return;
		}

		entity.GetComponent<SpriteComponent>().SetAtlasSpriteSize(*value);
	}

	void Script::Eagle_SpriteComponent_GetAtlasSpriteSizeCoef(GUID entityID, glm::vec2* outValue)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get atlas sprite size coef. Entity is null");
			return;
		}

		*outValue = entity.GetComponent<SpriteComponent>().GetAtlasSpriteSizeCoef();
	}

	void Script::Eagle_SpriteComponent_SetAtlasSpriteSizeCoef(GUID entityID, const glm::vec2* value)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set atlas sprite size coef. Entity is null");
			return;
		}

		entity.GetComponent<SpriteComponent>().SetAtlasSpriteSizeCoef(*value);
	}

	bool Script::Eagle_SpriteComponent_GetIsAtlas(GUID entityID)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetIsAtlas`. Entity is null");
			return false;
		}

		return entity.GetComponent<SpriteComponent>().IsAtlas();
	}

	void Script::Eagle_SpriteComponent_SetIsAtlas(GUID entityID, bool value)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetIsAtlas`. Entity is null");
			return;
		}

		entity.GetComponent<SpriteComponent>().SetIsAtlas(value);
	}

	bool Script::Eagle_SpriteComponent_DoesCastShadows(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<SpriteComponent>().DoesCastShadows();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `DoesCastShadows` of Sprite Component. Entity is null");
			return false;
		}
	}

	void Script::Eagle_SpriteComponent_SetCastsShadows(GUID entityID, bool value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<SpriteComponent>().SetCastsShadows(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetCastsShadows` of Sprite Component. Entity is null");
	}

	bool Script::Eagle_SpriteComponent_DoesReceiveDecals(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<SpriteComponent>().DoesReceiveDecals();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `DoesReceiveDecals` of Sprite Component. Entity is null");
			return false;
		}
	}

	void Script::Eagle_SpriteComponent_SetVisible(GUID entityID, bool value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<SpriteComponent>().SetVisible(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetVisible` of Sprite Component. Entity is null");
	}

	bool Script::Eagle_SpriteComponent_IsVisible(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<SpriteComponent>().IsVisible();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `IsVisible` of Sprite Component. Entity is null");
			return false;
		}
	}

	void Script::Eagle_SpriteComponent_SetReceivesDecals(GUID entityID, bool value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<SpriteComponent>().SetReceivesDecals(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetReceivesDecals` of Sprite Component. Entity is null");
	}

	//--------------Script Component--------------
	void Script::Eagle_ScriptComponent_SetScript(GUID entityID, void* type)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetScript`. Entity is null");
			return;
		}
		
		auto& sc = entity.GetComponent<ScriptComponent>();
		const bool bOldExist = ScriptEngine::ModuleExists(sc.ModuleName);
		if (bOldExist)
		{
			ScriptEngine::OnDestroyEntity(entity);
			ScriptEngine::RemoveEntityScript(entity);
		}

		if (type == nullptr)
		{
			sc.ModuleName.clear();
			return;
		}

		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);
		std::string moduleName = mono_type_get_name_full(monoType, MonoTypeNameFormat::MONO_TYPE_NAME_FORMAT_FULL_NAME);
		if (!ScriptEngine::ModuleExists(moduleName))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetScript`. '{}' is an invalid script", moduleName);
			return;
		}

		sc.ModuleName = std::move(moduleName);
		if (ScriptEngine::InstantiateEntityClass(entity))
			ScriptEngine::OnCreateEntity(entity);
	}

	MonoReflectionType* Script::Eagle_ScriptComponent_GetScriptType(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			auto& script = entity.GetComponent<ScriptComponent>();
			if (!ScriptEngine::ModuleExists(script.ModuleName))
				return nullptr;

			MonoType* monoType = mono_reflection_type_from_name(script.ModuleName.data(), s_AppAssemblyImage);
			MonoReflectionType* reflectionType = mono_type_get_object(mono_domain_get(), monoType);
			return reflectionType;
		}

		EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetScriptType`. Entity is null");
		return nullptr;
	}

	MonoObject* Script::Eagle_ScriptComponent_GetInstance(GUID entityID)
	{
		if (!entityID.IsNull())
			return ScriptEngine::GetEntityMonoObject(entityID);

		EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetInstance`. Entity is null");
		return nullptr;
	}

	//--------------SceneSequence Component--------------
	static SceneSequenceComponent* GetSceneSequenceComponent(const GUID& entityID, const char* functionName)
	{
		const auto& scene = Scene::GetCurrentScene();
		Entity entity = scene ? scene->GetEntityByGUID(entityID) : Entity::Null;
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call SceneSequenceComponent `{}`. Entity is null", functionName);
			return nullptr;
		}
		if (!entity.HasComponent<SceneSequenceComponent>())
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call SceneSequenceComponent `{}`. Entity '{}' has no Scene Sequence component", functionName, entity.GetName());
			return nullptr;
		}
		return &entity.GetComponent<SceneSequenceComponent>();
	}

	void Script::Eagle_SceneSequenceComponent_Play(GUID entityID)
	{
		SceneSequenceComponent* component = GetSceneSequenceComponent(entityID, "Play");
		if (!component)
			return;

		component->Play();
	}

	void Script::Eagle_SceneSequenceComponent_Pause(GUID entityID)
	{
		SceneSequenceComponent* component = GetSceneSequenceComponent(entityID, "Pause");
		if (!component)
			return;

		component->Pause();
	}

	void Script::Eagle_SceneSequenceComponent_Stop(GUID entityID)
	{
		SceneSequenceComponent* component = GetSceneSequenceComponent(entityID, "Stop");
		if (!component)
			return;

		component->Stop();
	}

	bool Script::Eagle_SceneSequenceComponent_IsPlaying(GUID entityID)
	{
		SceneSequenceComponent* component = GetSceneSequenceComponent(entityID, "IsPlaying");
		if (!component)
			return false;

		return component->IsPlaying();
	}

	void Script::Eagle_SceneSequenceComponent_SetTime(GUID entityID, float time)
	{
		SceneSequenceComponent* component = GetSceneSequenceComponent(entityID, "SetTime");
		if (!component)
			return;

		component->SetTime(time);
	}

	float Script::Eagle_SceneSequenceComponent_GetTime(GUID entityID)
	{
		SceneSequenceComponent* component = GetSceneSequenceComponent(entityID, "GetTime");
		if (!component)
			return 0.f;

		return component->GetTime();
	}

	float Script::Eagle_SceneSequenceComponent_GetDuration(GUID entityID)
	{
		SceneSequenceComponent* component = GetSceneSequenceComponent(entityID, "GetDuration");
		if (!component)
			return 0.f;

		return component->GetDuration();
	}

	void Script::Eagle_SceneSequenceComponent_SetPlayRate(GUID entityID, float rate)
	{
		SceneSequenceComponent* component = GetSceneSequenceComponent(entityID, "SetPlayRate");
		if (!component)
			return;

		component->SetPlayRate(rate);
	}

	float Script::Eagle_SceneSequenceComponent_GetPlayRate(GUID entityID)
	{
		SceneSequenceComponent* component = GetSceneSequenceComponent(entityID, "GetPlayRate");
		if (!component)
			return 0.f;

		return component->GetPlayRate();
	}

	void Script::Eagle_SceneSequenceComponent_SetDriveCamera(GUID entityID, bool value)
	{
		SceneSequenceComponent* component = GetSceneSequenceComponent(entityID, "SetDriveCamera");
		if (!component)
			return;

		component->SetDriveCameraAllowed(value);
	}

	bool Script::Eagle_SceneSequenceComponent_GetDriveCamera(GUID entityID)
	{
		SceneSequenceComponent* component = GetSceneSequenceComponent(entityID, "GetDriveCamera");
		if (!component)
			return false;

		return component->AllowedToDriveCamera();
	}

	void Script::Eagle_SceneSequenceComponent_SetAsset(GUID entityID, GUID assetID)
	{
		SceneSequenceComponent* component = GetSceneSequenceComponent(entityID, "SetAsset");
		if (!component)
			return;

		if (assetID.IsNull())
		{
			component->SetAsset(nullptr);
			return;
		}

		Ref<Asset> asset;
		if (!AssetManager::Get(assetID, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call SceneSequenceComponent `SetAsset`. Couldn't find an asset");
			return;
		}

		Ref<AssetSceneSequence> sequenceAsset = Cast<AssetSceneSequence>(asset);
		if (!sequenceAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call SceneSequenceComponent `SetAsset`. Provided asset is not a Scene Sequence asset");
			return;
		}

		component->SetAsset(sequenceAsset);
	}

	GUID Script::Eagle_SceneSequenceComponent_GetAsset(GUID entityID)
	{
		SceneSequenceComponent* component = GetSceneSequenceComponent(entityID, "GetAsset");
		if (!component)
			return GUID(0, 0);

		const auto& asset = component->GetAsset();
		return asset ? asset->GetGUID() : GUID(0, 0);
	}

	//--------------ParticleSystem Component--------------
	void Script::Eagle_ParticleSystemComponent_Spawn(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			entity.GetComponent<ParticleSystemComponent>().Spawn();
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `Spawn` of Particle System Component. Entity is null");
			return;
		}
	}

	void Script::Eagle_ParticleSystemComponent_Destroy(GUID entityID, bool bForceImmediateRemoval)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			entity.GetComponent<ParticleSystemComponent>().Destroy(bForceImmediateRemoval);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `Destroy` of Particle System Component. Entity is null");
			return;
		}
	}

	void Script::Eagle_ParticleSystemComponent_SetAsset(GUID entityID, GUID assetID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call ParticleSystemComponent `SetAsset`. Entity is null");
			return;
		}

		auto& component = entity.GetComponent<ParticleSystemComponent>();
		if (assetID.IsNull())
		{
			component.SetAsset(nullptr);
			return;
		}

		Ref<Asset> asset;
		if (!AssetManager::Get(assetID, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call ParticleSystemComponent `SetAsset`. Couldn't find an asset");
			return;
		}

		Ref<AssetParticleSystem> psAsset = Cast<AssetParticleSystem>(asset);
		if (!psAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call ParticleSystemComponent `SetAsset`. Provided asset is not a Particle System asset");
			return;
		}

		component.SetAsset(psAsset);

		// Check if we need to notify particle system auto destruction script
		if (entity.HasComponent<NativeScriptComponent>())
		{
			auto& scriptComp = entity.GetComponent<NativeScriptComponent>();
			if (PSAutoDestroyScript* script = scriptComp.As<PSAutoDestroyScript>())
			{
				script->OnAssetChanged();
			}
		}
	}

	GUID Script::Eagle_ParticleSystemComponent_GetAsset(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call ParticleSystemComponent `GetAsset`. Entity is null");
			return GUID(0, 0);
		}

		const auto& component = entity.GetComponent<ParticleSystemComponent>();
		const auto& asset = component.GetAsset();
		return asset ? asset->GetGUID() : GUID(0, 0);
	}

	void Script::Eagle_ParticleSystemComponent_DuplicatePose(GUID entityID, uint32_t emitterIndex, GUID compEntityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call ParticleSystemComponent `DuplicatePose`. Entity is null");
			return;
		}

		Entity skeletalEntity = scene->GetEntityByGUID(compEntityID);
		if (!skeletalEntity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call ParticleSystemComponent `DuplicatePose`. Failed to get Entity of SkeletalMeshComponent");
			return;
		}

		if (!skeletalEntity.HasComponent<SkeletalMeshComponent>())
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call ParticleSystemComponent `DuplicatePose`. SkeletalMeshComponent of entity is invalid: {}", skeletalEntity.GetName());
			return;
		}

		auto& ps = entity.GetComponent<ParticleSystemComponent>();
		if (emitterIndex >= ps.PerEmitterAnimData.size())
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call ParticleSystemComponent `DuplicatePose`. Emitter index is out of bounds! Index: {}; Emitters count: {}", emitterIndex, ps.PerEmitterAnimData.size());
			return;
		}

		const auto& emitter = ps.GetAsset()->GetEmitters()[emitterIndex];
		if (emitter.MeshAsset != skeletalEntity.GetComponent<SkeletalMeshComponent>().GetMeshAsset())
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call ParticleSystemComponent `DuplicatePose`. Source `Skeletal Mesh Component` and `Emitter` use different skeletal mesh assets! They must match.");
			return;
		}
		ps.PerEmitterAnimData[emitterIndex].SrcOfLastPose = skeletalEntity;
	}

	//--------------Decal Component--------------
	void Script::Eagle_DecalComponent_SetMaterial(GUID entityID, GUID assetID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (!entity)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set decal component material. Entity is null");
			return;
		}

		auto& component = entity.GetComponent<DecalComponent>();
		if (assetID.IsNull())
		{
			component.SetMaterialAsset(nullptr);
			return;
		}

		Ref<Asset> asset;
		if (!AssetManager::Get(assetID, &asset))
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set decal component material. Couldn't find an asset");
			return;
		}

		Ref<AssetMaterial> materialAsset = Cast<AssetMaterial>(asset);
		if (!materialAsset)
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set decal component material. Provided asset is not a material asset");
			return;
		}

		component.SetMaterialAsset(materialAsset);
	}

	GUID Script::Eagle_DecalComponent_GetMaterial(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			const auto& asset = entity.GetComponent<DecalComponent>().GetMaterialAsset();
			return asset ? asset->GetGUID() : GUID(0, 0);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get material of Decal Component. Entity is null");
			return GUID(0, 0);
		}
	}

	void Script::Eagle_DecalComponent_SetAdjustAspectRatioEnabled(GUID entityID, bool value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			entity.GetComponent<DecalComponent>().SetAdjustAspectRatioEnabled(value);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set `bAdjustAspectRatio` of Decal Component. Entity is null");
			return;
		}
	}

	bool Script::Eagle_DecalComponent_IsAdjustAspectRatioEnabled(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<DecalComponent>().IsAdjustAspectRatioEnabled();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `IsAdjustAspectRatioEnabled` of Decal Component. Entity is null");
			return false;
		}
	}

	void Script::Eagle_DecalComponent_SetSortPriority(GUID entityID, uint32_t value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			entity.GetComponent<DecalComponent>().SetSortPriority(value);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set `SortPriority` of Decal Component. Entity is null");
			return;
		}
	}

	uint32_t Script::Eagle_DecalComponent_GetSortPriority(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<DecalComponent>().GetSortPriority();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get `SortPriority` of Decal Component. Entity is null");
			return 0u;
		}
	}

	void Script::Eagle_DecalComponent_SetVisible(GUID entityID, bool value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			entity.GetComponent<DecalComponent>().SetVisible(value);
		else
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetVisible` of Decal Component. Entity is null");
	}

	bool Script::Eagle_DecalComponent_IsVisible(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
			return entity.GetComponent<DecalComponent>().IsVisible();
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `IsVisible` of Decal Component. Entity is null");
			return false;
		}
	}

	//--------------NavigationMesh Component--------------
	void Script::Eagle_NavigationMeshComponent_Build(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			scene->BuildNavMesh(&entity.GetComponent<NavigationMeshComponent>());
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `Build` of Navigation Mesh Component. Entity is null");
			return;
		}
	}

	void Script::Eagle_NavigationMeshComponent_SetCrowdSettings(GUID entityID, const AINavigation::CrowdSettings* settings)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			entity.GetComponent<NavigationMeshComponent>().SetCrowdSettings(*settings);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetCrowdSettings` of Navigation Mesh Component. Entity is null");
			return;
		}
	}

	void Script::Eagle_NavigationMeshComponent_GetCrowdSettings(GUID entityID, AINavigation::CrowdSettings* settings)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			*settings = entity.GetComponent<NavigationMeshComponent>().GetCrowdSettings();
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetCrowdSettings` of Navigation Mesh Component. Entity is null");
			return;
		}
	}

	void Script::Eagle_NavigationMeshComponent_SetSettings(GUID entityID, const AINavigation::MeshSettings* settings)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			entity.GetComponent<NavigationMeshComponent>().SetSettings(*settings);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetSettings` of Navigation Mesh Component. Entity is null");
			return;
		}
	}

	void Script::Eagle_NavigationMeshComponent_GetSettings(GUID entityID, AINavigation::MeshSettings* settings)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			*settings = entity.GetComponent<NavigationMeshComponent>().GetSettings();
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetSettings` of Navigation Mesh Component. Entity is null");
			return;
		}
	}

	void Script::Eagle_NavigationMeshComponent_SetAutoRebuild(GUID entityID, bool value)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			entity.GetComponent<NavigationMeshComponent>().bAutoRebuild = value;
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't set `bAutoRebuild` of Navigation Mesh Component. Entity is null");
			return;
		}
	}

	bool Script::Eagle_NavigationMeshComponent_GetAutoRebuild(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			return entity.GetComponent<NavigationMeshComponent>().bAutoRebuild;
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't get `bAutoRebuild` of Navigation Mesh Component. Entity is null");
			return false;
		}
	}

	//--------------NavigationCrowdAgent Component--------------
	void Script::Eagle_NavigationCrowdAgentComponent_TeleportAgent(GUID entityID, const glm::vec3* location)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			entity.GetComponent<NavigationCrowdAgentComponent>().TeleportAgent(*location);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `TeleportAgent` of Navigation Crowd Agent Component. Entity is null");
			return;
		}
	}

	void Script::Eagle_NavigationCrowdAgentComponent_SetMoveTarget(GUID entityID, const glm::vec3* location)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			entity.GetComponent<NavigationCrowdAgentComponent>().SetMoveTarget(*location);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetMoveTarget` of Navigation Crowd Agent Component. Entity is null");
			return;
		}
	}

	void Script::Eagle_NavigationCrowdAgentComponent_ResetMoveTarget(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			entity.GetComponent<NavigationCrowdAgentComponent>().ResetMoveTarget();
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `ResetMoveTarget` of Navigation Crowd Agent Component. Entity is null");
			return;
		}
	}

	bool Script::Eagle_NavigationCrowdAgentComponent_IsValid(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			return entity.GetComponent<NavigationCrowdAgentComponent>().IsValid();
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `IsValid` of Navigation Crowd Agent Component. Entity is null");
			return false;
		}
	}

	void Script::Eagle_NavigationCrowdAgentComponent_SetSettings(GUID entityID, const AINavigation::AgentSettings* settings)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			entity.GetComponent<NavigationCrowdAgentComponent>().SetSettings(*settings);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `SetSettings` of Navigation Crowd Agent Component. Entity is null");
			return;
		}
	}

	void Script::Eagle_NavigationCrowdAgentComponent_GetSettings(GUID entityID, AINavigation::AgentSettings* settings)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			*settings = entity.GetComponent<NavigationCrowdAgentComponent>().GetSettings();
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetSettings` of Navigation Crowd Agent Component. Entity is null");
			return;
		}
	}

	bool Script::Eagle_NavigationCrowdAgentComponent_GetLocation(GUID entityID, glm::vec3* location)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			return entity.GetComponent<NavigationCrowdAgentComponent>().GetLocation(location);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetLocation` of Navigation Crowd Agent Component. Entity is null");
			return false;
		}
	}

	bool Script::Eagle_NavigationCrowdAgentComponent_GetVelocity(GUID entityID, glm::vec3* velocity)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			return entity.GetComponent<NavigationCrowdAgentComponent>().GetVelocity(velocity);
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetVelocity` of Navigation Crowd Agent Component. Entity is null");
			return false;
		}
	}

	MoveRequestState Script::Eagle_NavigationCrowdAgentComponent_GetTargetState(GUID entityID)
	{
		auto& scene = Scene::GetCurrentScene();
		Entity entity = scene->GetEntityByGUID(entityID);
		if (entity)
		{
			return entity.GetComponent<NavigationCrowdAgentComponent>().GetAgentTargetState();
		}
		else
		{
			EG_CORE_ERROR("[ScriptEngine] Couldn't call `GetTargetState` of Navigation Crowd Agent Component. Entity is null");
			return MoveRequestState::DT_CROWDAGENT_TARGET_NONE;
		}
	}
}
