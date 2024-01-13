#include "EntityPropertiesPanel.h"

#include "Eagle/Asset/AssetManager.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

namespace Eagle
{
	static const char* s_MetalnessHelpMsg = "Controls how 'metal-like' surface looks like.\nDefault is 0";
	static const char* s_RoughnessHelpMsg = "Controls how rough surface looks like.\nRoughness of 0 is a mirror reflection and 1 is completely matte.\nDefault is 0.5";
	static const char* s_AOHelpMsg = "Can be used to affect how ambient lighting is applied to an object. If it's 0, ambient lighting won't affect it. Default is 1.0";
	static const char* s_TriggerHelpMsg = "Its role is to report that there has been an overlap with another shape.\nTrigger shapes play no part in the simulation of the scene";
	static const char* s_AttenuationRadiusHelpMsg = "Bounds of the light's visible influence.\nThis clamping of the light's influence is not physically correct but very important for performance";
	static const char* s_BlendModeHelpMsg = "Translucent materials do not cast shadows!\nUse translucent materials with caution cause rendering them can be expensive";
	static const char* s_OpacityHelpMsg = "Controls the translucency of the material. 0 - fully transparent, 1 - fully opaque. Default is 0.5";
	static const char* s_OpacityMaskHelpMsg = "When in Masked mode, a material is either completely visible or completely invisible.\nValues below 0.5 are invisible";
	static const char* s_CastsShadowsHelpMsg = "Translucent materials don't cast shadows unless 'Translucent shadows' feature is enabled. Translucent materials do not cast shadows on other translucent materials!";
	static const char* s_Text2DPosHelpMsg = "Normalized Device Coords. It's the position of the top left vertex of the first symbol\n"
		"Text2D will try to be at the same position of the screen no matter the resolution. Also it'll try to occupy the same amount of space\n"
		"(-1; -1) is the top left corner of the screen\n(0; 0) is the center\n(1; 1) is the bottom right corner of the screen";
	static const char* s_Image2DPosHelpMsg = "Normalized Device Coords. It's the position of the top left corner\n"
		"Image2D will try to be at the same position of the screen no matter the resolution. Also it'll try to occupy the same amount of space\n"
		"(-1; -1) is the top left corner of the screen\n(0; 0) is the center\n(1; 1) is the top bottom corner of the screen";
	static const char* s_IsVolumetricLightHelpMsg = "Note that it's performance intensive. For it to account for object interaction, light needs to cast shadows.\nIf you want to use it, enable volumetric light in Renderer Settings";
	static const char* s_TwoSidedMeshColliderHelpMsg = "Only affects non-convex mesh colliders.\nNon-convex meshes are one-sided meaning collision won't be registered from the back side. For example, that might be a problem for windows."
		" To fix it, set this flag";
	static const char* s_SpriteCoordsHelpMsg = "It's a sprite index within an atlas. For example, if an atlas is 128x128 and a sprite has a 32x32 size, and in case you want to select a sprite at 64x32, here you enter 2x1.";

#define AssetField_Case(type) \
	case FieldType::type:\
	{\
		GUID value = bRuntime ? field.GetRuntimeValue<GUID>(entityInstance) : field.GetStoredValue<GUID>();\
		Ref<Asset> asset;\
		Ref<type> castedAsset;\
		if (AssetManager::Get(value, &asset))\
			castedAsset = Cast<type>(asset);\
		if (UI::DrawAssetSelection(field.Name, castedAsset))\
		{\
			value = castedAsset ? castedAsset->GetGUID() : GUID(0, 0);\
			if (bRuntime)\
				field.SetRuntimeValue(entityInstance, value);\
			else\
				field.SetStoredValue(value);\
			bEntityChanged = true;\
		}\
		break;\
	}


	bool EntityPropertiesPanel::OnImGuiRender(Entity entity, bool bRuntime, bool bVolumetricsEnabled, bool bDrawWorldTransform)
	{
		this->bRuntime = bRuntime;
		this->bVolumetricsEnabled = bVolumetricsEnabled;
		this->bDrawWorldTransform = bDrawWorldTransform;
		m_Entity = entity;
		bEntityChanged = false;

		DrawComponents(entity);

		return bEntityChanged;
	}

