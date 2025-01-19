#include "egpch.h"
#include "MaterialAssetEditor.h"

#include "Eagle/Asset/Asset.h"
#include "Eagle/UI/UI.h"
#include "Eagle/Renderer/Material.h"
#include "Eagle/Components/Components.h"

namespace Eagle
{
	MaterialAssetEditor::MaterialAssetEditor(const Ref<AssetMaterial>& asset)
		: AssetEditor(true), m_Asset(asset)
	{
		m_Sphere = AssetManager::GetPreviewSphere();

		Entity entity = m_Scene->CreateEntity("MaterialAssetEditor");
		auto& sm = entity.AddComponent<StaticMeshComponent>();
		sm.SetMeshAsset(m_Sphere);
		sm.SetMaterialAsset(0, m_Asset);

		Transform tr{};
		tr.Rotation = glm::rotate(tr.Rotation.GetQuat(), glm::radians(-90.f), glm::vec3(1.f, 0.f, 0.f));
		sm.SetWorldTransform(tr);

		auto& camera = m_Scene->GetEditorCamera();
		camera.SetLocation(glm::vec3(0.f, 5.f, 15.f));
		camera.LookAt(glm::vec3(0, 0, 0));
		const glm::vec3 cameraDir = camera.GetForwardVector();

		const auto& aabb = m_Sphere->GetMesh()->GetAABB();
		const glm::vec3 center = aabb.Center();
		camera.SetLocation(center - cameraDir * aabb.MaxSide() * 5.f); // Move back
		camera.LookAt(center);
	}

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
		UI::Text("Type", "Material");

		Material::BlendMode blendMode = material->GetBlendMode();
		if (UI::ComboEnum("Blend Mode", blendMode, s_BlendModeHelpMsg))
		{
			material->SetBlendMode(blendMode);
			bChanged = true;
		}
		UI::EndPropertyGrid();

		Ref<AssetTexture2D> temp;

		// Albedo
		{
			UI::TextWithSeparator("Albedo");
			UI::BeginPropertyGrid("MaterialDetails_Albedo");
			
			temp = material->GetAlbedoAsset();
			if (UI::DrawAssetSelection("Albedo Texture", temp))
			{
				material->SetAlbedoAsset(temp);
				bChanged = true;
			}

			glm::vec3 value = material->GetAlbedo();
			if (UI::PropertyColor("Albedo Value", value, false))
			{
				material->SetAlbedo(value);
				bChanged = true;
			}

			bool bUseTexture = !material->IsRawAlbedoUsed();
			if (UI::Property("Use Texture", bUseTexture))
				material->SetRawAlbedoUsed(!bUseTexture);
			
			UI::EndPropertyGrid();
		}

		// Metalness
		{
			UI::TextWithSeparator("Metalness");
			UI::BeginPropertyGrid("MaterialDetails_Metalness");
			
			temp = material->GetMetalnessAsset();
			if (UI::DrawAssetSelection("Metalness Texture", temp, s_MetalnessHelpMsg))
			{
				material->SetMetalnessAsset(temp);
				bChanged = true;
			}

			float value = material->GetMetalness();
			if (UI::PropertySlider("Metalness Value", value, 0.f, 1.f, s_MetalnessHelpMsg))
			{
				material->SetMetalness(value);
				bChanged = true;
			}

			bool bUseTexture = !material->IsRawMetalnessUsed();
			if (UI::Property("Use Texture", bUseTexture))
				material->SetRawMetalnessUsed(!bUseTexture);
			
			UI::EndPropertyGrid();
		}

		// Normal
		{
			UI::TextWithSeparator("Normal");
			UI::BeginPropertyGrid("MaterialDetails_Normal");
			
			temp = material->GetNormalAsset();
			if (UI::DrawAssetSelection("Normal", temp, "Currently, decals ignore this input"))
			{
				material->SetNormalAsset(temp);
				bChanged = true;
			}
			
			UI::EndPropertyGrid();
		}

		// Roughness
		{
			UI::TextWithSeparator("Roughness");
			UI::BeginPropertyGrid("MaterialDetails_Roughness");
			
			temp = material->GetRoughnessAsset();
			if (UI::DrawAssetSelection("Roughness Texture", temp, s_RoughnessHelpMsg))
			{
				material->SetRoughnessAsset(temp);
				bChanged = true;
			}

			float value = material->GetRoughness();
			if (UI::PropertySlider("Roughness Value", value, 0.f, 1.f, s_RoughnessHelpMsg))
			{
				material->SetRoughness(value);
				bChanged = true;
			}

			bool bUseTexture = !material->IsRawRoughnessUsed();
			if (UI::Property("Use Texture", bUseTexture))
				material->SetRawRoughnessUsed(!bUseTexture);
			
			UI::EndPropertyGrid();
		}

