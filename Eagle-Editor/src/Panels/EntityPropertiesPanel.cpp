#include "EntityPropertiesPanel.h"

#include "Eagle/Asset/AssetManager.h"
#include "Eagle/Animation/Animation.h"

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
	static const char* s_ObstacleHelpMsg = "Can be used for AI Navigation to block the path. Note: only box obstacles react to rotation (along Y), other obstacles don't rotate!\nMesh colliders can't be obstacles";
	static const char* s_AffectsNavMeshHelpMsg = "If set to false, it won't affect NavMesh builds. It still can be used as an obstacle though";
	static const char* s_FilterLowHangingObstaclesHelpMsg = "Marks non-walkable spans as walkable if their maximum is within AgentMaxClimb of the span below them.\n"
		"This removes small obstacles and rasterization artifacts that the agent would be able to walk over such as curbs.\n"
		"It also allows agents to move up terraced structures like stairs.";
	static const char* s_FilterLedgeSpans = "Marks spans that are ledges as not-walkable.\nA ledge is a span with one or more neighbors whose maximum is further away than AgentMaxClimb from the current span's maximum.\n"
		"This method removes the impact of the overestimation of conservative voxelization so the resulting mesh will not have regions hanging in the air over ledges.";
	static const char* s_FilterWalkableLowHeightSpans = "Marks walkable spans as not walkable if the clearance above the span is less than the specified AgentHeight.\n"
		"For this filter, the clearance above the span is the distance from the span's maximum to the minimum of the next higher span in the same column.\n"
		"If there is no higher span in the column, the clearance is computed as the distance from the top of the span to the maximum heightfield height.";
	static const std::vector<std::string> s_LockStrings = { "X", "Y", "Z" };

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
			{\
				field.SetStoredValue(value);\
				bEntityChanged = true;\
			}\
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

	SceneComponent* EntityPropertiesPanel::GetSelectedComponent()
	{
		if (!m_Entity)
			return nullptr;

		switch (m_SelectedComponent)
		{
		case SelectedComponent::None: return nullptr;
		case SelectedComponent::Sprite: return &m_Entity.GetComponent<SpriteComponent>();
		case SelectedComponent::StaticMesh: return &m_Entity.GetComponent<StaticMeshComponent>();
		case SelectedComponent::SkeletalMesh: return &m_Entity.GetComponent<SkeletalMeshComponent>();
		case SelectedComponent::Billboard: return &m_Entity.GetComponent<BillboardComponent>();
		case SelectedComponent::Text3D: return &m_Entity.GetComponent<TextComponent>();
		case SelectedComponent::Camera: return &m_Entity.GetComponent<CameraComponent>();
		case SelectedComponent::PointLight: return &m_Entity.GetComponent<PointLightComponent>();
		case SelectedComponent::DirectionalLight: return &m_Entity.GetComponent<DirectionalLightComponent>();
		case SelectedComponent::SpotLight: return &m_Entity.GetComponent<SpotLightComponent>();
		case SelectedComponent::BoxCollider: return &m_Entity.GetComponent<BoxColliderComponent>();
		case SelectedComponent::SphereCollider: return &m_Entity.GetComponent<SphereColliderComponent>();
		case SelectedComponent::CapsuleCollider: return &m_Entity.GetComponent<CapsuleColliderComponent>();
		case SelectedComponent::MeshCollider: return &m_Entity.GetComponent<MeshColliderComponent>();
		case SelectedComponent::AudioComponent: return &m_Entity.GetComponent<AudioComponent>();
		case SelectedComponent::ReverbComponent: return &m_Entity.GetComponent<ReverbComponent>();
		case SelectedComponent::ParticleSystem: return &m_Entity.GetComponent<ParticleSystemComponent>();
		case SelectedComponent::Decal: return &m_Entity.GetComponent<DecalComponent>();
		case SelectedComponent::NavigationMeshComponent: return &m_Entity.GetComponent<NavigationMeshComponent>();
		}
		return nullptr;
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
#define EG_ADD_COMPONENT_MENU_ITEM(type, name) DrawAddComponentMenuItem<type>(name, #type)

			UI::TextWithSeparator("Basic");
			EG_ADD_COMPONENT_MENU_ITEM(ScriptComponent, "C# Script");
			EG_ADD_COMPONENT_MENU_ITEM(CameraComponent, "Camera");

			UI::TextWithSeparator("2D");
			EG_ADD_COMPONENT_MENU_ITEM(SpriteComponent, "Sprite");
			EG_ADD_COMPONENT_MENU_ITEM(BillboardComponent, "Billboard");
			EG_ADD_COMPONENT_MENU_ITEM(Text2DComponent, "Text 2D");
			EG_ADD_COMPONENT_MENU_ITEM(Image2DComponent, "Image 2D");
			EG_ADD_COMPONENT_MENU_ITEM(DecalComponent, "Decal");
			EG_ADD_COMPONENT_MENU_ITEM(ParticleSystemComponent, "Particle System");

			UI::TextWithSeparator("3D");
			EG_ADD_COMPONENT_MENU_ITEM(StaticMeshComponent, "Static Mesh");
			EG_ADD_COMPONENT_MENU_ITEM(SkeletalMeshComponent, "Skeletal Mesh");
			EG_ADD_COMPONENT_MENU_ITEM(TextComponent, "Text");

			UI::TextWithSeparator("Audio");
			EG_ADD_COMPONENT_MENU_ITEM(AudioComponent, "Audio");
			EG_ADD_COMPONENT_MENU_ITEM(ReverbComponent, "Reverb");

			UI::TextWithSeparator("Physics");
			EG_ADD_COMPONENT_MENU_ITEM(RigidBodyComponent, "Rigid Body");
			EG_ADD_COMPONENT_MENU_ITEM(BoxColliderComponent, "Box Collider");
			EG_ADD_COMPONENT_MENU_ITEM(SphereColliderComponent, "Sphere Collider");
			EG_ADD_COMPONENT_MENU_ITEM(CapsuleColliderComponent, "Capsule Collider");
			EG_ADD_COMPONENT_MENU_ITEM(MeshColliderComponent, "Mesh Collider");

			UI::TextWithSeparator("AI Navigation");
			EG_ADD_COMPONENT_MENU_ITEM(NavigationMeshComponent, "Navigation Mesh");

			UI::TextWithSeparator("Lights");
			EG_ADD_COMPONENT_MENU_ITEM(PointLightComponent, "Point Light");
			EG_ADD_COMPONENT_MENU_ITEM(DirectionalLightComponent, "Directional Light");
			EG_ADD_COMPONENT_MENU_ITEM(SpotLightComponent, "Spot Light");

#undef EG_ADD_COMPONENT_MENU_ITEM

			ImGui::EndPopup();
		}

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
#define EG_DRAW_COMPONENT_LINE(label, type, typeEnum) { if (DrawComponentLine<type>(label, entity, m_SelectedComponent == typeEnum)) m_SelectedComponent = typeEnum; }
				EG_DRAW_COMPONENT_LINE("C# Script", ScriptComponent, SelectedComponent::Script);
				EG_DRAW_COMPONENT_LINE("Audio", AudioComponent, SelectedComponent::AudioComponent);
				EG_DRAW_COMPONENT_LINE("Reverb", ReverbComponent, SelectedComponent::ReverbComponent);
				EG_DRAW_COMPONENT_LINE("Rigid Body", RigidBodyComponent, SelectedComponent::RigidBody);
				EG_DRAW_COMPONENT_LINE("Box Collider", BoxColliderComponent, SelectedComponent::BoxCollider);
				EG_DRAW_COMPONENT_LINE("Sphere Collider", SphereColliderComponent, SelectedComponent::SphereCollider);
				EG_DRAW_COMPONENT_LINE("Capsule Collider", CapsuleColliderComponent, SelectedComponent::CapsuleCollider);
				EG_DRAW_COMPONENT_LINE("Mesh Collider", MeshColliderComponent, SelectedComponent::MeshCollider);
				EG_DRAW_COMPONENT_LINE("Sprite", SpriteComponent, SelectedComponent::Sprite);
				EG_DRAW_COMPONENT_LINE("Static Mesh", StaticMeshComponent, SelectedComponent::StaticMesh);
				EG_DRAW_COMPONENT_LINE("Skeletal Mesh", SkeletalMeshComponent, SelectedComponent::SkeletalMesh);
				EG_DRAW_COMPONENT_LINE("Billboard", BillboardComponent, SelectedComponent::Billboard);
				EG_DRAW_COMPONENT_LINE("Text", TextComponent, SelectedComponent::Text3D);
				EG_DRAW_COMPONENT_LINE("Text 2D", Text2DComponent, SelectedComponent::Text2D);
				EG_DRAW_COMPONENT_LINE("Image 2D", Image2DComponent, SelectedComponent::Image2D);
				EG_DRAW_COMPONENT_LINE("Camera", CameraComponent, SelectedComponent::Camera);
				EG_DRAW_COMPONENT_LINE("Point Light", PointLightComponent, SelectedComponent::PointLight);
				EG_DRAW_COMPONENT_LINE("Directional Light", DirectionalLightComponent, SelectedComponent::DirectionalLight);
				EG_DRAW_COMPONENT_LINE("Spot Light", SpotLightComponent, SelectedComponent::SpotLight);
				EG_DRAW_COMPONENT_LINE("Particle System", ParticleSystemComponent, SelectedComponent::ParticleSystem);
				EG_DRAW_COMPONENT_LINE("Decal", DecalComponent, SelectedComponent::Decal);
				EG_DRAW_COMPONENT_LINE("Navigation Mesh", NavigationMeshComponent, SelectedComponent::NavigationMeshComponent);