	void EntityPropertiesPanel::DrawComponents(Entity& entity)
	{
		auto& entityName = entity.GetComponent<EntitySceneNameComponent>().Name;
		char buffer[256];
		memset(buffer, 0, sizeof(buffer));
		strncpy_s(buffer, entityName.c_str(), sizeof(buffer));

		ImGui::PushID(int(entity.GetID()));
		if (ImGui::InputText("##Name", buffer, sizeof(buffer)))
		{
			//TODO: Add Check for empty input
			entityName = std::string(buffer);
		}
		ImGui::PopID();
		
		ImGui::SameLine();
		ImGui::PushItemWidth(-1);

		if (ImGui::Button("Add"))
			ImGui::OpenPopup("AddComponent");

		if (ImGui::BeginPopup("AddComponent"))
		{
			UI::PushItemDisabled();
			ImGui::Separator();
			ImGui::Text("Basic");
			ImGui::Separator();
			UI::PopItemDisabled();
			DrawAddComponentMenuItem<ScriptComponent>("C# Script");
			DrawAddComponentMenuItem<CameraComponent>("Camera");
			DrawAddComponentMenuItem<SpriteComponent>("Sprite");
			DrawAddComponentMenuItem<StaticMeshComponent>("Static Mesh");
			DrawAddComponentMenuItem<BillboardComponent>("Billboard");
			DrawAddComponentMenuItem<TextComponent>("Text");
			DrawAddComponentMenuItem<Text2DComponent>("Text 2D");
			DrawAddComponentMenuItem<Image2DComponent>("Image 2D");

			UI::PushItemDisabled();
			ImGui::Separator();
			ImGui::Text("Audio");
			ImGui::Separator();
			UI::PopItemDisabled();
			DrawAddComponentMenuItem<AudioComponent>("Audio");
			DrawAddComponentMenuItem<ReverbComponent>("Reverb");

			UI::PushItemDisabled();
			ImGui::Separator();
			ImGui::Text("Physics");
			ImGui::Separator();
			UI::PopItemDisabled();
			DrawAddComponentMenuItem<RigidBodyComponent>("Rigid Body");
			DrawAddComponentMenuItem<BoxColliderComponent>("Box Collider");
			DrawAddComponentMenuItem<SphereColliderComponent>("Sphere Collider");
			DrawAddComponentMenuItem<CapsuleColliderComponent>("Capsule Collider");
			DrawAddComponentMenuItem<MeshColliderComponent>("Mesh Collider");

			UI::PushItemDisabled();
			ImGui::Separator();
			ImGui::Text("Lights");
			ImGui::Separator();
			UI::PopItemDisabled();
			DrawAddComponentMenuItem<PointLightComponent>("Point Light");
			DrawAddComponentMenuItem<DirectionalLightComponent>("Directional Light");
			DrawAddComponentMenuItem<SpotLightComponent>("Spot Light");

			ImGui::EndPopup();
		}
		ImGui::SameLine();
		ImGui::Text("GUID: %i", entity.GetID());

		ImGui::PopItemWidth();

		const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth
			| ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_AllowItemOverlap;

		ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();

		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
		ImGui::Separator();
		bool treeOpened = ImGui::TreeNodeEx((void*)(uint64_t)entity.GetID(), flags, "Components");
		ImGui::PopStyleVar();
		if (treeOpened)
		{
			ImGuiTreeNodeFlags childFlags = (m_SelectedComponent == SelectedComponent::None ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_OpenOnArrow
				| ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen;

			const std::string& entityName = entity.GetComponent<EntitySceneNameComponent>().Name;
			bool entityTreeOpened = ImGui::TreeNodeEx((void*)(typeid(Entity).hash_code() + typeid(Entity).hash_code()), childFlags, entityName.c_str());

			if (ImGui::IsItemClicked())
			{
				m_SelectedComponent = SelectedComponent::None;
			}
			
			if(entityTreeOpened)
			{
				if (DrawComponentLine<ScriptComponent>("C# Script", entity, m_SelectedComponent == SelectedComponent::Script))
				{
					m_SelectedComponent = SelectedComponent::Script;
				}
				if (DrawComponentLine<AudioComponent>("Audio", entity, m_SelectedComponent == SelectedComponent::AudioComponent))
				{
					m_SelectedComponent = SelectedComponent::AudioComponent;
				}
				if (DrawComponentLine<ReverbComponent>("Reverb", entity, m_SelectedComponent == SelectedComponent::ReverbComponent))
				{
					m_SelectedComponent = SelectedComponent::ReverbComponent;
				}
				if (DrawComponentLine<RigidBodyComponent>("Rigid Body", entity, m_SelectedComponent == SelectedComponent::RigidBody))
				{
					m_SelectedComponent = SelectedComponent::RigidBody;
				}
				if (DrawComponentLine<BoxColliderComponent>("Box Collider", entity, m_SelectedComponent == SelectedComponent::BoxCollider))
				{
					m_SelectedComponent = SelectedComponent::BoxCollider;
				}
				if (DrawComponentLine<SphereColliderComponent>("Sphere Collider", entity, m_SelectedComponent == SelectedComponent::SphereCollider))
				{
					m_SelectedComponent = SelectedComponent::SphereCollider;
				}
				if (DrawComponentLine<CapsuleColliderComponent>("Capsule Collider", entity, m_SelectedComponent == SelectedComponent::CapsuleCollider))
				{
					m_SelectedComponent = SelectedComponent::CapsuleCollider;
				}
				if (DrawComponentLine<MeshColliderComponent>("Mesh Collider", entity, m_SelectedComponent == SelectedComponent::MeshCollider))
				{
					m_SelectedComponent = SelectedComponent::MeshCollider;
				}
				if (DrawComponentLine<SpriteComponent>("Sprite", entity, m_SelectedComponent == SelectedComponent::Sprite))
				{
					m_SelectedComponent = SelectedComponent::Sprite;
				}
				if (DrawComponentLine<StaticMeshComponent>("Static Mesh", entity, m_SelectedComponent == SelectedComponent::StaticMesh))
				{
					m_SelectedComponent = SelectedComponent::StaticMesh;
				}
				if (DrawComponentLine<BillboardComponent>("Billboard", entity, m_SelectedComponent == SelectedComponent::Billboard))
				{
					m_SelectedComponent = SelectedComponent::Billboard;
				}
				if (DrawComponentLine<TextComponent>("Text", entity, m_SelectedComponent == SelectedComponent::Text3D))
				{
					m_SelectedComponent = SelectedComponent::Text3D;
				}
				if (DrawComponentLine<Text2DComponent>("Text 2D", entity, m_SelectedComponent == SelectedComponent::Text2D))
				{
					m_SelectedComponent = SelectedComponent::Text2D;
				}
				if (DrawComponentLine<Image2DComponent>("Image 2D", entity, m_SelectedComponent == SelectedComponent::Image2D))
				{
					m_SelectedComponent = SelectedComponent::Image2D;
				}
				if (DrawComponentLine<CameraComponent>("Camera", entity, m_SelectedComponent == SelectedComponent::Camera))
				{
					m_SelectedComponent = SelectedComponent::Camera;
				}
				if (DrawComponentLine<PointLightComponent>("Point Light", entity, m_SelectedComponent == SelectedComponent::PointLight))
				{
					m_SelectedComponent = SelectedComponent::PointLight;
				}
				if (DrawComponentLine<DirectionalLightComponent>("Directional Light", entity, m_SelectedComponent == SelectedComponent::DirectionalLight))
				{
					m_SelectedComponent = SelectedComponent::DirectionalLight;
				}
				if (DrawComponentLine<SpotLightComponent>("Spot Light", entity, m_SelectedComponent == SelectedComponent::SpotLight))
				{
					m_SelectedComponent = SelectedComponent::SpotLight;
				}
				ImGui::TreePop();
			}

			ImGui::TreePop();
		}

		if (m_SelectedComponent == SelectedComponent::None && entity.HasComponent<TransformComponent>())
		{
			DrawEntityTransformNode(entity);
		}
		switch (m_SelectedComponent)
		{
			case SelectedComponent::Sprite:
			{
				DrawComponentTransformNode(entity, entity.GetComponent<SpriteComponent>());
				DrawComponent<SpriteComponent>("Sprite", entity, [&entity, this](SpriteComponent& sprite)
				{
					bool bAtlas = sprite.IsAtlas();
					UI::BeginPropertyGrid("SpriteComponent");

					bool bCastsShadows = sprite.DoesCastShadows();

					if (UI::Property("Casts shadows", bCastsShadows, s_CastsShadowsHelpMsg))
					{
						sprite.SetCastsShadows(bCastsShadows);
						bEntityChanged = true;
					}

					if (UI::Property("Is Atlas", bAtlas))
					{
						sprite.SetIsAtlas(bAtlas);
						bEntityChanged = true;
					}
					
					if (bAtlas)
					{
						ImGui::Separator();

						glm::vec2 coords = sprite.GetAtlasSpriteCoords();
						glm::vec2 spriteSize = sprite.GetAtlasSpriteSize();
						glm::vec2 spriteSizeCoef = sprite.GetAtlasSpriteSizeCoef();

						if (UI::PropertyDrag("Sprite Coords", coords, 1.f, 0.f, 0.f, s_SpriteCoordsHelpMsg))
						{
							sprite.SetAtlasSpriteCoords(coords);
							bEntityChanged = true;
						}

						if (UI::PropertyDrag("Sprite Size", spriteSize, 1.f, 0.f, 0.f, "Size of a sprite within an atlas"))
						{
							sprite.SetAtlasSpriteSize(spriteSize);
							bEntityChanged = true;
						}

						if (UI::PropertyDrag("Sprite Size Coef", spriteSizeCoef, 1.f, 0.f, 0.f, "Some sprites might have different sizes within an atlas. If that's the case, you this coef to change the size"))
						{
							sprite.SetAtlasSpriteSizeCoef(spriteSizeCoef);
							bEntityChanged = true;
						}

						ImGui::Separator();
					}

					auto materialAsset = sprite.GetMaterialAsset();
					if (UI::DrawAssetSelection("Material", materialAsset))
					{
						sprite.SetMaterialAsset(materialAsset);
						bEntityChanged = true;
					}
						 
					UI::EndPropertyGrid();
				});
				break;
			}

			case SelectedComponent::StaticMesh:
			{
				DrawComponentTransformNode(entity, entity.GetComponent<StaticMeshComponent>());
				DrawComponent<StaticMeshComponent>("Static Mesh", entity, [&entity, this](StaticMeshComponent& smComponent)
				{
					UI::BeginPropertyGrid("StaticMeshComponent");
					Ref<AssetMesh> staticMesh = smComponent.GetMeshAsset();
					bool bCastsShadows = smComponent.DoesCastShadows();

					if (UI::DrawAssetSelection("Static Mesh", staticMesh))
					{
						smComponent.SetMeshAsset(staticMesh);
						bEntityChanged = true;
					}

					if (UI::Property("Casts shadows", bCastsShadows, s_CastsShadowsHelpMsg))
					{
						smComponent.SetCastsShadows(bCastsShadows);
						bEntityChanged = true;
					}

					ImGui::Separator();

					auto materialAsset = smComponent.GetMaterialAsset();
					if (UI::DrawAssetSelection("Material", materialAsset))
					{
						smComponent.SetMaterialAsset(materialAsset);
						bEntityChanged = true;
					}

					UI::EndPropertyGrid();
				});
				break;
			}

			case SelectedComponent::Billboard:
			{
				DrawComponentTransformNode(entity, entity.GetComponent<BillboardComponent>());
				DrawComponent<BillboardComponent>("Billboard", entity, [&entity, this](auto& billboard)
				{
					UI::BeginPropertyGrid("BillboardComponent");

					bEntityChanged |= UI::DrawAssetSelection("Texture", billboard.TextureAsset);

					UI::EndPropertyGrid();
				});
				break;
			}

			case SelectedComponent::Text3D:
			{
				DrawComponentTransformNode(entity, entity.GetComponent<TextComponent>());
				DrawComponent<TextComponent>("Text", entity, [&entity, this](TextComponent& component)
				{
					float lineSpacing = component.GetLineSpacing();
					float kerning = component.GetKerning();
					float maxWidth = component.GetMaxWidth();
					std::string text = component.GetText();
					bool bLit = component.IsLit();
					bool bCastsShadows = component.DoesCastShadows();
					Ref<AssetFont> asset = component.GetFontAsset();

					UI::BeginPropertyGrid("TextComponent");

					if (UI::DrawAssetSelection("Font", asset))
					{
						component.SetFontAsset(asset);
						bEntityChanged = true;
					}

					if (UI::PropertyTextMultiline("Text", text))
					{
						component.SetText(text);
						bEntityChanged = true;
					}

					if (UI::Property("Casts shadows", bCastsShadows, s_CastsShadowsHelpMsg))
					{
						component.SetCastsShadows(bCastsShadows);
						bEntityChanged = true;
					}

					if (UI::Property("Is Lit", bLit, "Should this text be affected by lighting?\nIf it is lit, 'Color' input is ignored and 'Albedo' & 'Emissive' are used instead"))
					{
						component.SetIsLit(bLit);
						bEntityChanged = true;
					}

					if (bLit)
					{
						glm::vec3 albedo = component.GetAlbedoColor();
						glm::vec3 emissive = component.GetEmissiveColor();
						float metallness = component.GetMetallness();
						float roughness = component.GetRoughness();
						float ao = component.GetAO();
						auto blendMode = component.GetBlendMode();

						if (UI::ComboEnum("Blend Mode", blendMode, s_BlendModeHelpMsg))
						{
							component.SetBlendMode(blendMode);
							bEntityChanged = true;
						}

						if (UI::PropertyColor("Albedo", albedo))
						{
							component.SetAlbedoColor(albedo);
							bEntityChanged = true;
						}

						if (UI::PropertyColor("Emissive Color", emissive, true, "HDR"))
						{
							component.SetEmissiveColor(emissive);
							bEntityChanged = true;
						}

						if (UI::PropertySlider("Metalness", metallness, 0.f, 1.f, s_MetalnessHelpMsg))
						{
							component.SetMetallness(metallness);
							bEntityChanged = true;
						}

						if (UI::PropertySlider("Roughness", roughness, 0.f, 1.f, s_RoughnessHelpMsg))
						{
							component.SetRoughness(roughness);
							bEntityChanged = true;
						}

						if (UI::PropertySlider("Ambient Occlusion", ao, 0.f, 1.f, s_AOHelpMsg))
						{
							component.SetAO(ao);
							bEntityChanged = true;
						}

						{
							const bool bTranslucent = blendMode == Material::BlendMode::Translucent;
							float opacity = component.GetOpacity();

							if (!bTranslucent)
								UI::PushItemDisabled();

							if (UI::PropertySlider("Opacity", opacity, 0.f, 1.f, s_OpacityHelpMsg))
							{
								component.SetOpacity(opacity);
								bEntityChanged = true;
							}

							if (!bTranslucent)
								UI::PopItemDisabled();
						}

						{
							const bool bMasked = blendMode == Material::BlendMode::Masked;
							float opacityMask = component.GetOpacityMask();

							if (!bMasked)
								UI::PushItemDisabled();

							if (UI::PropertySlider("Opacity Mask", opacityMask, 0.f, 1.f, s_OpacityMaskHelpMsg))
							{
								component.SetOpacityMask(opacityMask);
								bEntityChanged = true;
							}

							if (!bMasked)
								UI::PopItemDisabled();
						}
					}
					else
					{
						glm::vec3 color = component.GetColor();
						if (UI::PropertyColor("Color", color, true, "HDR"))
						{
							component.SetColor(color);
							bEntityChanged = true;
						}
					}
					
					if (UI::PropertyDrag("Line Spacing", lineSpacing, 0.1f))
					{
						component.SetLineSpacing(lineSpacing);
						bEntityChanged = true;
					}

					if (UI::PropertyDrag("Kerning", kerning, 0.1f))
					{
						component.SetKerning(kerning);
						bEntityChanged = true;
					}

					if (UI::PropertyDrag("Max Width", maxWidth, 0.1f))
					{
						component.SetMaxWidth(maxWidth);
						bEntityChanged = true;
					}

					UI::EndPropertyGrid();
				});
				break;
			}
			
			case SelectedComponent::Text2D:
			{
				DrawComponent<Text2DComponent>("Text 2D", entity, [&entity, this](Text2DComponent& component)
				{
					float lineSpacing = component.GetLineSpacing();
					float kerning = component.GetKerning();
					float maxWidth = component.GetMaxWidth();
					std::string text = component.GetText();
					Ref<AssetFont> asset = component.GetFontAsset();

					UI::BeginPropertyGrid("Text2DComponent");

					if (UI::DrawAssetSelection("Font", asset))
					{
						component.SetFontAsset(asset);
						bEntityChanged = true;
					}

					if (UI::PropertyTextMultiline("Text", text))
					{
						component.SetText(text);
						bEntityChanged = true;
					}

					glm::vec3 color = component.GetColor();
					if (UI::PropertyColor("Color", color, true))
					{
						component.SetColor(color);
						bEntityChanged = true;
					}

					glm::vec2 pos = component.GetPosition();
					if (UI::PropertyDrag("Position", pos, 0.01f, 0.f, 0.f, s_Text2DPosHelpMsg))
					{
						component.SetPosition(pos);
						bEntityChanged = true;
					}

					glm::vec2 scale = component.GetScale();
					if (UI::PropertyDrag("Scale", scale, 0.01f))
					{
						component.SetScale(scale);
						bEntityChanged = true;
					}

					float rotation = component.GetRotation();
					if (UI::PropertyDrag("Rotation", rotation, 1.f))
					{
						component.SetRotation(rotation);
						bEntityChanged = true;
					}

					float opacity = component.GetOpacity();
					if (UI::PropertyDrag("Opacity", opacity, 0.05f, 0.f, 1.f))
					{
						component.SetOpacity(opacity);
						bEntityChanged = true;
					}

					bool bVisible = component.IsVisible();
					if (UI::Property("Is Visible", bVisible))
					{
						component.SetIsVisible(bVisible);
						bEntityChanged = true;
					}
					
					if (UI::PropertyDrag("Line Spacing", lineSpacing, 0.1f))
					{
						component.SetLineSpacing(lineSpacing);
						bEntityChanged = true;
					}

					if (UI::PropertyDrag("Kerning", kerning, 0.1f))
					{
						component.SetKerning(kerning);
						bEntityChanged = true;
					}

					if (UI::PropertyDrag("Max Width", maxWidth, 0.1f))
					{
						component.SetMaxWidth(maxWidth);
						bEntityChanged = true;
					}

					UI::EndPropertyGrid();
				});
				break;
			}
			
			case SelectedComponent::Image2D:
			{
				DrawComponent<Image2DComponent>("Image 2D", entity, [&entity, this](Image2DComponent& component)
				{
					Ref<AssetTexture2D> asset = component.GetTextureAsset();

					UI::BeginPropertyGrid("Image2DComponent");

					if (UI::DrawAssetSelection("Texture", asset))
					{
						component.SetTextureAsset(asset);
						bEntityChanged = true;
					}

					glm::vec3 tint = component.GetTint();
					if (UI::PropertyColor("Tint", tint, true))
					{
						component.SetTint(tint);
						bEntityChanged = true;
					}

					glm::vec2 pos = component.GetPosition();
					if (UI::PropertyDrag("Position", pos, 0.01f, 0.f, 0.f, s_Image2DPosHelpMsg))
					{
						component.SetPosition(pos);
						bEntityChanged = true;
					}

					glm::vec2 scale = component.GetScale();
					if (UI::PropertyDrag("Scale", scale, 0.01f))
					{
						component.SetScale(scale);
						bEntityChanged = true;
					}

					float rotation = component.GetRotation();
					if (UI::PropertyDrag("Rotation", rotation, 1.f))
					{
						component.SetRotation(rotation);
						bEntityChanged = true;
					}

					float opacity = component.GetOpacity();
					if (UI::PropertyDrag("Opacity", opacity, 0.05f, 0.f, 1.f))
					{
						component.SetOpacity(opacity);
						bEntityChanged = true;
					}

					bool bVisible = component.IsVisible();
					if (UI::Property("Is Visible", bVisible))
					{
						component.SetIsVisible(bVisible);
						bEntityChanged = true;
					}

					UI::EndPropertyGrid();
				});
				break;
			}

			case SelectedComponent::Camera:
			{
				DrawComponentTransformNode(entity, entity.GetComponent<CameraComponent>());
				DrawComponent<CameraComponent>("Camera", entity, [&entity, this](CameraComponent& cameraComponent)
				{
					UI::BeginPropertyGrid("CameraComponent");
					auto& camera = cameraComponent.Camera;

					bEntityChanged |= UI::Property("Primary", cameraComponent.Primary);

					CameraProjectionMode projectionMode = camera.GetProjectionMode();
					if (UI::ComboEnum<CameraProjectionMode>("Projection", projectionMode))
					{
						camera.SetProjectionMode(projectionMode);
						bEntityChanged = true;
					}

					if (projectionMode == CameraProjectionMode::Perspective)
					{
						float verticalFov = glm::degrees(camera.GetPerspectiveVerticalFOV());
						if (UI::PropertyDrag("Vertical FOV", verticalFov))
						{
							camera.SetPerspectiveVerticalFOV(glm::radians(verticalFov));
							bEntityChanged = true;
						}

						float perspectiveNear = camera.GetPerspectiveNearClip();
						if (UI::PropertyDrag("Near Clip", perspectiveNear))
						{
							camera.SetPerspectiveNearClip(perspectiveNear);
							bEntityChanged = true;
						}

						float perspectiveFar = camera.GetPerspectiveFarClip();
						if (UI::PropertyDrag("Far Clip", perspectiveFar))
						{
							camera.SetPerspectiveFarClip(perspectiveFar);
							bEntityChanged = true;
						}
					}
					else
					{
						float size = camera.GetOrthographicSize();
						if (UI::PropertyDrag("Size", size))
						{
							camera.SetOrthographicSize(size);
							bEntityChanged = true;
						}

						float orthoNear = camera.GetOrthographicNearClip();
						if (UI::PropertyDrag("Near Clip", orthoNear))
						{
							camera.SetOrthographicNearClip(orthoNear);
							bEntityChanged = true;
						}

						float orthoFar = camera.GetOrthographicFarClip();
						if (UI::PropertyDrag("Far Clip", orthoFar))
						{
							camera.SetOrthographicFarClip(orthoFar);
							bEntityChanged = true;
						}

						bEntityChanged |= UI::Property("Fixed Aspect Ratio", cameraComponent.FixedAspectRatio);
					}

					float shadowFar = camera.GetShadowFarClip();
					if (UI::PropertyDrag("Shadow Far Clip", shadowFar, 1.f, 0.f, FLT_MAX, "Max distance for cascades (directional light shadows)"))
					{
						camera.SetShadowFarClip(shadowFar);
						bEntityChanged = true;
					}

					float cascadesSplitAlpha = camera.GetCascadesSplitAlpha();
					if (UI::PropertySlider("Cascades Split Alpha", cascadesSplitAlpha, 0.f, 1.f, "Used to determine how to split cascades for directional light shadows"))
					{
						camera.SetCascadesSplitAlpha(cascadesSplitAlpha);
						bEntityChanged = true;
					}

					float cascadesTransitionAlpha = camera.GetCascadesSmoothTransitionAlpha();
					if (UI::PropertySlider("Cascades Smooth Transition Alpha", cascadesTransitionAlpha, 0.f, 1.f, "The blend amount between cascades of directional light shadows (if smooth transition is enabled). Try to keep it as low as possible"))
					{
						camera.SetCascadesSmoothTransitionAlpha(cascadesTransitionAlpha);
						bEntityChanged = true;
					}
					
					UI::EndPropertyGrid();
				});
				break;
			}

			case SelectedComponent::PointLight:
			{
				DrawComponentTransformNode(entity, entity.GetComponent<PointLightComponent>());
				DrawComponent<PointLightComponent>("Point Light", entity, [&entity, this](PointLightComponent& pointLight)
				{
					glm::vec3 lightColor = pointLight.GetLightColor();
					float intensity = pointLight.GetIntensity();
					float fogIntensity = pointLight.GetVolumetricFogIntensity();
					float radius = pointLight.GetRadius();
					bool bAffectsWorld = pointLight.DoesAffectWorld();
					bool bCastsShadows = pointLight.DoesCastShadows();
					bool bVisualizeRadius = pointLight.VisualizeRadiusEnabled();
					bool bVolumetric = pointLight.IsVolumetricLight();

					UI::BeginPropertyGrid("PointLightComponent");
					if (UI::PropertyColor("Light Color", lightColor))
					{
						pointLight.SetLightColor(lightColor);
						bEntityChanged = true;
					}

					if (UI::PropertyDrag("Intensity", intensity, 0.1f, 0.f))
					{
						pointLight.SetIntensity(intensity);
						bEntityChanged = true;
					}

					if (UI::PropertyDrag("Attenuation Radius", radius, 0.1f, 0.f, 0.f, s_AttenuationRadiusHelpMsg))
					{
						pointLight.SetRadius(radius);
						bEntityChanged = true;
					}

					if (UI::Property("Affects World", bAffectsWorld))
					{
						pointLight.SetAffectsWorld(bAffectsWorld);
						bEntityChanged = true;
					}

					if (UI::Property("Casts shadows", bCastsShadows))
					{
						pointLight.SetCastsShadows(bCastsShadows);
						bEntityChanged = true;
					}

					if (UI::Property("Visualize Radius", bVisualizeRadius))
					{
						pointLight.SetVisualizeRadiusEnabled(bVisualizeRadius);
						bEntityChanged = true;
					}

					if (!bVolumetricsEnabled)
						UI::PushItemDisabled();

					if (UI::Property("Is Volumetric", bVolumetric, s_IsVolumetricLightHelpMsg))
					{
						pointLight.SetIsVolumetricLight(bVolumetric);
						bEntityChanged = true;
					}

					if (!bVolumetricsEnabled)
						UI::PopItemDisabled();

					if (UI::PropertyDrag("Volumetric Fog Intensity", fogIntensity, 0.1f, 0.f))
					{
						pointLight.SetVolumetricFogIntensity(fogIntensity);
						bEntityChanged = true;
					}
					UI::EndPropertyGrid();
				});
				break;
			}

			case SelectedComponent::DirectionalLight:
			{
				DrawComponentTransformNode(entity, entity.GetComponent<DirectionalLightComponent>());
				DrawComponent<DirectionalLightComponent>("Directional Light", entity, [&entity, this](DirectionalLightComponent& directionalLight)
				{
					glm::vec3 lightColor = directionalLight.GetLightColor();
					float intensity = directionalLight.GetIntensity();
					float fogIntensity = directionalLight.GetVolumetricFogIntensity();
					bool bAffectsWorld = directionalLight.DoesAffectWorld();
					bool bCastsShadows = directionalLight.DoesCastShadows();
					bool bVolumetric = directionalLight.IsVolumetricLight();

					UI::BeginPropertyGrid("DirectionalLightComponent");
					if (UI::PropertyColor("Light Color", lightColor))
					{
						directionalLight.SetLightColor(lightColor);
						bEntityChanged = true;
					}

					if (UI::PropertyDrag("Intensity", intensity, 0.1f, 0.f))
					{
						directionalLight.SetIntensity(intensity);
						bEntityChanged = true;
					}

					bEntityChanged |= UI::PropertyColor("Ambient", directionalLight.Ambient);
						
					if (UI::Property("Affects world", bAffectsWorld))
					{
						directionalLight.SetAffectsWorld(bAffectsWorld);
						bEntityChanged = true;
					}

					if (UI::Property("Casts shadows", bCastsShadows))
					{
						directionalLight.SetCastsShadows(bCastsShadows);
						bEntityChanged = true;
					}

					bEntityChanged |= UI::Property("Visualize direction", directionalLight.bVisualizeDirection);

					if (!bVolumetricsEnabled)
						UI::PushItemDisabled();

					if (UI::Property("Is Volumetric", bVolumetric, s_IsVolumetricLightHelpMsg))
					{
						directionalLight.SetIsVolumetricLight(bVolumetric);
						bEntityChanged = true;
					}

					if (!bVolumetricsEnabled)
						UI::PopItemDisabled();

					if (UI::PropertyDrag("Volumetric Fog Intensity", fogIntensity, 0.1f, 0.f))
					{
						directionalLight.SetVolumetricFogIntensity(fogIntensity);
						bEntityChanged = true;
					}

					UI::EndPropertyGrid();
				});
				break;
			}

			case SelectedComponent::SpotLight:
			{
				DrawComponentTransformNode(entity, entity.GetComponent<SpotLightComponent>());
				DrawComponent<SpotLightComponent>("Spot Light", entity, [&entity, this](SpotLightComponent& spotLight)
				{
					glm::vec3 lightColor = spotLight.GetLightColor();
					float intensity = spotLight.GetIntensity();
					float fogIntensity = spotLight.GetVolumetricFogIntensity();
					float inner = spotLight.GetInnerCutOffAngle();
					float outer = spotLight.GetOuterCutOffAngle();
					float distance = spotLight.GetDistance();
					bool bVisualizeDistance = spotLight.VisualizeDistanceEnabled();
					bool bAffectsWorld = spotLight.DoesAffectWorld();
					bool bCastsShadows = spotLight.DoesCastShadows();
					bool bVolumetric = spotLight.IsVolumetricLight();

					UI::BeginPropertyGrid("SpotLightComponent");
					if (UI::PropertyColor("Light Color", lightColor))
					{
						spotLight.SetLightColor(lightColor);
						bEntityChanged = true;
					}

					if (UI::PropertyDrag("Intensity", intensity, 0.1f, 0.f))
					{
						spotLight.SetIntensity(intensity);
						bEntityChanged = true;
					}

					if (UI::PropertyDrag("Attenuation Distance", distance, 0.1f, 0.f, 0.f, s_AttenuationRadiusHelpMsg))
					{
						spotLight.SetDistance(distance);
						bEntityChanged = true;
					}

					if (UI::PropertySlider("Inner Angle", inner, 1.f, 80.f))
					{
						spotLight.SetInnerCutOffAngle(inner);
						bEntityChanged = true;
					}

					if (UI::PropertySlider("Outer Angle", outer, 1.f, 80.f))
					{
						spotLight.SetOuterCutOffAngle(outer);
						bEntityChanged = true;
					}

					if (UI::Property("Affects world", bAffectsWorld))
					{
						spotLight.SetAffectsWorld(bAffectsWorld);
						bEntityChanged = true;
					}

					if (UI::Property("Casts shadows", bCastsShadows))
					{
						spotLight.SetCastsShadows(bCastsShadows);
						bEntityChanged = true;
					}

					if (UI::Property("Visualize Distance", bVisualizeDistance))
					{
						spotLight.SetVisualizeDistanceEnabled(bVisualizeDistance);
						bEntityChanged = true;
					}

					if (!bVolumetricsEnabled)
						UI::PushItemDisabled();

					if (UI::Property("Is Volumetric", bVolumetric, s_IsVolumetricLightHelpMsg))
					{
						spotLight.SetIsVolumetricLight(bVolumetric);
						bEntityChanged = true;
					}

					if (!bVolumetricsEnabled)
						UI::PopItemDisabled();

					if (UI::PropertyDrag("Volumetric Fog Intensity", fogIntensity, 0.1f, 0.f))
					{
						spotLight.SetVolumetricFogIntensity(fogIntensity);
						bEntityChanged = true;
					}

					UI::EndPropertyGrid();
				});
				break;
			}
		
			case SelectedComponent::Script:
			{
				DrawComponent<ScriptComponent>("C# Script", entity, [&entity, this](ScriptComponent& scriptComponent)
				{
					UI::BeginPropertyGrid("ScriptComponent");

					if (bRuntime)
						UI::PushItemDisabled();

					EntityInstance& entityInstance = ScriptEngine::GetEntityInstanceData(entity).Instance;
					bool bModuleExists = ScriptEngine::ModuleExists(scriptComponent.ModuleName);
					const auto& scriptClasses = ScriptEngine::GetScriptsNames();
					int newSelection = 0;
					int currentSelection = -1;
					for (int i = 0; i < scriptClasses.size(); ++i)
					{
						if (scriptComponent.ModuleName == scriptClasses[i])
						{
							currentSelection = i;
							break;
						}
					}

					if (!bModuleExists)
						UI::PushFrameBGColor({150.f, 0.f, 0.f, 255.f});

					if(UI::ComboWithNone("Script Class", currentSelection, scriptClasses, newSelection))
					{
						if (newSelection == -1)
							scriptComponent.ModuleName = "";
						else
							scriptComponent.ModuleName = scriptClasses[newSelection];

						ScriptEngine::InitEntityScript(entity);
						bEntityChanged = true;
					}
						
					if (!bModuleExists)
						UI::PopFrameBGColor();

					if (bRuntime)
						UI::PopItemDisabled();

					ImGui::Separator();
					if (ScriptEngine::ModuleExists(scriptComponent.ModuleName))
					{
						for (auto& it : scriptComponent.PublicFields)
						{
							auto& field = it.second;
							switch (field.Type)
							{
								case FieldType::Int:
								case FieldType::UnsignedInt:
								{
									int value = bRuntime ? field.GetRuntimeValue<int>(entityInstance) : field.GetStoredValue<int>();
									if (UI::PropertyDrag(field.Name.c_str(), value))
									{
										if (bRuntime)
											field.SetRuntimeValue(entityInstance, value);
										else
											field.SetStoredValue(value);
										bEntityChanged = true;
									}
									break;
								}
								case FieldType::Float:
								{
									float value = bRuntime ? field.GetRuntimeValue<float>(entityInstance) : field.GetStoredValue<float>();
									if (UI::PropertyDrag(field.Name.c_str(), value))
									{
										if (bRuntime)
											field.SetRuntimeValue(entityInstance, value);
										else
											field.SetStoredValue(value);
										bEntityChanged = true;
									}
									break;
								}
								case FieldType::String:
								{
									std::string value = bRuntime ? field.GetRuntimeValue<std::string>(entityInstance) : field.GetStoredValue<const std::string&>();
									if (UI::PropertyText(field.Name.c_str(), value))
									{
										if (bRuntime)
											field.SetRuntimeValue<std::string>(entityInstance, value);
										else
											field.SetStoredValue<std::string>(value);
										bEntityChanged = true;
									}
									break;
								}
								case FieldType::Vec2:
								{
									glm::vec2 value = bRuntime ? field.GetRuntimeValue<glm::vec2>(entityInstance) : field.GetStoredValue<glm::vec2>();
									if (UI::PropertyDrag(field.Name.c_str(), value))
									{
										if (bRuntime)
											field.SetRuntimeValue(entityInstance, value);
										else
											field.SetStoredValue(value);
										bEntityChanged = true;
									}
									break;
								}
								case FieldType::Vec3:
								{
									glm::vec3 value = bRuntime ? field.GetRuntimeValue<glm::vec3>(entityInstance) : field.GetStoredValue<glm::vec3>();
									if (UI::PropertyDrag(field.Name.c_str(), value))
									{
										if (bRuntime)
											field.SetRuntimeValue(entityInstance, value);
										else
											field.SetStoredValue(value);
										bEntityChanged = true;
									}
									break;
								}
								case FieldType::Vec4:
								{
									glm::vec4 value = bRuntime ? field.GetRuntimeValue<glm::vec4>(entityInstance) : field.GetStoredValue<glm::vec4>();
									if (UI::PropertyDrag(field.Name.c_str(), value))
									{
										if (bRuntime)
											field.SetRuntimeValue(entityInstance, value);
										else
											field.SetStoredValue(value);
										bEntityChanged = true;
									}
									break;
								}
								case FieldType::Bool:
								{
									bool value = bRuntime ? field.GetRuntimeValue<bool>(entityInstance) : field.GetStoredValue<bool>();
									if (UI::Property(field.Name.c_str(), value))
									{
										if (bRuntime)
											field.SetRuntimeValue(entityInstance, value);
										else
											field.SetStoredValue(value);
										bEntityChanged = true;
									}
									break;
								}
								case FieldType::Color3:
								{
									glm::vec3 value = bRuntime ? field.GetRuntimeValue<glm::vec3>(entityInstance) : field.GetStoredValue<glm::vec3>();
									if (UI::PropertyColor(field.Name.c_str(), value, true))
									{
										if (bRuntime)
											field.SetRuntimeValue(entityInstance, value);
										else
											field.SetStoredValue(value);
										bEntityChanged = true;
									}
									break;
								}
								case FieldType::Color4:
								{
									glm::vec4 value = bRuntime ? field.GetRuntimeValue<glm::vec4>(entityInstance) : field.GetStoredValue<glm::vec4>();
									if (UI::PropertyColor(field.Name.c_str(), value, true))
									{
										if (bRuntime)
											field.SetRuntimeValue(entityInstance, value);
										else
											field.SetStoredValue(value);
										bEntityChanged = true;
									}
									break;
								}
								case FieldType::Enum:
								{
									int value = bRuntime ? field.GetRuntimeValue<int>(entityInstance) : field.GetStoredValue<int>();
									if (UI::Combo(field.Name, value, field.EnumFields, value))
									{
										if (bRuntime)
											field.SetRuntimeValue(entityInstance, value);
										else
											field.SetStoredValue(value);
										bEntityChanged = true;
									}
									break;
								}
								AssetField_Case(Asset);
								AssetField_Case(AssetTexture2D);
								AssetField_Case(AssetTextureCube);
								AssetField_Case(AssetMesh);
								AssetField_Case(AssetAudio);
								AssetField_Case(AssetSoundGroup);
								AssetField_Case(AssetFont);
								AssetField_Case(AssetMaterial);
								AssetField_Case(AssetPhysicsMaterial);
								AssetField_Case(AssetEntity);
								AssetField_Case(AssetScene);
							}
						}
					}

					UI::EndPropertyGrid();
				});
				break;
			}
		
			case SelectedComponent::RigidBody:
			{
				bool bCanRemove = !entity.HasAny<BoxColliderComponent, SphereColliderComponent, CapsuleColliderComponent, MeshColliderComponent>();
				DrawComponent<RigidBodyComponent>("Rigid Body", entity, [&entity, this](RigidBodyComponent& rigidBody)
				{
					static const std::vector<std::string> lockStrings = { "X", "Y", "Z" };
						
					UI::BeginPropertyGrid("RigidBodyComponent");

					if (bRuntime)
						UI::PushItemDisabled();

					bEntityChanged |= UI::ComboEnum<RigidBodyComponent::Type>("Body type", rigidBody.BodyType);

					if (bRuntime)
						UI::PopItemDisabled();
						
					if (rigidBody.BodyType == RigidBodyComponent::Type::Dynamic)
					{
						float mass = rigidBody.GetMass();
						float linearDamping = rigidBody.GetLinearDamping();
						float angularDamping = rigidBody.GetAngularDamping();
						float maxLinearVelocity = rigidBody.GetMaxLinearVelocity();
						float maxAngularVelocity = rigidBody.GetMaxAngularVelocity();
						bool bEnableGravity = rigidBody.IsGravityEnabled();
						bool bKinematic = rigidBody.IsKinematic();
						const ActorLockFlag lockFlags = rigidBody.GetLockFlags();
						bool bLockPositions[3] = { HasFlags(lockFlags, ActorLockFlag::PositionX), HasFlags(lockFlags, ActorLockFlag::PositionY), HasFlags(lockFlags, ActorLockFlag::PositionZ) };
						bool bLockRotations[3] = { HasFlags(lockFlags, ActorLockFlag::RotationX), HasFlags(lockFlags, ActorLockFlag::RotationY), HasFlags(lockFlags, ActorLockFlag::RotationZ) };

						if (bRuntime)
							UI::PushItemDisabled();
							
						bEntityChanged |= UI::ComboEnum<RigidBodyComponent::CollisionDetectionType>("Collision Detection", rigidBody.CollisionDetection,
							"When continuous collision detection (or CCD) is turned on, the affected rigid bodies will not go through other objects at high velocities (a problem also known as tunnelling)."
							"A cheaper but less robust approach is called speculative CCD");
							
						if (bRuntime)
							UI::PopItemDisabled();
							
						if (UI::PropertyDrag("Mass", mass, 0.1f))
						{
							rigidBody.SetMass(mass);
							bEntityChanged = true;
						}

						if (UI::PropertyDrag("Linear Damping", linearDamping, 0.1f))
						{
							rigidBody.SetLinearDamping(linearDamping);
							bEntityChanged = true;
						}

						if (UI::PropertyDrag("Angular Damping", angularDamping, 0.1f))
						{
							rigidBody.SetAngularDamping(angularDamping);
							bEntityChanged = true;
						}

						if (UI::PropertyDrag("Max Linear Velocity", maxLinearVelocity, 0.1f))
						{
							rigidBody.SetMaxLinearVelocity(maxLinearVelocity);
							bEntityChanged = true;
						}

						if (UI::PropertyDrag("Max Angular Velocity", maxAngularVelocity, 0.1f))
						{
							rigidBody.SetMaxAngularVelocity(maxAngularVelocity);
							bEntityChanged = true;
						}

						if (UI::Property("Enable Gravity", bEnableGravity))
						{
							rigidBody.SetEnableGravity(bEnableGravity);
							bEntityChanged = true;
						}

						if (UI::Property("Is Kinematic", bKinematic, "Sometimes controlling an actor using forces or constraints is not sufficiently robust, precise or flexible."
							" For example moving platforms or character controllers often need to manipulate an actor's position or have"
							" it exactly follow a specific path. Such a control scheme is provided by kinematic actors."))
						{
							rigidBody.SetIsKinematic(bKinematic);
							bEntityChanged = true;
						}
						if (UI::Property("Lock Position", lockStrings, bLockPositions))
						{
							rigidBody.SetLockFlag(ActorLockFlag::PositionX, bLockPositions[0]);
							rigidBody.SetLockFlag(ActorLockFlag::PositionY, bLockPositions[1]);
							rigidBody.SetLockFlag(ActorLockFlag::PositionZ, bLockPositions[2]);
							bEntityChanged = true;
						}
						if (UI::Property("Lock Rotation", lockStrings, bLockRotations))
						{
							rigidBody.SetLockFlag(ActorLockFlag::RotationX, bLockRotations[0]);
							rigidBody.SetLockFlag(ActorLockFlag::RotationY, bLockRotations[1]);
							rigidBody.SetLockFlag(ActorLockFlag::RotationZ, bLockRotations[2]);
							bEntityChanged = true;
						}
					}
					UI::EndPropertyGrid();
				}, bCanRemove);
				break;
			}

			case SelectedComponent::BoxCollider:
			{
				DrawComponentTransformNode(entity, entity.GetComponent<BoxColliderComponent>());
				DrawComponent<BoxColliderComponent>("Box Collider", entity, [&entity, this](BoxColliderComponent& collider)
				{
					UI::BeginPropertyGrid("BoxColliderComponent");

					Ref<AssetPhysicsMaterial> materialAsset = collider.GetPhysicsMaterialAsset();
					glm::vec3 size = collider.GetSize();
					bool bTrigger = collider.IsTrigger();
					bool bShowCollision = collider.IsCollisionVisible();

					if (UI::DrawAssetSelection("Physics Material", materialAsset))
					{
						collider.SetPhysicsMaterialAsset(materialAsset);
						bEntityChanged = true;
					}

					if (UI::Property("Is Trigger", bTrigger, s_TriggerHelpMsg))
					{
						collider.SetIsTrigger(bTrigger);
						bEntityChanged = true;
					}

					if (UI::PropertyDrag("Size", size, 0.05f))
					{
						collider.SetSize(size);
						bEntityChanged = true;
					}

					if (UI::Property("Is Collision Visible", bShowCollision))
					{
						collider.SetShowCollision(bShowCollision);
						bEntityChanged = true;
					}

					UI::EndPropertyGrid();
				});
				break;
			}

			case SelectedComponent::SphereCollider:
			{
				DrawComponentTransformNode(entity, entity.GetComponent<SphereColliderComponent>());
				DrawComponent<SphereColliderComponent>("Sphere Collider", entity, [&entity, this](SphereColliderComponent& collider)
				{
					UI::BeginPropertyGrid("SphereColliderComponent");

					Ref<AssetPhysicsMaterial> materialAsset = collider.GetPhysicsMaterialAsset();
					float radius = collider.GetRadius();
					bool bTrigger = collider.IsTrigger();
					bool bShowCollision = collider.IsCollisionVisible();

					if (UI::DrawAssetSelection("Physics Material", materialAsset))
					{
						collider.SetPhysicsMaterialAsset(materialAsset);
						bEntityChanged = true;
					}
						
					if (UI::Property("Is Trigger", bTrigger, s_TriggerHelpMsg))
					{
						collider.SetIsTrigger(bTrigger);
						bEntityChanged = true;
					}

					if (UI::PropertyDrag("Radius", radius, 0.5f))
					{
						collider.SetRadius(radius);
						bEntityChanged = true;
					}

					if (UI::Property("Is Collision Visible", bShowCollision))
					{
						collider.SetShowCollision(bShowCollision);
						bEntityChanged = true;
					}
						
					UI::EndPropertyGrid();
				});
				break;
			}

			case SelectedComponent::CapsuleCollider:
			{
				DrawComponentTransformNode(entity, entity.GetComponent<CapsuleColliderComponent>());
				DrawComponent<CapsuleColliderComponent>("Capsule Collider", entity, [&entity, this](CapsuleColliderComponent& collider)
				{
					UI::BeginPropertyGrid("CapsuleColliderComponent");

					Ref<AssetPhysicsMaterial> materialAsset = collider.GetPhysicsMaterialAsset();
					float height = collider.GetHeight();
					float radius = collider.GetRadius();
					bool bTrigger = collider.IsTrigger();
					bool bShowCollision = collider.IsCollisionVisible();

					if (UI::DrawAssetSelection("Physics Material", materialAsset))
					{
						collider.SetPhysicsMaterialAsset(materialAsset);
						bEntityChanged = true;
					}

					if (UI::Property("Is Trigger", bTrigger, s_TriggerHelpMsg))
					{
						collider.SetIsTrigger(bTrigger);
						bEntityChanged = true;
					}

					if (UI::PropertyDrag("Radius", radius, 0.05f))
					{
						collider.SetRadius(radius);
						bEntityChanged = true;
					}

					if (UI::PropertyDrag("Height", height, 0.05f))
					{
						collider.SetHeight(height);
						bEntityChanged = true;
					}

					if (UI::Property("Is Collision Visible", bShowCollision))
					{
						collider.SetShowCollision(bShowCollision);
						bEntityChanged = true;
					}

					UI::EndPropertyGrid();
				});
				break;
			}

			case SelectedComponent::MeshCollider:
			{
				DrawComponentTransformNode(entity, entity.GetComponent<MeshColliderComponent>());
				DrawComponent<MeshColliderComponent>("Mesh Collider", entity, [&entity, this](MeshColliderComponent& collider)
				{
					UI::BeginPropertyGrid("MeshColliderComponent");

					Ref<AssetPhysicsMaterial> materialAsset = collider.GetPhysicsMaterialAsset();
					Ref<AssetMesh> collisionMesh = collider.GetCollisionMeshAsset();
					bool bTrigger = collider.IsTrigger();
					bool bShowCollision = collider.IsCollisionVisible();
					bool bConvex = collider.IsConvex();
					bool bTwoSided = collider.IsTwoSided();

					if (UI::DrawAssetSelection("Collision Mesh", collisionMesh, "Must be set. Set the mesh that will be used to generate collision data for it"))
					{
						collider.SetCollisionMeshAsset(collisionMesh);
						bEntityChanged = true;
					}

					if (UI::DrawAssetSelection("Physics Material", materialAsset))
					{
						collider.SetPhysicsMaterialAsset(materialAsset);
						bEntityChanged = true;
					}

					if (UI::Property("Is Trigger", bTrigger, s_TriggerHelpMsg))
					{
						collider.SetIsTrigger(bTrigger);
						bEntityChanged = true;
					}

					if (UI::Property("Is Collision Visible", bShowCollision))
					{
						collider.SetShowCollision(bShowCollision);
						bEntityChanged = true;
					}

					if (UI::Property("Is Convex", bConvex, "Generates collision around the mesh.\nNon-convex mesh collider can be used only\nwith kinematic or static actors."))
					{
						collider.SetIsConvex(bConvex);
						bEntityChanged = true;
					}

					if (UI::Property("Is Two-Sided", bTwoSided, s_TwoSidedMeshColliderHelpMsg))
					{
						collider.SetIsTwoSided(bTwoSided);
						bEntityChanged = true;
					}

					UI::EndPropertyGrid();
				});
				break;
			}
		
			case SelectedComponent::AudioComponent:
			{
				DrawComponentTransformNode(entity, entity.GetComponent<AudioComponent>());
				DrawComponent<AudioComponent>("Audio", entity, [&entity, this](AudioComponent& audio)
				{
					static const std::vector<std::string> rollOffModels = { "Linear", "Inverse", "Linear Square", "Inverse Tapered" };
					static const std::vector<std::string> rollOffToolTips = 
					{ 
						"This sound will follow a linear rolloff model where MinDistance = full volume, MaxDistance = silence",
						"This sound will follow the inverse rolloff model where MinDistance = full volume, MaxDistance = where sound stops attenuating, and rolloff is fixed according to the global rolloff factor",
						"This sound will follow a linear-square rolloff model where MinDistance = full volume, MaxDistance = silence",
						"This sound will follow the inverse rolloff model at distances close to MinDistance and a linear-square rolloff close to MaxDistance" 
					};

					int inSelectedRollOffModel = 0;
					UI::BeginPropertyGrid("AudioComponent");

					Ref<AssetAudio> asset = audio.GetAudioAsset();
					float volume = audio.GetVolume();
					int loopCount = audio.GetLoopCount();
					bool bLooping = audio.IsLooping();
					bool bMuted = audio.IsMuted();
					bool bStreaming = audio.IsStreaming();
					float minDistance = audio.GetMinDistance();
					float maxDistance = audio.GetMaxDistance();
					uint32_t currentRollOff = (uint32_t)audio.GetRollOffModel();

					if (UI::DrawAssetSelection("Audio", asset))
					{
						audio.SetAudioAsset(asset);
						bEntityChanged = true;
					}

					if (UI::Combo("Roll off", currentRollOff, rollOffModels, inSelectedRollOffModel, rollOffToolTips))
					{
						audio.SetRollOffModel(RollOffModel(inSelectedRollOffModel));
						bEntityChanged = true;
					}

					if (UI::PropertySlider("Volume", volume, 0.f, 1.f))
					{
						audio.SetVolume(volume);
						bEntityChanged = true;
					}

					if (UI::PropertyDrag("Loop Count", loopCount, 1.f, -1, 10, "-1 = Loop Endlessly; 0 = Play once; 1 = Play twice, etc..."))
					{
						audio.SetLoopCount(loopCount);
						bEntityChanged = true;
					}

					if (UI::PropertyDrag("Min Distance", minDistance, 1.f, 0.f, maxDistance, "The minimum distance is the point at which the sound starts attenuating."
						" If the listener is any closer to the source than the minimum distance, the sound will play at full volume."))
					{
						audio.SetMinDistance(minDistance);
						bEntityChanged = true;
					}

					if (UI::PropertyDrag("Max Distance", maxDistance, 1.f, 0.f, 100000.f, "The maximum distance is the point at which the sound stops"
						" attenuating and its volume remains constant (a volume which is not necessarily zero)"))
					{
						audio.SetMaxDistance(maxDistance);
						bEntityChanged = true;
					}

					if (UI::Property("Is Looping?", bLooping))
					{
						audio.SetLooping(bLooping);
						bEntityChanged = true;
					}
						
					if (UI::Property("Is Streaming?", bStreaming, "When you stream a sound, you can only have one instance of it playing at any time."
						" This limitation exists because there is only one decode buffer per stream."
						" As a rule of thumb, streaming is great for music tracks, voice cues, and ambient tracks,"
						" while most sound effects should be loaded into memory"))
					{
						audio.SetStreaming(bStreaming);
						bEntityChanged = true;
					}

					if (UI::Property("Is Muted?", bMuted))
					{
						audio.SetMuted(bMuted);
						bEntityChanged = true;
					}

					bEntityChanged |= UI::Property("Autoplay", audio.bAutoplay);
					bEntityChanged |= UI::Property("Enable Doppler Effect", audio.bEnableDopplerEffect);

					UI::EndPropertyGrid();
				});
				break;
			}
		
			case SelectedComponent::ReverbComponent:
			{
				DrawComponentTransformNode(entity, entity.GetComponent<ReverbComponent>());
				DrawComponent<ReverbComponent>("Reverb", entity, [&entity, this](auto& reverb)
				{
					static const std::vector<std::string> presets = { "Generic", "Padded cell", "Room", "Bathroom", "Living room" , "Stone room",
									"Auditorium" , "Concert hall" , "Cave" , "Arena" , "Hangar" , "Carpetted hallway" , "Hallway" , "Stone corridor" , 
									"Alley", "Forest" , "City" , "Mountains", "Quarry" , "Plain" , "Parking lot" , "Sewer pipe" , "Under water" };

					int inSelectedPreset = 0;
					float minDistance = reverb.GetMinDistance();
					float maxDistance = reverb.GetMaxDistance();
					bool bActive = reverb.IsActive();
					bool bVisualize = reverb.IsVisualizeRadiusEnabled();
					UI::BeginPropertyGrid("ReverbComponent");

					if (UI::Combo("Preset", (uint32_t)reverb.GetPreset(), presets, inSelectedPreset))
					{
						reverb.SetPreset(ReverbPreset(inSelectedPreset));
						bEntityChanged = true;
					}

					if (UI::PropertyDrag("Min Distance", minDistance, 0.25f, 0.f, maxDistance, "Reverb is at full volume within that radius"))
					{
						reverb.SetMinDistance(minDistance);
						bEntityChanged = true;
					}

					if (UI::PropertyDrag("Max Distance", maxDistance, 0.25f, minDistance, 0.f, "Reverb is disabled outside that radius"))
					{
						reverb.SetMaxDistance(maxDistance);
						bEntityChanged = true;
					}

					if (UI::Property("Is Active", bActive))
					{
						reverb.SetActive(bActive);
						bEntityChanged = true;
					}

					if (UI::Property("Visualize radius", bVisualize))
					{
						reverb.SetVisualizeRadiusEnabled(bVisualize);
						bEntityChanged = true;
					}

					UI::EndPropertyGrid();
				});
				break;
			}
		}
	}

	void EntityPropertiesPanel::DrawEntityTransformNode(Entity& entity)
	{
		Transform transform;
		bool bValueChanged = false;
		bool bRotationChanged = false;
		bool bUseRelativeTransform = false;

		if (Entity parent = entity.GetParent())
		{
			transform = entity.GetRelativeTransform();
			bUseRelativeTransform = true;
		}
		else
			transform = entity.GetWorldTransform();

		glm::vec3 rotationInDegrees = glm::degrees(transform.Rotation.EulerAngles());
		glm::vec3 rotationInDegreesOld = rotationInDegrees;

		if (bDrawWorldTransform || bUseRelativeTransform)
		{
			DrawComponent<TransformComponent>(bUseRelativeTransform ? "Transform (relative)" : "Transform", entity, [&transform, &rotationInDegrees, &bValueChanged, &bRotationChanged](auto& transformComponent)
			{
				bValueChanged |= UI::DrawVec3Control("Location", transform.Location, glm::vec3{ 0.f });
				bRotationChanged = UI::DrawVec3Control("Rotation", rotationInDegrees, glm::vec3{ 0.f });
				bValueChanged |= UI::DrawVec3Control("Scale", transform.Scale3D, glm::vec3{ 1.f });
				bValueChanged |= bRotationChanged;
			}, false);
		}

		if (bValueChanged)
		{
			if (bRotationChanged)
			{
				float newYRot = rotationInDegrees.y;
				bool bInverted = m_InvertEntityRotation[entity];
				if (bInverted)
				{
					glm::vec3 rotDiff = rotationInDegreesOld - rotationInDegrees;
					rotDiff *= -1.f;
					rotationInDegrees = rotationInDegreesOld - rotDiff;
					newYRot = rotationInDegrees.y;
				}

				if (newYRot < -90.f)
				{
					m_InvertEntityRotation[entity] = !bInverted;
					rotationInDegrees.x -= 180.f;
					rotationInDegrees.y += 180.f;
					rotationInDegrees.y *= -1.f;
					rotationInDegrees.z += 180.f;
				}
				else if (newYRot > 90.f)
				{
					m_InvertEntityRotation[entity] = !bInverted;
					rotationInDegrees.x -= -180.f;
					rotationInDegrees.y += -180.f;
					rotationInDegrees.y *= -1.f;
					rotationInDegrees.z += -180.f;
				}

				transform.Rotation = Rotator::FromEulerAngles(glm::radians(rotationInDegrees));
			}

			if (bUseRelativeTransform)
				entity.SetRelativeTransform(transform);
			else
				entity.SetWorldTransform(transform);

			bEntityChanged |= bValueChanged;
		}
	}

	void EntityPropertiesPanel::DrawComponentTransformNode(Entity& entity, SceneComponent& sceneComponent)
	{
		Transform relativeTranform = sceneComponent.GetRelativeTransform();
		glm::vec3 rotationInDegrees = glm::degrees(relativeTranform.Rotation.EulerAngles());
		glm::vec3 rotationInDegreesOld = rotationInDegrees;
		bool bValueChanged = false;
		bool bRotationChanged = false;

		DrawComponent<TransformComponent>("Transform (relative)", entity, [&relativeTranform, &rotationInDegrees, &bValueChanged, &bRotationChanged](auto& transformComponent)
		{
			bValueChanged |= UI::DrawVec3Control("Location", relativeTranform.Location, glm::vec3{0.f});
			bRotationChanged = UI::DrawVec3Control("Rotation", rotationInDegrees, glm::vec3{0.f});
			bValueChanged |= UI::DrawVec3Control("Scale", relativeTranform.Scale3D, glm::vec3{1.f});
			bValueChanged |= bRotationChanged;
		}, false);

		if (bValueChanged)
		{
			if (bRotationChanged)
			{
				float newYRot = rotationInDegrees.y;
				bool bInverted = m_InvertComponentRotation[entity];
				if (bInverted)
				{
					glm::vec3 rotDiff = rotationInDegreesOld - rotationInDegrees;
					rotDiff *= -1.f;
					rotationInDegrees = rotationInDegreesOld - rotDiff;
					newYRot = rotationInDegrees.y;
				}

				if (newYRot < -90.f)
				{
					m_InvertComponentRotation[entity] = !bInverted;
					rotationInDegrees.x -= 180.f;
					rotationInDegrees.y += 180.f;
					rotationInDegrees.y *= -1.f;
					rotationInDegrees.z += 180.f;
				}
				else if (newYRot > 90.f)
				{
					m_InvertComponentRotation[entity] = !bInverted;
					rotationInDegrees.x -= -180.f;
					rotationInDegrees.y += -180.f;
					rotationInDegrees.y *= -1.f;
					rotationInDegrees.z += -180.f;
				}

				relativeTranform.Rotation = Rotator::FromEulerAngles(glm::radians(rotationInDegrees));
			}
			sceneComponent.SetRelativeTransform(relativeTranform);
			
			bEntityChanged |= bValueChanged;
		}
	}
}