		// AO
		{
			UI::TextWithSeparator("AO");
			UI::BeginPropertyGrid("MaterialDetails_AO");
			
			temp = material->GetAOAsset();
			if (UI::DrawAssetSelection("AO Texture", temp, s_AOHelpMsg))
			{
				material->SetAOAsset(temp);
				bChanged = true;
			}

			float value = material->GetAO();
			if (UI::PropertySlider("AO Value", value, 0.f, 1.f, s_AOHelpMsg))
			{
				material->SetAO(value);
				bChanged = true;
			}

			bool bUseTexture = !material->IsRawAOUsed();
			if (UI::Property("Use Texture", bUseTexture))
				material->SetRawAOUsed(!bUseTexture);
			
			UI::EndPropertyGrid();
		}

		// Emissive
		{
			UI::TextWithSeparator("Emissive");
			UI::BeginPropertyGrid("MaterialDetails_Emissive");
			
			temp = material->GetEmissiveAsset();
			if (UI::DrawAssetSelection("Emissive Texture", temp))
			{
				material->SetEmissiveAsset(temp);
				bChanged = true;
			}

			glm::vec3 value = material->GetEmissive();
			if (UI::PropertyColor("Emissive Value", value, false))
			{
				material->SetEmissive(value);
				bChanged = true;
			}

			bool bUseTexture = !material->IsRawEmissiveUsed();
			if (UI::Property("Use Texture", bUseTexture))
				material->SetRawEmissiveUsed(!bUseTexture);
			
			UI::EndPropertyGrid();
		}

		// Disable if not translucent
		{
			UI::TextWithSeparator("Opacity");

			const bool bTranslucent = blendMode == Material::BlendMode::Translucent;
			if (!bTranslucent)
				UI::PushItemDisabled();

			// Opacity
			{
				UI::BeginPropertyGrid("MaterialDetails_Opacity");
				
				temp = material->GetOpacityAsset();
				if (UI::DrawAssetSelection("Opacity Texture", temp, s_OpacityHelpMsg))
				{
					material->SetOpacityAsset(temp);
					bChanged = true;
				}

				float value = material->GetOpacity();
				if (UI::PropertySlider("Opacity Value", value, 0.f, 1.f, s_OpacityHelpMsg))
				{
					material->SetOpacity(value);
					bChanged = true;
				}

				bool bUseTexture = !material->IsRawOpacityUsed();
				if (UI::Property("Use Texture", bUseTexture))
					material->SetRawOpacityUsed(!bUseTexture);
				
				UI::EndPropertyGrid();
			}

			if (!bTranslucent)
				UI::PopItemDisabled();
		}

		// Disable if not masked
		{
			UI::TextWithSeparator("Opacity Mask");

			const bool bMasked = blendMode == Material::BlendMode::Masked;
			if (!bMasked)
				UI::PushItemDisabled();

			// Opacity Mask
			{
				UI::BeginPropertyGrid("MaterialDetails_Opacity Mask");

				temp = material->GetOpacityMaskAsset();
				if (UI::DrawAssetSelection("Opacity Mask Texture", temp, s_OpacityMaskHelpMsg))
				{
					material->SetOpacityMaskAsset(temp);
					bChanged = true;
				}

				float value = material->GetOpacityMask();
				if (UI::PropertySlider("Opacity Mask Value", value, 0.f, 1.f, s_OpacityMaskHelpMsg))
				{
					material->SetOpacityMask(value);
					bChanged = true;
				}

				bool bUseTexture = !material->IsRawOpacityMaskUsed();
				if (UI::Property("Use Texture", bUseTexture))
					material->SetRawOpacityMaskUsed(!bUseTexture);

				UI::EndPropertyGrid();
			}

			if (!bMasked)
				UI::PopItemDisabled();
		}

		UI::TextWithSeparator("Other");
		UI::BeginPropertyGrid("MaterialDetails");
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
		if (UI::PropertyDrag("Tiling Factor", tiling, 0.1f))
		{
			material->SetTilingFactor(tiling);
			bChanged = true;
		}

		UI::EndPropertyGrid();

		if (bChanged)
		{
			m_Asset->SetDirty(true);
			m_Asset->OnModified();
		}

		ImGui::Separator();
		ImGui::Separator();
		if (ImGui::Button("Save asset"))
			Asset::Save(m_Asset);

		ImGui::End();

		DrawViewport();
	}
}