#undef EG_DRAW_COMPONENT_LINE
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
					bool bReceivesDecals = sprite.DoesReceiveDecals();

					if (UI::Property("Casts shadows", bCastsShadows, s_CastsShadowsHelpMsg))
					{
						sprite.SetCastsShadows(bCastsShadows);
						bEntityChanged = true;
					}

					if (UI::Property("Receives Decals", bReceivesDecals))
					{
						sprite.SetReceivesDecals(bReceivesDecals);
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
					bool bReceivesDecals = smComponent.DoesReceiveDecals();
					Ref<AssetStaticMesh> staticMesh = smComponent.GetMeshAsset();
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

					if (UI::Property("Receives Decals", bReceivesDecals))
					{
						smComponent.SetReceivesDecals(bReceivesDecals);
						bEntityChanged = true;
					}

					ImGui::Separator();

					const uint32_t materialsCount = smComponent.GetMaterialsSlotsCount();
					for (uint32_t i = 0; i < materialsCount; ++i)
					{
						auto materialAsset = smComponent.GetMaterialAsset(i);
						if (UI::DrawAssetSelection("Material " + std::to_string(i), materialAsset))
						{
							smComponent.SetMaterialAsset(i, materialAsset);
							bEntityChanged = true;
						}
					}

					UI::EndPropertyGrid();
				});
				break;
			}
			
			case SelectedComponent::SkeletalMesh:
			{
				DrawComponentTransformNode(entity, entity.GetComponent<SkeletalMeshComponent>());
				DrawComponent<SkeletalMeshComponent>("Skeletal Mesh", entity, [&entity, this](SkeletalMeshComponent& smComponent)
				{
					UI::BeginPropertyGrid("SkeletalMeshComponent");
					Ref<AssetSkeletalMesh> skeletalMesh = smComponent.GetMeshAsset();
					bool bCastsShadows = smComponent.DoesCastShadows();
					bool bReceivesDecals = smComponent.DoesReceiveDecals();
					bool bRagdollEnabled = smComponent.IsRagdollEnabled();

					if (UI::DrawAssetSelection("Skeletal Mesh", skeletalMesh))
					{
						smComponent.SetMeshAsset(skeletalMesh);
						bEntityChanged = true;
					}

					if (UI::Property("Casts shadows", bCastsShadows, s_CastsShadowsHelpMsg))
					{
						smComponent.SetCastsShadows(bCastsShadows);
						bEntityChanged = true;
					}

					if (UI::Property("Receives Decals", bReceivesDecals))
					{
						smComponent.SetReceivesDecals(bReceivesDecals);
						bEntityChanged = true;
					}

					ImGui::Separator();

					const uint32_t materialsCount = smComponent.GetMaterialsSlotsCount();
					for (uint32_t i = 0; i < materialsCount; ++i)
					{
						auto materialAsset = smComponent.GetMaterialAsset(i);
						if (UI::DrawAssetSelection("Material " + std::to_string(i), materialAsset))
						{
							smComponent.SetMaterialAsset(i, materialAsset);
							bEntityChanged = true;
						}
					}

					ImGui::Separator();

					if (UI::Property("Ragdolling", bRagdollEnabled))
					{
						smComponent.SetRagdollEnabled(bRagdollEnabled);
						bEntityChanged = true;
					}

					UI::ComboEnum("Animation Type", smComponent.AnimType);

					const RootMotionLockFlag lockFlags = smComponent.GetRootMotionLockFlags();
					bool bLockPositions[3] = { HasFlags(lockFlags, RootMotionLockFlag::PositionX), HasFlags(lockFlags, RootMotionLockFlag::PositionY), HasFlags(lockFlags, RootMotionLockFlag::PositionZ) };
					if (UI::Property("Root Motion Lock Position", s_LockStrings, bLockPositions))
					{
						smComponent.SetRootMotionLockFlag(RootMotionLockFlag::PositionX, bLockPositions[0]);
						smComponent.SetRootMotionLockFlag(RootMotionLockFlag::PositionY, bLockPositions[1]);
						smComponent.SetRootMotionLockFlag(RootMotionLockFlag::PositionZ, bLockPositions[2]);
						bEntityChanged = true;
					}
					
					bool bEndGrid = true;
					if (smComponent.AnimType == SkeletalMeshComponent::AnimationType::Clip)
					{
						auto animAsset = smComponent.GetAnimationAsset();
						if (UI::DrawAssetSelection("Animation Clip", animAsset))
						{
							smComponent.SetAnimationAsset(animAsset);
							bEntityChanged = true;
						}

						const bool bHasAnim = animAsset.operator bool();
						float current = bHasAnim ? smComponent.CurrentClipPlayTime : 0.f;
						const float duration = bHasAnim ? animAsset->GetAnimation()->Duration : 1.f;
						if (!bHasAnim)
							UI::PushItemDisabled();

						if (UI::PropertySlider("Start Position", current, 0.f, duration))
						{
							smComponent.CurrentClipPlayTime = glm::clamp(current, 0.f, animAsset->GetAnimation()->Duration);
							smComponent.PrevClipPlayTime = smComponent.CurrentClipPlayTime;
							bEntityChanged = true;
						}

						if (!bHasAnim)
							UI::PopItemDisabled();

						bEntityChanged |= UI::PropertyDrag("Playback Speed", smComponent.ClipPlaybackSpeed, 0.1f);
						bEntityChanged |= UI::Property("Is Looping", smComponent.bClipLooping);
					}
					else
					{
						auto graphAsset = smComponent.GetAnimationGraphAsset();
						if (UI::DrawAssetSelection("Animation Graph", graphAsset))
						{
							smComponent.SetAnimationGraphAsset(graphAsset);
							bEntityChanged = true;
						}
						if (graphAsset)
						{
							UI::EndPropertyGrid();
							bEndGrid = false;

							constexpr ImGuiTreeNodeFlags treeFlags = ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth
								| ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_AllowItemOverlap;

							ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
							ImGui::Separator();
							bool treeOpened = ImGui::TreeNodeEx("Graph Variables", treeFlags);
							ImGui::PopStyleVar();
							if (treeOpened)
							{
								UI::BeginPropertyGrid("Graph_Variables");

								auto& graph = smComponent.GetAnimationGraph();
								for (auto& [name, var] : graph->GetVariables())
								{
									switch (var->GetType())
									{
									case GraphVariableType::Bool:
									{
										auto boolVar = Cast<GraphVariableBool>(var);
										bEntityChanged |= UI::Property(name, boolVar->Value);
										break;
									}
									case GraphVariableType::Float:
									{
										auto floatVar = Cast<GraphVariableFloat>(var);
										bEntityChanged |= UI::PropertyDrag(name, floatVar->Value, 0.1f);
										break;
									}
									case GraphVariableType::Animation:
									{
										auto animVar = Cast<GraphVariableAnimation>(var);
										bEntityChanged |= UI::DrawAssetSelection(name, animVar->Value);
										break;
									}
									case GraphVariableType::String:
									{
										auto animVar = Cast<GraphVariableString>(var);
										bEntityChanged |= UI::PropertyText(name, animVar->Value);
										break;
									}
									default:
										EG_CORE_ASSERT(false);
									}
								}

								UI::EndPropertyGrid();
								ImGui::TreePop();
							}
						}
					}

					if (bEndGrid)
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
					bool bReceivesDecals = component.DoesReceiveDecals();
					Ref<AssetFont> asset = component.GetFontAsset();
					Ref<AssetMaterial> materialAsset = component.GetMaterialAsset();

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
						if (UI::DrawAssetSelection("Material", materialAsset))
						{
							component.SetMaterialAsset(materialAsset);
							bEntityChanged = true;
						}

						if (UI::Property("Receives Decals", bReceivesDecals))
						{
							component.SetReceivesDecals(bReceivesDecals);
							bEntityChanged = true;
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

					if (UI::PropertyDrag("Volumetric Fog Intensity", fogIntensity, 0.1f, 0.f, 0.f, "Requires `Is Volumetric` to be enabled"))
					{
						pointLight.SetVolumetricFogIntensity(fogIntensity);
						bEntityChanged = true;
					}

					if (!bVolumetricsEnabled)
						UI::PopItemDisabled();

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

					if (UI::PropertyDrag("Volumetric Fog Intensity", fogIntensity, 0.1f, 0.f, 0.f, "Requires `Is Volumetric` to be enabled"))
					{
						directionalLight.SetVolumetricFogIntensity(fogIntensity);
						bEntityChanged = true;
					}

					if (!bVolumetricsEnabled)
						UI::PopItemDisabled();

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

					if (UI::PropertyDrag("Volumetric Fog Intensity", fogIntensity, 0.1f, 0.f, 0.f, "Requires `Is Volumetric` to be enabled"))
					{
						spotLight.SetVolumetricFogIntensity(fogIntensity);
						bEntityChanged = true;
					}

					if (!bVolumetricsEnabled)
						UI::PopItemDisabled();

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
										{
											field.SetStoredValue(value);
											bEntityChanged = true;
										}
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
										{
											field.SetStoredValue(value);
											bEntityChanged = true;
										}
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
										{
											field.SetStoredValue<std::string>(value);
											bEntityChanged = true;
										}
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
										{
											field.SetStoredValue(value);
											bEntityChanged = true;
										}
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
										{
											field.SetStoredValue(value);
											bEntityChanged = true;
										}
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
										{
											field.SetStoredValue(value);
											bEntityChanged = true;
										}
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
										{
											field.SetStoredValue(value);
											bEntityChanged = true;
										}
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
										{
											field.SetStoredValue(value);
											bEntityChanged = true;
										}
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
										{
											field.SetStoredValue(value);
											bEntityChanged = true;
										}
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
										{
											field.SetStoredValue(value);
											bEntityChanged = true;
										}
									}
									break;
								}
								case FieldType::Entity:
								{
									const auto& scene = Scene::GetCurrentScene();
									GUID value = bRuntime ? field.GetRuntimeValue<GUID>(entityInstance) : field.GetStoredValue<GUID>();
									Entity entity = scene->GetEntityByGUID(value);
									const bool bValid = entity.IsValid();
									int currentSelection = -1;
									int i = 0;

									const auto entities = scene->GetAllEntitiesWith<IDComponent, EntitySceneNameComponent>();
									std::vector<std::string> names;
									std::vector<GUID> ids;
									names.reserve(entities.size_hint());
									ids.reserve(entities.size_hint());

									for (auto& [entity, idComp, nameComp] : entities.each())
									{
										const auto& ID = idComp.ID;
										const auto& name = nameComp.Name;
										ids.emplace_back(ID);
										names.emplace_back(name);

										if (bValid && (ID == value))
										{
											currentSelection = i;
										}
										i++;
									}
									if (UI::ComboWithNone(field.Name.c_str(), currentSelection, names, currentSelection))
									{
										value = currentSelection == -1 ? GUID(0, 0) : ids[currentSelection];

										if (bRuntime)
											field.SetRuntimeValue(entityInstance, value);
										else
										{
											field.SetStoredValue(value);
											bEntityChanged = true;
										}
									}
									break;
								}
								AssetField_Case(Asset);
								AssetField_Case(AssetTexture2D);
								AssetField_Case(AssetTextureCube);
								AssetField_Case(AssetStaticMesh);
								AssetField_Case(AssetSkeletalMesh);
								AssetField_Case(AssetAudio);
								AssetField_Case(AssetSoundGroup);
								AssetField_Case(AssetFont);
								AssetField_Case(AssetMaterial);
								AssetField_Case(AssetPhysicsMaterial);
								AssetField_Case(AssetEntity);
								AssetField_Case(AssetScene);
								AssetField_Case(AssetAnimation);
								AssetField_Case(AssetAnimationGraph);
								AssetField_Case(AssetParticleSystem);
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
					UI::BeginPropertyGrid("RigidBodyComponent");

					if (bRuntime)
						UI::PushItemDisabled();

					bEntityChanged |= UI::ComboEnum<PhysicsBodyType>("Body type", rigidBody.BodyType);

					if (bRuntime)
						UI::PopItemDisabled();
						
					if (rigidBody.BodyType == PhysicsBodyType::Dynamic)
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
							
						bEntityChanged |= UI::ComboEnum<CollisionDetectionType>("Collision Detection", rigidBody.CollisionDetection,
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

						if (UI::Property("Is Kinematic", bKinematic, "Kinametics have infinite mass and inertia. Use `SetKinematicTarget` to control it."
							" These are good for dynamic objects that are moved by your code instead of by the physics simulation (e.g. moving platform)"))
						{
							rigidBody.SetIsKinematic(bKinematic);
							bEntityChanged = true;
						}
						if (UI::Property("Lock Position", s_LockStrings, bLockPositions))
						{
							rigidBody.SetLockFlag(ActorLockFlag::PositionX, bLockPositions[0]);
							rigidBody.SetLockFlag(ActorLockFlag::PositionY, bLockPositions[1]);
							rigidBody.SetLockFlag(ActorLockFlag::PositionZ, bLockPositions[2]);
							bEntityChanged = true;
						}
						if (UI::Property("Lock Rotation", s_LockStrings, bLockRotations))
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
					bool bObstacle = collider.IsObstacle();
					bool bAffectsNavMesh = collider.DoesAffectNavMeshBuild();

					if (UI::DrawAssetSelection("Physics Material", materialAsset))
					{
						collider.SetPhysicsMaterialAsset(materialAsset);
						bEntityChanged = true;
					}

					if (UI::PropertyDrag("Size", size, 0.05f))
					{
						collider.SetSize(size);
						bEntityChanged = true;
					}

					if (UI::Property("Is Trigger", bTrigger, s_TriggerHelpMsg))
					{
						collider.SetIsTrigger(bTrigger);
						bEntityChanged = true;
					}

					if (UI::Property("Is Obstacle", bObstacle, s_ObstacleHelpMsg))
					{
						collider.SetIsObstacle(bObstacle);
						bEntityChanged = true;
					}

					if (UI::Property("Affects NavMesh", bAffectsNavMesh, s_AffectsNavMeshHelpMsg))
					{
						collider.SetAffectsNavMeshBuild(bAffectsNavMesh);
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
					bool bObstacle = collider.IsObstacle();
					bool bAffectsNavMesh = collider.DoesAffectNavMeshBuild();

					if (UI::DrawAssetSelection("Physics Material", materialAsset))
					{
						collider.SetPhysicsMaterialAsset(materialAsset);
						bEntityChanged = true;
					}

					if (UI::PropertyDrag("Radius", radius, 0.5f))
					{
						collider.SetRadius(radius);
						bEntityChanged = true;
					}
						
					if (UI::Property("Is Trigger", bTrigger, s_TriggerHelpMsg))
					{
						collider.SetIsTrigger(bTrigger);
						bEntityChanged = true;
					}

					if (UI::Property("Is Obstacle", bObstacle, s_ObstacleHelpMsg))
					{
						collider.SetIsObstacle(bObstacle);
						bEntityChanged = true;
					}

					if (UI::Property("Affects NavMesh", bAffectsNavMesh, s_AffectsNavMeshHelpMsg))
					{
						collider.SetAffectsNavMeshBuild(bAffectsNavMesh);
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
					bool bObstacle = collider.IsObstacle();
					bool bAffectsNavMesh = collider.DoesAffectNavMeshBuild();

					if (UI::DrawAssetSelection("Physics Material", materialAsset))
					{
						collider.SetPhysicsMaterialAsset(materialAsset);
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

					if (UI::Property("Is Trigger", bTrigger, s_TriggerHelpMsg))
					{
						collider.SetIsTrigger(bTrigger);
						bEntityChanged = true;
					}

					if (UI::Property("Is Obstacle", bObstacle, s_ObstacleHelpMsg))
					{
						collider.SetIsObstacle(bObstacle);
						bEntityChanged = true;
					}

					if (UI::Property("Affects NavMesh", bAffectsNavMesh, s_AffectsNavMeshHelpMsg))
					{
						collider.SetAffectsNavMeshBuild(bAffectsNavMesh);
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
					Ref<AssetStaticMesh> collisionMesh = collider.GetCollisionMeshAsset();
					bool bTrigger = collider.IsTrigger();
					bool bShowCollision = collider.IsCollisionVisible();
					bool bConvex = collider.IsConvex();
					bool bTwoSided = collider.IsTwoSided();
					bool bAffectsNavMesh = collider.DoesAffectNavMeshBuild();

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

					if (UI::Property("Affects NavMesh", bAffectsNavMesh, s_AffectsNavMeshHelpMsg))
					{
						collider.SetAffectsNavMeshBuild(bAffectsNavMesh);
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
		
			case SelectedComponent::ParticleSystem:
			{
				DrawComponentTransformNode(entity, entity.GetComponent<ParticleSystemComponent>());
				DrawComponent<ParticleSystemComponent>("Particle System", entity, [&entity, this](ParticleSystemComponent& system)
				{
					auto asset = system.GetAsset();

					UI::BeginPropertyGrid("ParticleSystemComponent");

					if (UI::DrawAssetSelection("Particle System", asset))
					{
						system.SetAsset(asset);
						bEntityChanged = true;
					}
					bEntityChanged |= UI::Property("Auto-spawn", system.bAutospawn);

					UI::EndPropertyGrid();
				});
				break;
			}

			case SelectedComponent::Decal:
			{
				DrawComponentTransformNode(entity, entity.GetComponent<DecalComponent>());
				DrawComponent<DecalComponent>("Decal", entity, [&entity, this](DecalComponent& decal)
				{
					UI::BeginPropertyGrid("DecalComponent");

					auto materialAsset = decal.GetMaterialAsset();
					uint32_t sortPriority = decal.GetSortPriority();
					bool bAdjustAspectRatio = decal.IsAdjustAspectRatioEnabled();

					if (UI::DrawAssetSelection("Material", materialAsset, "Material data will be blended with the underlying material based on 'Opacity'"))
					{
						decal.SetMaterialAsset(materialAsset);
						bEntityChanged = true;
					}
					if (UI::PropertyDrag("Sort Priority", sortPriority, 1.f, 0, 0, "Higher value results in Decal being draw on top of others"))
					{
						decal.SetSortPriority(sortPriority);
						bEntityChanged = true;
					}
					if (UI::Property("Adjust Aspect Ratio", bAdjustAspectRatio, "Aspect Ratio will be adjusted according to Albedo texture"))
					{
						decal.SetAdjustAspectRatioEnabled(bAdjustAspectRatio);
						bEntityChanged = true;
					}

					UI::EndPropertyGrid();
				});
				break;
			}
		
			case SelectedComponent::NavigationMeshComponent:
			{
				DrawComponentTransformNode(entity, entity.GetComponent<NavigationMeshComponent>());
				DrawComponent<NavigationMeshComponent>("Navigation Mesh", entity, [&entity, this](NavigationMeshComponent& component)
				{
					auto settings = component.GetSettings();
					bool bChanged = false;

					UI::BeginPropertyGrid("NavigationMeshComponent");

					if (UI::Button("Build", "Build"))
					{
						entity.GetScene()->BuildNavMesh(&component);
					}
					UI::Property("Auto Rebuild", component.bAutoRebuild, "If enabled, nav mesh is rebuilt automatically when its transform or settings are changed");
					
					UI::TextWithSeparator("Settings");

					bChanged |= UI::PropertyDrag("Visibility AABB Min", settings.AABB.Min, 0.1f, 0, 0);
					bChanged |= UI::PropertyDrag("Visibility AABB Max", settings.AABB.Max, 0.1f, 0, 0);

					if (UI::PropertyDrag("Max Query Nodes", settings.MaxQueryNodes, 32.f, 1, 65535, "Maximum number of search nodes. [Limits: 0 < value <= 65535]"))
					{
						settings.MaxQueryNodes = glm::clamp(settings.MaxQueryNodes, 1u, 65535u);
						bChanged = true;
					}
					if (UI::PropertyDrag("Expected Layers per tile", settings.ExpectedLayersPerTile))
					{
						settings.ExpectedLayersPerTile = glm::clamp(settings.ExpectedLayersPerTile, 1u, 65535u);
						bChanged = true;
					}
					if (UI::PropertyDrag("Max Layers", settings.MaxLayers))
					{
						settings.MaxLayers = glm::clamp(settings.MaxLayers, 1u, 65535u);
						bChanged = true;
					}
					if (UI::PropertyDrag("Max Obstacles", settings.MaxObstacles))
					{
						settings.MaxObstacles = glm::clamp(settings.MaxObstacles, 0u, 1u << 24u);
						bChanged = true;
					}
					if (UI::PropertyDrag("Tile Size", settings.TileSize, 1.f, 0, 0, "The width/height size of tile's on the xz-plane"))
					{
						settings.TileSize = glm::clamp(settings.TileSize, 1u, 1u << 24u);
						bChanged = true;
					}
					if (UI::PropertyDrag("Cell Size", settings.CellSize, 0.05f, 0, 0, "The xz-plane cell size to use for fields"))
					{
						settings.CellSize = glm::max(settings.CellSize, 0.005f);
						bChanged = true;
					}
					if (UI::PropertyDrag("Cell Height", settings.CellHeight, 0.05f, 0, 0, "The y-axis cell size to use for fields"))
					{
						settings.CellHeight = glm::max(settings.CellHeight, 0.005f);
						bChanged = true;
					}

					if (UI::PropertyDrag("Max Slope", settings.MaxSlope, 1.f, 0.f, 90.f, "The maximum slope that is considered walkable"))
					{
						settings.MaxSlope = glm::clamp(settings.MaxSlope, 0.f, 90.f);
						bChanged = true;
					}
					if (UI::PropertyDrag("Agent Height", settings.AgentHeight, 0.05f, 0.0f, 0.f, "Minimum floor to 'ceiling' height that will still allow the floor area to be considered walkable"))
					{
						settings.AgentHeight = glm::max(settings.AgentHeight, 0.1f);
						bChanged = true;
					}
					if (UI::PropertyDrag("Agent Max Climb", settings.AgentMaxClimb, 0.05f, 0.f, 0.f, "Maximum ledge height that is considered to still be traversable"))
					{
						settings.AgentMaxClimb = glm::max(settings.AgentMaxClimb, 0.0f);
						bChanged = true;
					}
					if (UI::PropertyDrag("Agent Radius", settings.AgentRadius, 0.05f, 0.f, 0.f, "The distance to erode/shrink the walkable area of the heightfield away from obstructions"))
					{
						settings.AgentRadius = glm::max(settings.AgentRadius, 0.0f);
						bChanged = true;
					}
					if (UI::PropertyDrag("Edge Max Len", settings.EdgeMaxLen, 0.05f, 0.f, 0.f, "The maximum allowed length for contour edges along the border of the mesh"))
					{
						settings.EdgeMaxLen = glm::max(settings.EdgeMaxLen, 0.0f);
						bChanged = true;
					}
					if (UI::PropertyDrag("Edge Max Error", settings.EdgeMaxError, 0.05f, 0.f, 0.f, "The maximum distance a simplified contour's border edges should deviate the original raw contour"))
					{
						settings.EdgeMaxError = glm::max(settings.EdgeMaxError, 0.0f);
						bChanged = true;
					}
					if (UI::PropertyDrag("Region Min Size", settings.RegionMinSize, 0.05f, 0.f, 0.f, "The minimum number of cells allowed to form isolated island areas"))
					{
						settings.RegionMinSize = glm::max(settings.RegionMinSize, 0.0f);
						bChanged = true;
					}
					if (UI::PropertyDrag("Region Merge Size", settings.RegionMergeSize, 0.05f, 0.f, 0.f, "Any regions with a span count smaller than this value will, if possible, be merged with larger regions"))
					{
						settings.RegionMergeSize = glm::max(settings.RegionMergeSize, 0.0f);
						bChanged = true;
					}
					if (UI::PropertyDrag("Verts Per Poly", settings.VertsPerPoly, 1, 3, 0, "The maximum number of vertices allowed for polygons generated during the contour to polygon conversion process"))
					{
						settings.VertsPerPoly = glm::clamp(settings.VertsPerPoly, 3u, 65535u);
						bChanged = true;
					}
					
					bChanged |= UI::PropertyDrag("Border Size", settings.BorderSize, 1, 0, 0, "The size of the non-navigable border around the heightfield");
					bChanged |= UI::Property("Filter Low Hanging Obstacles", settings.FilterLowHangingObstacles, s_FilterLowHangingObstaclesHelpMsg);
					bChanged |= UI::Property("Filter Ledge Spans", settings.FilterLedgeSpans, s_FilterLedgeSpans);
					bChanged |= UI::Property("Filter Walkable Low Height Spans", settings.FilterWalkableLowHeightSpans, s_FilterWalkableLowHeightSpans);

					UI::EndPropertyGrid();

					if (bChanged)
					{
						component.SetSettings(settings);
						bEntityChanged = true;
					}
				});
				
				break;
			}
		}
	}

	void EntityPropertiesPanel::DrawEntityTransformNode(Entity& entity)
	{
		Transform transform;
		bool bValueChanged = false;
		bool bUseRelativeTransform = false;

		if (Entity parent = entity.GetParent())
		{
			transform = entity.GetRelativeTransform();
			bUseRelativeTransform = true;
		}
		else
			transform = entity.GetWorldTransform();

		if (bDrawWorldTransform || bUseRelativeTransform)
		{
			DrawComponent<TransformComponent>(bUseRelativeTransform ? "Transform (relative)" : "Transform", entity, [&transform, &bValueChanged](auto& transformComponent)
			{
				const glm::quat q = transform.Rotation.GetQuat();
				glm::vec4 quat(q.x, q.y, q.z, q.w);

				bValueChanged |= UI::DrawVec3Control("Location", transform.Location, glm::vec3{ 0.f });
				if (UI::DrawVec4Control("Rotation (Quat)", quat, glm::vec4{ 0, 0, 0, 1 }))
				{
					if (glm::all(glm::epsilonEqual(quat, glm::vec4(0), 0.001f)))
						quat.w = 1.f;
				    quat = glm::normalize(quat);
				    transform.Rotation = glm::quat(quat.w, quat.x, quat.y, quat.z);
				    bValueChanged = true;
				}
				bValueChanged |= UI::DrawVec3Control("Scale", transform.Scale3D, glm::vec3{ 1.f });
			}, false);
		}

		if (bValueChanged)
		{
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
		bool bValueChanged = false;

		DrawComponent<TransformComponent>("Transform (relative)", entity, [&relativeTranform, &bValueChanged](auto& transformComponent)
		{
			const glm::quat q = relativeTranform.Rotation.GetQuat();
			glm::vec4 quat(q.x, q.y, q.z, q.w);

			bValueChanged |= UI::DrawVec3Control("Location", relativeTranform.Location, glm::vec3{0.f});
			if (UI::DrawVec4Control("Rotation (Quat)", quat, glm::vec4{ 0, 0, 0, 1 }))
			{
				if (glm::all(glm::epsilonEqual(quat, glm::vec4(0), 0.001f)))
					quat.w = 1.f;
				quat = glm::normalize(quat);
				relativeTranform.Rotation = glm::quat(quat.w, quat.x, quat.y, quat.z);
				bValueChanged = true;
			}
			bValueChanged |= UI::DrawVec3Control("Scale", relativeTranform.Scale3D, glm::vec3{1.f});
		}, false);

		if (bValueChanged)
		{
			sceneComponent.SetRelativeTransform(relativeTranform);
			bEntityChanged |= bValueChanged;
		}
	}
}
