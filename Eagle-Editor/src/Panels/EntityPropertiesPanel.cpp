#include "EntityPropertiesPanel.h"
#include "../EditorResources.h"

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
	static const char* s_CastsShadowsHelpMsg = "Translucent materials don't cast shadows unless 'Translucent shadows' feature is enabled. Translucent materials don't cast shadows on other translucent materials!";
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
	static const char* s_FFTTypeHelpMsg = "Used in spectrum analysis to reduce leakage/transient signals interfering with the analysis.\n"
		"This is a problem with analysis of continuous signals that only have a small portion of the signal sample (the fft window size).\n"
		"Windowing the signal with a curve or triangle tapers the sides of the fft window to help alleviate this problem.";
	static const char* s_PositionSolverIterationsHelpMsg = "The solver iteration count determines how accurately joints and contacts are resolved. "
		"If you are having trouble with jointed bodies oscillating and behaving erratically, then "
		"setting a higher position iteration count may improve their stability.";
	static const char* s_VelocitySolverIterationsHelpMsg = "If intersecting bodies are being depenetrated too violently, increase the number of velocity "
		"iterations.More velocity iterations will drive the relative exit velocity of the intersecting "
		"objects closer to the correct value given the restitution.";
	static const char* s_CollisionDetectionTypeHelpMsg = "When continuous collision detection (or CCD) is turned on, the affected rigid bodies will not go through other objects at high velocities (a problem also known as tunnelling). "
		"A cheaper but less robust approach is called speculative CCD";

	bool EntityPropertiesPanel::OnImGuiRender(Entity entity, bool bRuntime, bool bVolumetricsEnabled)
	{
		this->bRuntime = bRuntime;
		this->bVolumetricsEnabled = bVolumetricsEnabled;
		m_Entity = entity;
		bEntityChanged = false;

		DrawComponents(entity);

		return bEntityChanged;
	}

	void EntityPropertiesPanel::SetEntitySelected(Entity entity, SelectedComponent selectedComponent)
	{
		m_Entity = entity;
		m_SelectedComponent = selectedComponent;
	}

	SceneComponent* EntityPropertiesPanel::GetSelectedComponent()
	{
		if (!m_Entity)
			return nullptr;

		switch (m_SelectedComponent)
		{
		case SelectedComponent::None: return nullptr;
		case SelectedComponent::SpriteComponent: return &m_Entity.GetComponent<SpriteComponent>();
		case SelectedComponent::StaticMeshComponent: return &m_Entity.GetComponent<StaticMeshComponent>();
		case SelectedComponent::SkeletalMeshComponent: return &m_Entity.GetComponent<SkeletalMeshComponent>();
		case SelectedComponent::BillboardComponent: return &m_Entity.GetComponent<BillboardComponent>();
		case SelectedComponent::TextComponent: return &m_Entity.GetComponent<TextComponent>();
		case SelectedComponent::CameraComponent: return &m_Entity.GetComponent<CameraComponent>();
		case SelectedComponent::PointLightComponent: return &m_Entity.GetComponent<PointLightComponent>();
		case SelectedComponent::DirectionalLightComponent: return &m_Entity.GetComponent<DirectionalLightComponent>();
		case SelectedComponent::SpotLightComponent: return &m_Entity.GetComponent<SpotLightComponent>();
		case SelectedComponent::BoxColliderComponent: return &m_Entity.GetComponent<BoxColliderComponent>();
		case SelectedComponent::SphereColliderComponent: return &m_Entity.GetComponent<SphereColliderComponent>();
		case SelectedComponent::CapsuleColliderComponent: return &m_Entity.GetComponent<CapsuleColliderComponent>();
		case SelectedComponent::MeshColliderComponent: return &m_Entity.GetComponent<MeshColliderComponent>();
		case SelectedComponent::AudioComponent: return &m_Entity.GetComponent<AudioComponent>();
		case SelectedComponent::ReverbComponent: return &m_Entity.GetComponent<ReverbComponent>();
		case SelectedComponent::ParticleSystemComponent: return &m_Entity.GetComponent<ParticleSystemComponent>();
		case SelectedComponent::DecalComponent: return &m_Entity.GetComponent<DecalComponent>();
		case SelectedComponent::NavigationMeshComponent: return &m_Entity.GetComponent<NavigationMeshComponent>();
		}
		return nullptr;
	}

	bool EntityPropertiesPanel::HasSelectedComponent() const
	{
		if (!m_Entity)
			return false;

		switch (m_SelectedComponent)
		{
			case SelectedComponent::None: return false;
			case SelectedComponent::SpriteComponent: return m_Entity.HasComponent<SpriteComponent>();
			case SelectedComponent::StaticMeshComponent: return m_Entity.HasComponent<StaticMeshComponent>();
			case SelectedComponent::SkeletalMeshComponent: return m_Entity.HasComponent<SkeletalMeshComponent>();
			case SelectedComponent::BillboardComponent: return m_Entity.HasComponent<BillboardComponent>();
			case SelectedComponent::TextComponent: return m_Entity.HasComponent<TextComponent>();
			case SelectedComponent::CameraComponent: return m_Entity.HasComponent<CameraComponent>();
			case SelectedComponent::PointLightComponent: return m_Entity.HasComponent<PointLightComponent>();
			case SelectedComponent::DirectionalLightComponent: return m_Entity.HasComponent<DirectionalLightComponent>();
			case SelectedComponent::SpotLightComponent: return m_Entity.HasComponent<SpotLightComponent>();
			case SelectedComponent::ScriptComponent: return m_Entity.HasComponent<ScriptComponent>();
			case SelectedComponent::RigidBodyComponent: return m_Entity.HasComponent<RigidBodyComponent>();
			case SelectedComponent::BoxColliderComponent: return m_Entity.HasComponent<BoxColliderComponent>();
			case SelectedComponent::SphereColliderComponent: return m_Entity.HasComponent<SphereColliderComponent>();
			case SelectedComponent::CapsuleColliderComponent: return m_Entity.HasComponent<CapsuleColliderComponent>();
			case SelectedComponent::MeshColliderComponent: return m_Entity.HasComponent<MeshColliderComponent>();
			case SelectedComponent::AudioComponent: return m_Entity.HasComponent<AudioComponent>();
			case SelectedComponent::ReverbComponent: return m_Entity.HasComponent<ReverbComponent>();
			case SelectedComponent::Text2DComponent: return m_Entity.HasComponent<Text2DComponent>();
			case SelectedComponent::Image2DComponent: return m_Entity.HasComponent<Image2DComponent>();
			case SelectedComponent::ParticleSystemComponent: return m_Entity.HasComponent<ParticleSystemComponent>();
			case SelectedComponent::DecalComponent: return m_Entity.HasComponent<DecalComponent>();
			case SelectedComponent::NavigationMeshComponent: return m_Entity.HasComponent<NavigationMeshComponent>();
			case SelectedComponent::NavigationCrowdAgentComponent: return m_Entity.HasComponent<NavigationCrowdAgentComponent>();
		}
		return false;
	}

	void EntityPropertiesPanel::DrawComponents(Entity& entity)
	{
		ImGui::PushID((void*)entity.GetGUID().GetHash());

		if (!HasSelectedComponent())
		{
			m_SelectedComponent = SelectedComponent::None;
		}
		auto& entityName = entity.GetComponent<EntitySceneNameComponent>().Name;

		if (UI::InputText("##Name", entityName))
		{
			//TODO: Add Check for empty input
			bEntityChanged = true;
		}
		
		ImGui::SameLine();
		ImGui::PushItemWidth(-1);

		if (ImGui::Button("Add"))
			ImGui::OpenPopup("AddComponent");

		if (ImGui::BeginPopup("AddComponent"))
		{
#define EG_ADD_COMPONENT_MENU_ITEM(type, name) do { if (DrawAddComponentMenuItem<type>(name, #type)) { m_SelectedComponent = SelectedComponent::type; } } while (0)

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
			EG_ADD_COMPONENT_MENU_ITEM(NavigationCrowdAgentComponent, "Navigation Crowd Agent");

			UI::TextWithSeparator("Lights");
			EG_ADD_COMPONENT_MENU_ITEM(PointLightComponent, "Point Light");
			EG_ADD_COMPONENT_MENU_ITEM(DirectionalLightComponent, "Directional Light");
			EG_ADD_COMPONENT_MENU_ITEM(SpotLightComponent, "Spot Light");

#undef EG_ADD_COMPONENT_MENU_ITEM

			ImGui::EndPopup();
		}

		ImGui::PopItemWidth();

		const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth
			| ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_AllowOverlap;

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
				const bool bCanRemoveRigidBody = !entity.HasAny<BoxColliderComponent, SphereColliderComponent, CapsuleColliderComponent, MeshColliderComponent>();

#define EG_DRAW_COMPONENT_LINE(label, type, typeEnum) { if (DrawComponentLine<type>(label, entity, m_SelectedComponent == typeEnum)) m_SelectedComponent = typeEnum; }
#define EG_DRAW_COMPONENT_LINE_EX(label, type, typeEnum, bCanRemove) { if (DrawComponentLine<type>(label, entity, m_SelectedComponent == typeEnum, bCanRemove)) m_SelectedComponent = typeEnum; }
				EG_DRAW_COMPONENT_LINE("C# Script", ScriptComponent, SelectedComponent::ScriptComponent);
				EG_DRAW_COMPONENT_LINE("Audio", AudioComponent, SelectedComponent::AudioComponent);
				EG_DRAW_COMPONENT_LINE("Reverb", ReverbComponent, SelectedComponent::ReverbComponent);
				EG_DRAW_COMPONENT_LINE_EX("Rigid Body", RigidBodyComponent, SelectedComponent::RigidBodyComponent, bCanRemoveRigidBody);
				EG_DRAW_COMPONENT_LINE("Box Collider", BoxColliderComponent, SelectedComponent::BoxColliderComponent);
				EG_DRAW_COMPONENT_LINE("Sphere Collider", SphereColliderComponent, SelectedComponent::SphereColliderComponent);
				EG_DRAW_COMPONENT_LINE("Capsule Collider", CapsuleColliderComponent, SelectedComponent::CapsuleColliderComponent);
				EG_DRAW_COMPONENT_LINE("Mesh Collider", MeshColliderComponent, SelectedComponent::MeshColliderComponent);
				EG_DRAW_COMPONENT_LINE("Sprite", SpriteComponent, SelectedComponent::SpriteComponent);
				EG_DRAW_COMPONENT_LINE("Static Mesh", StaticMeshComponent, SelectedComponent::StaticMeshComponent);
				EG_DRAW_COMPONENT_LINE("Skeletal Mesh", SkeletalMeshComponent, SelectedComponent::SkeletalMeshComponent);
				EG_DRAW_COMPONENT_LINE("Billboard", BillboardComponent, SelectedComponent::BillboardComponent);
				EG_DRAW_COMPONENT_LINE("Text", TextComponent, SelectedComponent::TextComponent);
				EG_DRAW_COMPONENT_LINE("Text 2D", Text2DComponent, SelectedComponent::Text2DComponent);
				EG_DRAW_COMPONENT_LINE("Image 2D", Image2DComponent, SelectedComponent::Image2DComponent);
				EG_DRAW_COMPONENT_LINE("Camera", CameraComponent, SelectedComponent::CameraComponent);
				EG_DRAW_COMPONENT_LINE("Point Light", PointLightComponent, SelectedComponent::PointLightComponent);
				EG_DRAW_COMPONENT_LINE("Directional Light", DirectionalLightComponent, SelectedComponent::DirectionalLightComponent);
				EG_DRAW_COMPONENT_LINE("Spot Light", SpotLightComponent, SelectedComponent::SpotLightComponent);
				EG_DRAW_COMPONENT_LINE("Particle System", ParticleSystemComponent, SelectedComponent::ParticleSystemComponent);
				EG_DRAW_COMPONENT_LINE("Decal", DecalComponent, SelectedComponent::DecalComponent);
				EG_DRAW_COMPONENT_LINE("Navigation Mesh", NavigationMeshComponent, SelectedComponent::NavigationMeshComponent);
				EG_DRAW_COMPONENT_LINE("Navigation Crowd Agent", NavigationCrowdAgentComponent, SelectedComponent::NavigationCrowdAgentComponent);
#undef EG_DRAW_COMPONENT_LINE
#undef EG_DRAW_COMPONENT_LINE_EX
				ImGui::TreePop();
			}

			ImGui::TreePop();
		}

		if (m_SelectedComponent == SelectedComponent::None && entity.HasComponent<TransformComponent>())
		{
			DrawEntityTransformNode(entity);
			DrawComponent<TagComponent>("Tag", entity, [this](auto& component)
			{
				UI::BeginPropertyGrid("TagComponent");
				bEntityChanged |= UI::PropertyText("Tag", component.Tag);
				UI::EndPropertyGrid();
			}, false);
		}
		switch (m_SelectedComponent)
		{
			case SelectedComponent::SpriteComponent:
			{
				DrawComponentTransformNode(entity, entity.GetComponent<SpriteComponent>());
				DrawComponent<SpriteComponent>("Sprite", entity, [&entity, this](SpriteComponent& sprite)
				{
					bool bCastsShadows = sprite.DoesCastShadows();
					bool bReceivesDecals = sprite.DoesReceiveDecals();
					bool bAtlas = sprite.IsAtlas();
					bool bVisible = sprite.IsVisible();

					UI::BeginPropertyGrid("SpriteComponent");

					if (UI::Property("Is Visible", bVisible))
					{
						sprite.SetVisible(bVisible);
						bEntityChanged = true;
					}

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
					if (EditorResources::DrawAssetSelection("Material", materialAsset))
					{
						sprite.SetMaterialAsset(materialAsset);
						bEntityChanged = true;
					}
						 
					UI::EndPropertyGrid();
				});
				break;
			}

			case SelectedComponent::StaticMeshComponent:
			{
				DrawComponentTransformNode(entity, entity.GetComponent<StaticMeshComponent>());
				DrawComponent<StaticMeshComponent>("Static Mesh", entity, [&entity, this](StaticMeshComponent& smComponent)
				{
					UI::BeginPropertyGrid("StaticMeshComponent");
					bool bReceivesDecals = smComponent.DoesReceiveDecals();
					Ref<AssetStaticMesh> staticMesh = smComponent.GetMeshAsset();
					bool bCastsShadows = smComponent.DoesCastShadows();
					bool bVisible = smComponent.IsVisible();

					if (EditorResources::DrawAssetSelection("Static Mesh", staticMesh))
					{
						smComponent.SetMeshAsset(staticMesh);
						bEntityChanged = true;
					}

					if (UI::Property("Is Visible", bVisible))
					{
						smComponent.SetVisible(bVisible);
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

					const uint32_t materialsCount = smComponent.GetMaterialsSlotsCount();
					if (materialsCount > 0)
						UI::TextWithSeparator("Materials");
					for (uint32_t i = 0; i < materialsCount; ++i)
					{
						auto materialAsset = smComponent.GetMaterialAsset(i);
						if (EditorResources::DrawAssetSelection("Material " + std::to_string(i), materialAsset))
						{
							smComponent.SetMaterialAsset(i, materialAsset);
							bEntityChanged = true;
						}
					}

					UI::EndPropertyGrid();
				});
				break;
			}
			
			case SelectedComponent::SkeletalMeshComponent:
			{
				DrawComponentTransformNode(entity, entity.GetComponent<SkeletalMeshComponent>());
				DrawComponent<SkeletalMeshComponent>("Skeletal Mesh", entity, [&entity, this](SkeletalMeshComponent& smComponent)
				{
					Ref<AssetSkeletalMesh> skeletalMesh = smComponent.GetMeshAsset();
					bool bCastsShadows = smComponent.DoesCastShadows();
					bool bReceivesDecals = smComponent.DoesReceiveDecals();
					bool bRagdollEnabled = smComponent.IsRagdollEnabled();
					bool bVisible = smComponent.IsVisible();

					UI::BeginPropertyGrid("SkeletalMeshComponent");

					if (EditorResources::DrawAssetSelection("Skeletal Mesh", skeletalMesh))
					{
						smComponent.SetMeshAsset(skeletalMesh);
						bEntityChanged = true;
					}

					if (UI::Property("Is Visible", bVisible))
					{
						smComponent.SetVisible(bVisible);
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

					const uint32_t materialsCount = smComponent.GetMaterialsSlotsCount();
					if (materialsCount > 0)
						UI::TextWithSeparator("Materials");
					for (uint32_t i = 0; i < materialsCount; ++i)
					{
						auto materialAsset = smComponent.GetMaterialAsset(i);
						if (EditorResources::DrawAssetSelection("Material " + std::to_string(i), materialAsset))
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
					if (smComponent.AnimType == AnimationType::Clip)
					{
						auto animAsset = smComponent.GetAnimationAsset();
						if (EditorResources::DrawAssetSelection("Animation Clip", animAsset))
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
						if (EditorResources::DrawAssetSelection("Animation Graph", graphAsset))
						{
							smComponent.SetAnimationGraphAsset(graphAsset);
							bEntityChanged = true;
						}
						if (graphAsset)
						{
							UI::EndPropertyGrid();
							bEndGrid = false;

							constexpr ImGuiTreeNodeFlags treeFlags = ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth
								| ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_AllowOverlap;

							ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
							ImGui::Separator();
							bool treeOpened = ImGui::TreeNodeEx("Graph Variables", treeFlags);
							ImGui::PopStyleVar();
							if (treeOpened)
							{
								UI::BeginPropertyGrid("Graph_Variables");

								bEntityChanged |= EditorResources::DrawGraphVariables(smComponent.GetAnimationGraph());

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

			case SelectedComponent::BillboardComponent:
			{
				DrawComponentTransformNode(entity, entity.GetComponent<BillboardComponent>());
				DrawComponent<BillboardComponent>("Billboard", entity, [&entity, this](BillboardComponent& billboard)
				{
					UI::BeginPropertyGrid("BillboardComponent");

					bEntityChanged |= EditorResources::DrawAssetSelection("Texture", billboard.TextureAsset);
					bEntityChanged |= UI::Property("Is Visible", billboard.bVisible);

					UI::EndPropertyGrid();
				});
				break;
			}

			case SelectedComponent::TextComponent:
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
					bool bVisible = component.IsVisible();
					Ref<AssetFont> asset = component.GetFontAsset();
					Ref<AssetMaterial> materialAsset = component.GetMaterialAsset();

					UI::BeginPropertyGrid("TextComponent");

					if (EditorResources::DrawAssetSelection("Font", asset))
					{
						component.SetFontAsset(asset);
						bEntityChanged = true;
					}

					if (UI::PropertyTextMultiline("Text", text))
					{
						component.SetText(text);
						bEntityChanged = true;
					}

					if (UI::Property("Is Visible", bVisible))
					{
						component.SetVisible(bVisible);
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
						if (EditorResources::DrawAssetSelection("Material", materialAsset))
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

						bool bDoubleSided = component.IsDoubleSided();
						if (UI::Property("Double Sided", bDoubleSided))
						{
							component.SetDoubleSided(bDoubleSided);
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
			
			case SelectedComponent::Text2DComponent:
			{
				DrawComponent<Text2DComponent>("Text 2D", entity, [&entity, this](Text2DComponent& component)
				{
					float lineSpacing = component.GetLineSpacing();
					float kerning = component.GetKerning();
					float maxWidth = component.GetMaxWidth();
					std::string text = component.GetText();
					Ref<AssetFont> asset = component.GetFontAsset();

					UI::BeginPropertyGrid("Text2DComponent");

					if (EditorResources::DrawAssetSelection("Font", asset))
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
			
			case SelectedComponent::Image2DComponent:
			{
				DrawComponent<Image2DComponent>("Image 2D", entity, [&entity, this](Image2DComponent& component)
				{
					Ref<AssetTexture2D> asset = component.GetTextureAsset();

					UI::BeginPropertyGrid("Image2DComponent");

					if (EditorResources::DrawAssetSelection("Texture", asset))
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

			case SelectedComponent::CameraComponent:
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
					if (UI::PropertyDrag("Shadow Far Clip", shadowFar, 1.f, 0.f, FLT_MAX, "If a light source is beyond this distance from the camera, its shadows won't be rendered (doesn't affect a directional light since it doesn't really have a position)"))
					{
						camera.SetShadowFarClip(shadowFar);
						bEntityChanged = true;
					}

					float dirLightShadowFar = camera.GetDirLightShadowFarClip();
					if (UI::PropertyDrag("Dir Light Shadow Far Clip", dirLightShadowFar, 1.f, 0.f, FLT_MAX, "Pixels beyond this distance from the camera won't receive shadows from a directional light"))
					{
						camera.SetDirLightShadowFarClip(dirLightShadowFar);
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

					bool bDebugFrustumCulling = cameraComponent.IsDebugFrustumCullingEnabled();
					if (UI::Property("Use for culling", bDebugFrustumCulling, "When enabled, this camera's frustum will be used for culling. This property is not saved. Use for debug purposes only"))
					{
						cameraComponent.SetDebugFrustumCullingEnabled(bDebugFrustumCulling);
					}
					
					UI::EndPropertyGrid();

					ImGui::Separator();
					if (ImGui::Button("Copy transform from the editor camera"))
					{
						const auto& scene = entity.GetScene();
						const auto& editorCamera = scene->EditorCamera;
						cameraComponent.SetWorldTransform(editorCamera.GetTransform());
					}
				});
				break;
			}

			case SelectedComponent::PointLightComponent:
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

					if (UI::PropertyDrag("Attenuation Radius", radius, 0.05f, 0.f, 0.f, s_AttenuationRadiusHelpMsg))
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

			case SelectedComponent::DirectionalLightComponent:
			{
				DrawComponentTransformNode(entity, entity.GetComponent<DirectionalLightComponent>());
				DrawComponent<DirectionalLightComponent>("Directional Light", entity, [&entity, this](DirectionalLightComponent& directionalLight)
				{
					glm::vec3 lightColor = directionalLight.GetLightColor();
					glm::vec3 ambientColor = directionalLight.GetAmbientColor();
					float intensity = directionalLight.GetIntensity();
					float fogIntensity = directionalLight.GetVolumetricFogIntensity();
					bool bAffectsWorld = directionalLight.DoesAffectWorld();
					bool bCastsShadows = directionalLight.DoesCastShadows();
					bool bCastsScreenSpaceShadows = directionalLight.DoesCastScreenSpaceShadows();
					bool bVolumetric = directionalLight.IsVolumetricLight();
					bool bVisualize = directionalLight.IsVisualizeDirectionEnabled();

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

					if (UI::PropertyColor("Ambient", ambientColor))
					{
						directionalLight.SetAmbientColor(ambientColor);
						bEntityChanged = true;
					}
						
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

					if (UI::Property("Casts screen space shadows", bCastsScreenSpaceShadows))
					{
						directionalLight.SetCastsScreenSpaceShadows(bCastsScreenSpaceShadows);
						bEntityChanged = true;
					}

					if (UI::Property("Visualize direction", bVisualize))
					{
						directionalLight.SetVisualizeDirectionEnabled(bVisualize);
						bEntityChanged = true;
					}

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

			case SelectedComponent::SpotLightComponent:
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
		
			case SelectedComponent::ScriptComponent:
			{
				DrawComponent<ScriptComponent>("C# Script", entity, [&entity, this](ScriptComponent& scriptComponent)
				{
					UI::BeginPropertyGrid("ScriptComponent");

					if (bRuntime)
						UI::PushItemDisabled();

					bool bModuleExists = ScriptEngine::ModuleExists(scriptComponent.ModuleName);
					const auto& scriptClasses = ScriptEngine::GetEntityClasses();

					if (!bModuleExists)
						UI::PushFrameBGColor({150.f, 0.f, 0.f, 255.f});

					if(UI::ComboWithNone("Script Class", scriptComponent.ModuleName, scriptClasses))
					{
						ScriptEngine::UpdateEntityPublicFields(entity);
						bEntityChanged = true;
					}
						
					if (!bModuleExists)
						UI::PopFrameBGColor();

					if (bRuntime)
						UI::PopItemDisabled();

					ImGui::Separator();
					if (ScriptEngine::ModuleExists(scriptComponent.ModuleName))
					{
						EntityInstance* entityInstance = ScriptEngine::GetEntityInstance(entity);
						for (auto& field : scriptComponent.PublicFields)
						{
							// Don't mark as changed during runtime
							bEntityChanged |= UI::Property(field, entityInstance ? entityInstance->GetMonoInstance() : nullptr, bRuntime, entity) && !bRuntime;
						}
					}

					UI::EndPropertyGrid();
				});
				break;
			}
		
			case SelectedComponent::RigidBodyComponent:
			{
				bool bCanRemove = !entity.HasAny<BoxColliderComponent, SphereColliderComponent, CapsuleColliderComponent, MeshColliderComponent>();
				DrawComponent<RigidBodyComponent>("Rigid Body", entity, [&entity, this](RigidBodyComponent& rigidBody)
				{
					UI::BeginPropertyGrid("RigidBodyComponent");

					PhysicsBodyType bodyType = rigidBody.GetBodyType();

					if (UI::ComboEnum<PhysicsBodyType>("Body type", bodyType))
					{
						rigidBody.SetBodyType(bodyType);
						bEntityChanged = true;
					}
						
					if (bodyType == PhysicsBodyType::Dynamic)
					{
						CollisionDetectionType collisionDetection = rigidBody.GetCollisionDetectionType();
						uint32_t positionSolverIterations = rigidBody.GetPositionSolverIterations();
						uint32_t velocitySolverIterations = rigidBody.GetVelocitySolverIterations();
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
							
						if (UI::ComboEnum<CollisionDetectionType>("Collision Detection", collisionDetection, s_CollisionDetectionTypeHelpMsg))
						{
							rigidBody.SetCollisionDetectionType(collisionDetection);
							bEntityChanged = true;
						}
							
						if (bRuntime)
							UI::PopItemDisabled();
							
						if (UI::PropertyDrag("Position Solver Iterations", positionSolverIterations, 1, PhysicsSettings::MinPositionSolverIterations, PhysicsSettings::MaxPositionSolverIterations, s_PositionSolverIterationsHelpMsg))
						{
							rigidBody.SetPositionSolverIterations(positionSolverIterations);
							bEntityChanged = true;
						}
							
						if (UI::PropertyDrag("Velocity Solver Iterations", velocitySolverIterations, 1, PhysicsSettings::MinVelocitySolverIterations, PhysicsSettings::MaxVelocitySolverIterations, s_VelocitySolverIterationsHelpMsg))
						{
							rigidBody.SetVelocitySolverIterations(velocitySolverIterations);
							bEntityChanged = true;
						}
							
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

			case SelectedComponent::BoxColliderComponent:
			{
				DrawComponentTransformNode(entity, entity.GetComponent<BoxColliderComponent>());
				DrawComponent<BoxColliderComponent>("Box Collider", entity, [&entity, this](BoxColliderComponent& collider)
				{
					UI::BeginPropertyGrid("BoxColliderComponent");

					Ref<AssetPhysicsMaterial> materialAsset = collider.GetPhysicsMaterialAsset();
					glm::vec3 size = collider.GetSize();
					bool bTrigger = collider.IsTrigger();
					bool bCollisionEnabled = collider.IsCollisionEnabled();
					bool bShowCollision = collider.IsCollisionVisible();
					bool bObstacle = collider.IsObstacle();
					bool bAffectsNavMesh = collider.DoesAffectNavMeshBuild();
					uint32_t collisionGroup = (uint32_t)collider.GetCollisionGroup();
					uint32_t interactingCollisionGroup = (uint32_t)collider.GetInteractingCollisionGroup();
					const auto& collisionGroups = Project::GetAllCollisionGroups();

					if (EditorResources::DrawAssetSelection("Physics Material", materialAsset))
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

					if (UI::Property("Collision Enabled", bCollisionEnabled))
					{
						collider.SetCollisionEnabled(bCollisionEnabled);
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

					constexpr float thickness = 2.5f;
					UI::TextWithSeparator("Collision Groups", thickness, "Collision groups it belongs to");
					if (UI::PropertyBitMask("Collision Groups", collisionGroup, collisionGroups))
					{
						collider.SetCollisionGroup(CollisionGroup(collisionGroup));
						bEntityChanged = true;
					}

					UI::TextWithSeparator("Interacting Collision Groups", thickness, "Collision groups it can interact with");
					if (UI::PropertyBitMask("Interacting Collision Groups", interactingCollisionGroup, collisionGroups))
					{
						collider.SetInteractingCollisionGroup(CollisionGroup(interactingCollisionGroup));
						bEntityChanged = true;
					}

					UI::EndPropertyGrid();
				});
				break;
			}

			case SelectedComponent::SphereColliderComponent:
			{
				DrawComponentTransformNode(entity, entity.GetComponent<SphereColliderComponent>());
				DrawComponent<SphereColliderComponent>("Sphere Collider", entity, [&entity, this](SphereColliderComponent& collider)
				{
					UI::BeginPropertyGrid("SphereColliderComponent");

					Ref<AssetPhysicsMaterial> materialAsset = collider.GetPhysicsMaterialAsset();
					float radius = collider.GetRadius();
					bool bTrigger = collider.IsTrigger();
					bool bCollisionEnabled = collider.IsCollisionEnabled();
					bool bShowCollision = collider.IsCollisionVisible();
					bool bObstacle = collider.IsObstacle();
					bool bAffectsNavMesh = collider.DoesAffectNavMeshBuild();
					uint32_t collisionGroup = (uint32_t)collider.GetCollisionGroup();
					uint32_t interactingCollisionGroup = (uint32_t)collider.GetInteractingCollisionGroup();
					const auto& collisionGroups = Project::GetAllCollisionGroups();

					if (EditorResources::DrawAssetSelection("Physics Material", materialAsset))
					{
						collider.SetPhysicsMaterialAsset(materialAsset);
						bEntityChanged = true;
					}

					if (UI::PropertyDrag("Radius", radius, 0.05f))
					{
						collider.SetRadius(radius);
						bEntityChanged = true;
					}

					if (UI::Property("Collision Enabled", bCollisionEnabled))
					{
						collider.SetCollisionEnabled(bCollisionEnabled);
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

					constexpr float thickness = 2.5f;
					UI::TextWithSeparator("Collision Groups", thickness, "Collision groups it belongs to");
					if (UI::PropertyBitMask("Collision Groups", collisionGroup, collisionGroups))
					{
						collider.SetCollisionGroup(CollisionGroup(collisionGroup));
						bEntityChanged = true;
					}

					UI::TextWithSeparator("Interacting Collision Groups", thickness, "Collision groups it can interact with");
					if (UI::PropertyBitMask("Interacting Collision Groups", interactingCollisionGroup, collisionGroups))
					{
						collider.SetInteractingCollisionGroup(CollisionGroup(interactingCollisionGroup));
						bEntityChanged = true;
					}
						
					UI::EndPropertyGrid();
				});
				break;
			}

			case SelectedComponent::CapsuleColliderComponent:
			{
				DrawComponentTransformNode(entity, entity.GetComponent<CapsuleColliderComponent>());
				DrawComponent<CapsuleColliderComponent>("Capsule Collider", entity, [&entity, this](CapsuleColliderComponent& collider)
				{
					UI::BeginPropertyGrid("CapsuleColliderComponent");

					Ref<AssetPhysicsMaterial> materialAsset = collider.GetPhysicsMaterialAsset();
					float height = collider.GetHeight();
					float radius = collider.GetRadius();
					bool bTrigger = collider.IsTrigger();
					bool bCollisionEnabled = collider.IsCollisionEnabled();
					bool bShowCollision = collider.IsCollisionVisible();
					bool bObstacle = collider.IsObstacle();
					bool bAffectsNavMesh = collider.DoesAffectNavMeshBuild();
					uint32_t collisionGroup = (uint32_t)collider.GetCollisionGroup();
					uint32_t interactingCollisionGroup = (uint32_t)collider.GetInteractingCollisionGroup();
					const auto& collisionGroups = Project::GetAllCollisionGroups();

					if (EditorResources::DrawAssetSelection("Physics Material", materialAsset))
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

					if (UI::Property("Collision Enabled", bCollisionEnabled))
					{
						collider.SetCollisionEnabled(bCollisionEnabled);
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

					constexpr float thickness = 2.5f;
					UI::TextWithSeparator("Collision Groups", thickness, "Collision groups it belongs to");
					if (UI::PropertyBitMask("Collision Groups", collisionGroup, collisionGroups))
					{
						collider.SetCollisionGroup(CollisionGroup(collisionGroup));
						bEntityChanged = true;
					}

					UI::TextWithSeparator("Interacting Collision Groups", thickness, "Collision groups it can interact with");
					if (UI::PropertyBitMask("Interacting Collision Groups", interactingCollisionGroup, collisionGroups))
					{
						collider.SetInteractingCollisionGroup(CollisionGroup(interactingCollisionGroup));
						bEntityChanged = true;
					}

					UI::EndPropertyGrid();
				});
				break;
			}

			case SelectedComponent::MeshColliderComponent:
			{
				DrawComponentTransformNode(entity, entity.GetComponent<MeshColliderComponent>());
				DrawComponent<MeshColliderComponent>("Mesh Collider", entity, [&entity, this](MeshColliderComponent& collider)
				{
					UI::BeginPropertyGrid("MeshColliderComponent");

					Ref<AssetPhysicsMaterial> materialAsset = collider.GetPhysicsMaterialAsset();
					Ref<AssetBaseMesh> collisionMesh = collider.GetCollisionMeshAsset();
					bool bTrigger = collider.IsTrigger();
					bool bCollisionEnabled = collider.IsCollisionEnabled();
					bool bShowCollision = collider.IsCollisionVisible();
					bool bConvex = collider.IsConvex();
					bool bTwoSided = collider.IsTwoSided();
					bool bAffectsNavMesh = collider.DoesAffectNavMeshBuild();
					uint32_t collisionGroup = (uint32_t)collider.GetCollisionGroup();
					uint32_t interactingCollisionGroup = (uint32_t)collider.GetInteractingCollisionGroup();
					const auto& collisionGroups = Project::GetAllCollisionGroups();

					if (EditorResources::DrawAssetSelection("Collision Mesh", collisionMesh, "Must be set. Set the mesh that will be used to generate collision data for it"))
					{
						collider.SetCollisionMeshAsset(collisionMesh);
						bEntityChanged = true;
					}

					if (EditorResources::DrawAssetSelection("Physics Material", materialAsset))
					{
						collider.SetPhysicsMaterialAsset(materialAsset);
						bEntityChanged = true;
					}

					if (UI::Property("Is Trigger", bTrigger, s_TriggerHelpMsg))
					{
						collider.SetIsTrigger(bTrigger);
						bEntityChanged = true;
					}

					if (UI::Property("Collision Enabled", bCollisionEnabled))
					{
						collider.SetCollisionEnabled(bCollisionEnabled);
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

					if (UI::Property("Is Collision Visible", bShowCollision))
					{
						collider.SetShowCollision(bShowCollision);
						bEntityChanged = true;
					}

					constexpr float thickness = 2.5f;
					UI::TextWithSeparator("Collision Groups", thickness, "Collision groups it belongs to");
					if (UI::PropertyBitMask("Collision Groups", collisionGroup, collisionGroups))
					{
						collider.SetCollisionGroup(CollisionGroup(collisionGroup));
						bEntityChanged = true;
					}

					UI::TextWithSeparator("Interacting Collision Groups", thickness, "Collision groups it can interact with");
					if (UI::PropertyBitMask("Interacting Collision Groups", interactingCollisionGroup, collisionGroups))
					{
						collider.SetInteractingCollisionGroup(CollisionGroup(interactingCollisionGroup));
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
					float pitch = audio.GetPitch();
					float pan = audio.GetPan();
					int loopCount = audio.GetLoopCount();
					uint32_t fftSamples = audio.GetFFTSamples();
					FFTWindowType fftType = audio.GetFFTType();
					bool bLooping = audio.IsLooping();
					bool bMuted = audio.IsMuted();
					bool bStreaming = audio.IsStreaming();
					bool bFFTEnabled = audio.IsFFTEnabled();
					float minDistance = audio.GetMinDistance();
					float maxDistance = audio.GetMaxDistance();
					uint32_t currentRollOff = (uint32_t)audio.GetRollOffModel();
					bool b3D = audio.Is3D();

					if (EditorResources::DrawAssetSelection("Audio", asset))
					{
						audio.SetAudioAsset(asset);
						bEntityChanged = true;
					}

					if (UI::Property("Is 3D", b3D))
					{
						audio.SetIs3D(b3D);
						bEntityChanged = true;
					}

					if (UI::Combo("Roll off", currentRollOff, rollOffModels, inSelectedRollOffModel, rollOffToolTips))
					{
						audio.SetRollOffModel(RollOffModel(inSelectedRollOffModel));
						bEntityChanged = true;
					}

					if (UI::PropertyDrag("Volume", volume, 0.05f))
					{
						volume = glm::max(volume, 0.f);
						audio.SetVolume(volume);
						bEntityChanged = true;
					}

					if (UI::PropertySlider("Pitch", pitch, 0.f, 10.f))
					{
						audio.SetPitch(pitch);
						bEntityChanged = true;
					}

					if (UI::PropertySlider("Pan", pan, -1.f, 1.f))
					{
						audio.SetPan(pan);
						bEntityChanged = true;
					}

					if (UI::PropertyDrag("Loop Count", loopCount, 1.f, -1, 10, "-1 = Loop Endlessly; 0 = Play once; 1 = Play twice, etc..."))
					{
						audio.SetLoopCount(loopCount);
						bEntityChanged = true;
					}

					if (UI::Property("Is Looping", bLooping))
					{
						audio.SetLooping(bLooping);
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
						
					if (UI::Property("Is Streaming", bStreaming, "When you stream a sound, you can only have one instance of it playing at any time."
						" This limitation exists because there is only one decode buffer per stream."
						" As a rule of thumb, streaming is great for music tracks, voice cues, and ambient tracks,"
						" while most sound effects should be loaded into memory"))
					{
						audio.SetStreaming(bStreaming);
						bEntityChanged = true;
					}

					if (UI::Property("Is Muted", bMuted))
					{
						audio.SetMuted(bMuted);
						bEntityChanged = true;
					}

					if (UI::PropertyDrag("FFT Samples", fftSamples, 1.f, 0, 0, "Must be a power of 2 between 64 and 8192"))
					{
						audio.SetFFTSamples(fftSamples);
						bEntityChanged = true;
					}

					if (UI::ComboEnum("FFT Type", fftType, s_FFTTypeHelpMsg))
					{
						audio.SetFFTType(fftType);
						bEntityChanged = true;
					}

					if (UI::Property("FFT Enabled", bFFTEnabled, "If enabled, you can extract spectrum data of the sound"))
					{
						audio.SetFFTEnabled(bFFTEnabled);
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
		
			case SelectedComponent::ParticleSystemComponent:
			{
				DrawComponentTransformNode(entity, entity.GetComponent<ParticleSystemComponent>());
				DrawComponent<ParticleSystemComponent>("Particle System", entity, [&entity, this](ParticleSystemComponent& system)
				{
					auto asset = system.GetAsset();

					UI::BeginPropertyGrid("ParticleSystemComponent");

					if (EditorResources::DrawAssetSelection("Particle System", asset))
					{
						system.SetAsset(asset);
						bEntityChanged = true;
					}
					bEntityChanged |= UI::Property("Auto-spawn", system.bAutospawn);

					UI::EndPropertyGrid();
				});
				break;
			}

			case SelectedComponent::DecalComponent:
			{
				DrawComponentTransformNode(entity, entity.GetComponent<DecalComponent>());
				DrawComponent<DecalComponent>("Decal", entity, [&entity, this](DecalComponent& decal)
				{
					UI::BeginPropertyGrid("DecalComponent");

					auto materialAsset = decal.GetMaterialAsset();
					uint32_t sortPriority = decal.GetSortPriority();
					bool bAdjustAspectRatio = decal.IsAdjustAspectRatioEnabled();
					bool bVisible = decal.IsVisible();

					if (EditorResources::DrawAssetSelection("Material", materialAsset, "Material data will be blended with the underlying material based on 'Opacity'"))
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
					if (UI::Property("Is Visible", bVisible))
					{
						decal.SetVisible(bVisible);
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
					UI::BeginPropertyGrid("NavigationMeshComponent");

					if (UI::Button("Build", "Build"))
					{
						entity.GetScene()->BuildNavMesh(&component);
						bEntityChanged = true;
					}
					UI::Property("Auto Rebuild", component.bAutoRebuild, "If enabled, nav mesh is rebuilt automatically when its transform or settings are changed");
					
					UI::TextWithSeparator("Crowd Settings");
					{
						auto settings = component.GetCrowdSettings();
						bool bCrowdChanged = false;

						bCrowdChanged |= UI::PropertyDrag("Max Agents", settings.MaxAgents);

						if (UI::PropertyDrag("Max Agent Radius", settings.MaxAgentRadius, 0.05f))
						{
							settings.MaxAgentRadius = glm::max(settings.MaxAgentRadius, 0.0f);
							bCrowdChanged = true;
						}

						if (bCrowdChanged)
						{
							component.SetCrowdSettings(settings);
							bEntityChanged = true;
						}
					}

					UI::TextWithSeparator("Nav Mesh Settings");
					auto settings = component.GetSettings();
					bool bChanged = false;

					bChanged |= UI::PropertyDrag("AABB Min", settings.AABB.Min, 0.1f, 0, 0);
					bChanged |= UI::PropertyDrag("AABB Max", settings.AABB.Max, 0.1f, 0, 0);

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
					if (UI::PropertyDrag("Cell Height", settings.CellHeight, 0.01f, 0, 0, "The y-axis cell size to use for fields"))
					{
						settings.CellHeight = glm::max(settings.CellHeight, 0.001f);
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

			case SelectedComponent::NavigationCrowdAgentComponent:
			{
				DrawComponent<NavigationCrowdAgentComponent>("Navigation Crowd Agent", entity, [&entity, this](NavigationCrowdAgentComponent& component)
				{
					auto settings = component.GetSettings();
					bool bChanged = false;

					UI::BeginPropertyGrid("NavigationCrowdAgentComponent");
					
					if (UI::PropertyDrag("Agent Radius", settings.AgentRadius, 0.05f))
					{
						settings.AgentRadius = glm::max(settings.AgentRadius, 0.1f);
						bChanged = true;
					}

					if (UI::PropertyDrag("Agent Height", settings.AgentHeight, 0.05f))
					{
						settings.AgentHeight = glm::max(settings.AgentHeight, 0.1f);
						bChanged = true;
					}

					if (UI::PropertyDrag("Max Acceleration", settings.MaxAcceleration, 0.1f))
					{
						settings.MaxAcceleration = glm::max(settings.MaxAcceleration, 0.1f);
						bChanged = true;
					}

					if (UI::PropertyDrag("Max Speed", settings.MaxSpeed, 0.1f))
					{
						settings.MaxSpeed = glm::max(settings.MaxSpeed, 0.1f);
						bChanged = true;
					}

					if (UI::PropertyDrag("Separation Weight", settings.SeparationWeight, 0.1f))
					{
						settings.SeparationWeight = glm::max(settings.SeparationWeight, 0.1f);
						bChanged = true;
					}

					bChanged |= UI::ComboEnum("Obstacle Avoidance Quality", settings.ObstacleAvoidanceQuality);
					
					bChanged |= UI::Property("Anticipate Turns", settings.bAnticipateTurns);
					bChanged |= UI::Property("Optimize Path Visibility", settings.bOptimizeVis);
					bChanged |= UI::Property("Optimize Path Topology", settings.bOptimizeTopo);
					bChanged |= UI::Property("Crowd Separation", settings.bSeparation);

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

		ImGui::PopID();
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

		DrawComponent<TransformComponent>(bUseRelativeTransform ? "Transform (relative)" : "Transform", entity, [&transform, &bValueChanged](auto& transformComponent)
		{
			glm::quat quat = transform.Rotation.GetQuat();

			bValueChanged |= UI::DrawVec3Control("Location", transform.Location, glm::vec3{ 0.f });
			if (UI::DrawQuatControl("Rotation (Quat)", quat))
			{
				transform.Rotation = quat;
				bValueChanged = true;
			}
			bValueChanged |= UI::DrawVec3Control("Scale", transform.Scale3D, glm::vec3{ 1.f });
		}, false);

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
			glm::quat quat = relativeTranform.Rotation.GetQuat();

			bValueChanged |= UI::DrawVec3Control("Location", relativeTranform.Location, glm::vec3{0.f});
			if (UI::DrawQuatControl("Rotation (Quat)", quat))
			{
				relativeTranform.Rotation = quat;
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
