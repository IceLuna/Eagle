#include "egpch.h"
#include "MaterialAssetEditor.h"

#include "Eagle/Asset/Asset.h"
#include "Eagle/UI/UI.h"
#include "Eagle/Renderer/Material.h"

namespace Eagle
{
	void MaterialAssetEditor::OnImGuiRender(bool* pOpen)
	{
		static const char* s_MetalnessHelpMsg = "Controls how 'metal-like' surface looks like.\nDefault is 0";
		static const char* s_RoughnessHelpMsg = "Controls how rough surface looks like.\nRoughness of 0 is a mirror reflection and 1 is completely matte.\nDefault is 0.5";
		static const char* s_AOHelpMsg = "Can be used to affect how ambient lighting is applied to an object. If it's 0, ambient lighting won't affect it. Default is 1.0";
		static const char* s_BlendModeHelpMsg = "Translucent materials do not cast shadows!\nUse translucent materials with caution cause rendering them can be expensive";
		static const char* s_OpacityHelpMsg = "Controls the translucency of the material. 0 - fully transparent, 1 - fully opaque. Default is 0.5";
		static const char* s_OpacityMaskHelpMsg = "When in Masked mode, a material is either completely visible or completely invisible.\nValues below 0.5 are invisible";

		const auto& material = m_Asset->GetMaterial();
		bool bChanged = false;

		ImGui::SetNextWindowSize(ImVec2(720.f, 560.f), ImGuiCond_FirstUseEver);
		bool bHidden = !ImGui::Begin(m_Asset->GetPath().u8string().c_str(), pOpen);
		UI::BeginPropertyGrid("MaterialDetails");

		UI::Text("Name", m_Asset->GetPath().stem().u8string());

		Material::BlendMode blendMode = material->GetBlendMode();
		if (UI::ComboEnum("Blend Mode", blendMode, s_BlendModeHelpMsg))
		{
			material->SetBlendMode(blendMode);
			bChanged = true;
		}

		Ref<AssetTexture2D> temp = material->GetAlbedoAsset();
		if (UI::DrawAssetSelection("Albedo", temp))
		{
			material->SetAlbedoAsset(temp);
			bChanged = true;
		}

		temp = material->GetMetallnessAsset();
		if (UI::DrawAssetSelection("Metalness", temp, s_MetalnessHelpMsg))
		{
			material->SetMetallnessAsset(temp);
			bChanged = true;
		}

		temp = material->GetNormalAsset();
		if (UI::DrawAssetSelection("Normal", temp))
		{
			material->SetNormalAsset(temp);
			bChanged = true;
		}

		temp = material->GetRoughnessAsset();
		if (UI::DrawAssetSelection("Roughness", temp, s_RoughnessHelpMsg))
		{
			material->SetRoughnessAsset(temp);
			bChanged = true;
		}

		temp = material->GetAOAsset();
		if (UI::DrawAssetSelection("Ambient Occlusion", temp, s_AOHelpMsg))
		{
			material->SetAOAsset(temp);
			bChanged = true;
		}

		temp = material->GetEmissiveAsset();
		if (UI::DrawAssetSelection("Emissive Color", temp))
		{
			material->SetEmissiveAsset(temp);
			bChanged = true;
		}

		// Disable if not translucent
		{
			const bool bTranslucent = blendMode == Material::BlendMode::Translucent;
			if (!bTranslucent)
				UI::PushItemDisabled();

			temp = material->GetOpacityAsset();
			if (UI::DrawAssetSelection("Opacity", temp, s_OpacityHelpMsg))
			{
				material->SetOpacityAsset(temp);
				bChanged = true;
			}

			if (!bTranslucent)
				UI::PopItemDisabled();
		}

		// Disable if not masked
		{
			const bool bMasked = blendMode == Material::BlendMode::Masked;
			if (!bMasked)
				UI::PushItemDisabled();

			temp = material->GetOpacityMaskAsset();
			if (UI::DrawAssetSelection("Opacity Mask", temp, s_OpacityMaskHelpMsg))
			{
				material->SetOpacityMaskAsset(temp);
				bChanged = true;
			}

			if (!bMasked)
				UI::PopItemDisabled();
		}

		glm::vec3 emissiveIntensity = material->GetEmissiveIntensity();
		if (UI::PropertyColor("Emissive Intensity", emissiveIntensity, true, "HDR"))
		{
			material->SetEmissiveIntensity(emissiveIntensity);
			bChanged = true;
		}

		glm::vec4 tintColor = material->GetTintColor();
		if (UI::PropertyColor("Tint Color", tintColor, true))
		{
			material->SetTintColor(tintColor);
			bChanged = true;
		}

		float tiling = material->GetTilingFactor();
		if (UI::PropertySlider("Tiling Factor", tiling, 1.f, 128.f))
		{
			material->SetTilingFactor(tiling);
			bChanged = true;
		}

		UI::EndPropertyGrid();

		if (bChanged)
			m_Asset->SetDirty(true);

		ImGui::Separator();
		ImGui::Separator();
		if (ImGui::Button("Save asset"))
			Asset::Save(m_Asset);

		ImGui::End();
	}
}
