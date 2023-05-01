#include "SceneHierarchyPanel.h"

#include "../EditorLayer.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <glm/gtc/type_ptr.hpp>
#include <Eagle/UI/UI.h>
#include <Eagle/Script/ScriptEngine.h>
#include <filesystem>

namespace Eagle
{
	static const char* s_MetallnessHelpMsg = "Controls how 'metal-like' surface looks like.\nDefault is 0";
	static const char* s_RoughnessHelpMsg = "Controls how rough surface looks like.\nRoughness of 0 is a mirror reflection and 1 is completely matte.\nDefault is 0.5";
	static const char* s_AOHelpMsg = "Simulates self-shadowing. Default is 1.0";
	static const char* s_StaticFrictionHelpMsg = "Static friction defines the amount of friction that is applied between surfaces that are not moving lateral to each-other";
	static const char* s_DynamicFrictionHelpMsg = "Dynamic friction defines the amount of friction applied between surfaces that are moving relative to each-other";
	static const char* s_TriggerHelpMsg = "Its role is to report that there has been an overlap with another shape.\nTrigger shapes play no part in the simulation of the scene";
	static const char* s_AttenuationRadiusHelpMsg = "Bounds the light's visible influence.\nThis clamping of the light's influence is not physically correct but very important for performance";

	SceneHierarchyPanel::SceneHierarchyPanel(const EditorLayer& editor) : m_Editor(editor)
	{}

	SceneHierarchyPanel::SceneHierarchyPanel(const EditorLayer& editor, const Ref<Scene>& scene) : m_Editor(editor)
	{
		SetContext(scene);
	}

	void SceneHierarchyPanel::SetContext(const Ref<Scene>& scene)
	{
		ClearSelection();
		m_InvertEntityRotation.clear();
		m_InvertComponentRotation.clear();
		m_Scene = scene;
	}

	void SceneHierarchyPanel::ClearSelection()
	{
		m_SelectedEntity = Entity::Null;
		m_SelectedComponent = SelectedComponent::None;
	}

	void SceneHierarchyPanel::SetEntitySelected(int entityID)
	{
		ClearSelection();

		if (entityID >= 0)
		{
			entt::entity enttID = (entt::entity)entityID;
			m_SelectedEntity = Entity{enttID, m_Scene.get()};
			m_SelectedComponent = SelectedComponent::None;
			m_ScrollToSelected = true;
		}
	}

	void SceneHierarchyPanel::OnImGuiRender()
	{
		DrawSceneHierarchy();
		
		ImGui::Begin("Properties");
		m_PropertiesHovered = ImGui::IsWindowHovered();
		if (m_SelectedEntity)
		{
			DrawComponents(m_SelectedEntity);
		}
		ImGui::End(); //Properties
	}

	void SceneHierarchyPanel::DrawSceneHierarchy()
	{
		ImGui::Begin("Scene Hierarchy");
		m_SceneHierarchyHovered = ImGui::IsWindowHovered();
		m_SceneHierarchyFocused = ImGui::IsWindowFocused();
		//TODO: Replace to "Drop on empty space"
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_CELL"))
			{
				uint32_t payload_n = *(uint32_t*)payload->Data;

				Entity droppedEntity((entt::entity)payload_n, m_Scene.get());
				droppedEntity.SetParent(Entity::Null);
			}

			ImGui::EndDragDropTarget();
		}

		auto view = m_Scene->GetAllEntitiesWith<EntitySceneNameComponent>();
		for (auto& entity : view)
		{
			DrawEntityNode(Entity(entity, m_Scene.get()));
		}

		if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered())
		{
			ClearSelection();
		}

		//Right-click on empty space in Scene Hierarchy
		if (ImGui::BeginPopupContextWindow("SceneHierarchyCreateEntity", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverExistingPopup | ImGuiPopupFlags_NoOpenOverItems))
		{
			if (ImGui::MenuItem("Create Entity"))
			{
				m_Scene->CreateEntity("Empty Entity");
				EG_EDITOR_TRACE("Created Entity");
			}

			ImGui::EndPopup();
		}

		ImGui::End(); //Scene Hierarchy
	}

	static bool isRelativeOf(const Entity& parent, const Entity& child)
	{
		bool bResult = false;
		bResult = parent.IsParentOf(child);
		if (bResult)
			return true;

		auto& children = parent.GetChildren();
		for (auto& myChild : children)
		{
			bResult = isRelativeOf(myChild, child);
			if (bResult)
				return true;
		}
		return false;
	}

	void SceneHierarchyPanel::DrawEntityNode(Entity& entity)
	{
		if (entity.HasParent()) //For drawing children use DrawChilds
			return;

		const auto& entityName = entity.GetComponent<EntitySceneNameComponent>().Name;

		//If selected child of this entity, open tree node
		if (m_SelectedEntity && m_SelectedEntity != entity)
		{
			if (isRelativeOf(entity, m_SelectedEntity))
				ImGui::SetNextItemOpen(true);
		}
		if (m_SelectedEntity == entity && m_ScrollToSelected)
		{
			m_ScrollToSelected = false;
			ImGui::SetScrollHereY(0.f);
		}

		ImGuiTreeNodeFlags flags = (m_SelectedEntity == entity ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_OpenOnArrow
			| ImGuiTreeNodeFlags_SpanAvailWidth | (entity.HasChildren() ? 0 : ImGuiTreeNodeFlags_Leaf);
		bool opened = ImGui::TreeNodeEx((void*)(uint64_t)entity.GetID(), flags, entityName.c_str());

		if (!ImGui::IsItemVisible()) // Early-exit if it's not visible
		{
			if (opened)
			{
				DrawChilds(entity);
				ImGui::TreePop();
			}
			return;
		}

		if (ImGui::IsItemClicked())
		{
			ClearSelection();
			m_SelectedEntity = entity;
		}

		if (ImGui::BeginPopupContextItem())
		{
			if (ImGui::MenuItem("Delete Entity"))
			{
				if (m_SelectedEntity == entity)
					ClearSelection();
				m_Scene->DestroyEntity(entity);
			}
			ImGui::EndPopup();
		}

		if (ImGui::BeginDragDropSource())
		{
			uint32_t selectedEntityID = m_SelectedEntity.GetID();
			const auto& selectedEntityName = entity.GetComponent<EntitySceneNameComponent>().Name;

			ImGui::SetDragDropPayload("ENTITY_CELL", &selectedEntityID, sizeof(uint32_t));
			ImGui::Text(selectedEntityName.c_str());

			ImGui::EndDragDropSource();
		}

		if (ImGui::BeginDragDropTarget()) 
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_CELL"))
			{
				uint32_t payload_n = *(uint32_t*)payload->Data;

				Entity droppedEntity((entt::entity)payload_n, m_Scene.get());
				if (droppedEntity.GetParent() != entity)
					droppedEntity.SetParent(entity);
			}

			ImGui::EndDragDropTarget();
		}
	
		if (opened)
		{
			DrawChilds(entity);
			ImGui::TreePop();
		}
	}

	void SceneHierarchyPanel::DrawChilds(Entity& entity)
	{
		auto& children = entity.GetComponent<OwnershipComponent>().Children;

		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;

		for (int i = 0; i < children.size(); ++i)
		{
			Entity& child = children[i];
			ImGuiTreeNodeFlags childTreeFlags = flags | (child.HasChildren() ? 0 : ImGuiTreeNodeFlags_Leaf) | (m_SelectedEntity == child ? ImGuiTreeNodeFlags_Selected : 0);

			//If selected child of this entity, open tree node
			if (m_SelectedEntity && m_SelectedEntity != child)
				if (isRelativeOf(child, m_SelectedEntity))
					ImGui::SetNextItemOpen(true);
			if (m_SelectedEntity == child && m_ScrollToSelected)
			{
				m_ScrollToSelected = false;
				ImGui::SetScrollHereY(0.f);
			}

			const auto& childName = child.GetComponent<EntitySceneNameComponent>().Name;
			bool openedChild = ImGui::TreeNodeEx((void*)(uint64_t)child.GetID(), childTreeFlags, childName.c_str());

			if (!ImGui::IsItemVisible()) // Early-exit
			{
				if (openedChild)
				{
					DrawChilds(child);
					ImGui::TreePop();
				}
				continue;
			}

			if (ImGui::IsItemClicked())
			{
				ClearSelection();
				m_SelectedEntity = child;
			}

			std::string popupID = std::to_string(child.GetID());
			if (ImGui::BeginPopupContextItem(popupID.c_str()))
			{
				if (ImGui::MenuItem("Delete Entity"))
				{
					if (m_SelectedEntity == child)
						ClearSelection();
					m_Scene->DestroyEntity(child);
				}

				ImGui::EndPopup();
			}

			if (ImGui::BeginDragDropSource())
			{
				uint32_t selectedEntityID = m_SelectedEntity.GetID();
				const auto& selectedEntityName = child.GetComponent<EntitySceneNameComponent>().Name;

				ImGui::SetDragDropPayload("ENTITY_CELL", &selectedEntityID, sizeof(uint32_t));
				ImGui::Text(selectedEntityName.c_str());

				ImGui::EndDragDropSource();
			}

			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_CELL"))
				{
					uint32_t payload_n = *(uint32_t*)payload->Data;

					Entity droppedEntity((entt::entity)payload_n, m_Scene.get());
					
					if (droppedEntity.GetParent() != child)
						droppedEntity.SetParent(child);
				}

				ImGui::EndDragDropTarget();
			}
		
			if (openedChild)
			{
				DrawChilds(child);
				ImGui::TreePop();
			}
		}
	}

	void SceneHierarchyPanel::DrawComponents(Entity& entity)
	{
		auto& entityName = entity.GetComponent<EntitySceneNameComponent>().Name;
		char buffer[256];
		memset(buffer, 0, sizeof(buffer));
		strncpy_s(buffer, entityName.c_str(), sizeof(buffer));
		if (ImGui::InputText("##Name", buffer, sizeof(buffer)))
		{
			//TODO: Add Check for empty input
			entityName = std::string(buffer);
		}
		
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
		const bool bRuntime = (m_Editor.GetEditorState() == EditorState::Play);
		switch (m_SelectedComponent)
		{
			case SelectedComponent::Sprite:
			{
				DrawComponentTransformNode(entity, entity.GetComponent<SpriteComponent>());
				DrawComponent<SpriteComponent>("Sprite", entity, [&entity, this](SpriteComponent& sprite)
				{
					UI::BeginPropertyGrid("SpriteComponent");

					UI::Property("Is SubTexture?", sprite.bSubTexture);
					
					auto& material = sprite.Material;

					if (sprite.bSubTexture)
					{
						Ref<Texture2D> atlasTexture;
						if (sprite.SubTexture)
							atlasTexture = sprite.SubTexture->GetTexture();

						if (UI::DrawTexture2DSelection("Atlas", atlasTexture))
						{
							if (atlasTexture)
								sprite.SubTexture = SubTexture2D::CreateFromCoords(atlasTexture, sprite.SubTextureCoords, sprite.SpriteSize, sprite.SpriteSizeCoef);
							else
								sprite.SubTexture.reset();
						}

						bool bChanged = false;
						bChanged |= UI::PropertyDrag("SubTexture Coords", sprite.SubTextureCoords);
						bChanged |= UI::PropertyDrag("Sprite Size", sprite.SpriteSize);
						bChanged |= UI::PropertyDrag("Sprite Size Coef", sprite.SpriteSizeCoef);

						if (sprite.SubTexture)
						{
							if (bChanged)
							{
								sprite.SubTexture->UpdateCoords(sprite.SubTextureCoords, sprite.SpriteSize, sprite.SpriteSizeCoef);
							}

							const auto& atlasTexture = sprite.SubTexture->GetTexture();
							if (atlasTexture)
							{
								glm::vec2 atlasSize = atlasTexture->GetSize();
								std::string atlasSizeString = std::to_string((int)atlasSize.x) + "x" + std::to_string((int)atlasSize.y);
								UI::Text("Atlas size", atlasSizeString);
							}
							else
								UI::Text("Atlas size", "None");
						}
					}
					else
					{
						Ref<Texture2D> albedo = material->GetAlbedoTexture();
						if (UI::DrawTexture2DSelection("Albedo", albedo))
							material->SetAlbedoTexture(albedo);

						Ref<Texture2D> metallic = material->GetMetallnessTexture();
						if (UI::DrawTexture2DSelection("Metallness", metallic, s_MetallnessHelpMsg))
							material->SetMetallnessTexture(metallic);

						Ref<Texture2D> normal = material->GetNormalTexture();
						if (UI::DrawTexture2DSelection("Normal", normal))
							material->SetNormalTexture(normal);

						Ref<Texture2D> roughness = material->GetRoughnessTexture();
						if (UI::DrawTexture2DSelection("Roughness", roughness, s_RoughnessHelpMsg))
							material->SetRoughnessTexture(roughness);

						Ref<Texture2D> ao = material->GetAOTexture();
						if (UI::DrawTexture2DSelection("AO", ao, s_AOHelpMsg))
							material->SetAOTexture(ao);

						Ref<Texture2D> emissive = material->GetEmissiveTexture();
						if (UI::DrawTexture2DSelection("Emissive Color", emissive))
							material->SetEmissiveTexture(emissive);

						UI::PropertyColor("Emissive Intensity", material->EmissiveIntensity, true);
					}

					UI::PropertyColor("Tint Color", material->TintColor);
					UI::PropertySlider("Tiling Factor", material->TilingFactor, 1.f, 10.f);
						 
					UI::EndPropertyGrid();
				});
				break;
			}

			case SelectedComponent::StaticMesh:
			{
				DrawComponentTransformNode(entity, entity.GetComponent<StaticMeshComponent>());
				DrawComponent<StaticMeshComponent>("Static Mesh", entity, [&entity, this](auto& smComponent)
					{
						UI::BeginPropertyGrid("StaticMeshComponent");
						Ref<StaticMesh> staticMesh = smComponent.GetStaticMesh();

						if (UI::DrawStaticMeshSelection("Static Mesh", staticMesh))
							smComponent.SetStaticMesh(staticMesh);

						if (staticMesh)
						{
							auto& material = smComponent.Material;

							Ref<Texture2D> temp = material->GetAlbedoTexture();
							if (UI::DrawTexture2DSelection("Albedo", temp))
								material->SetAlbedoTexture(temp);

							temp = material->GetMetallnessTexture();
							if (UI::DrawTexture2DSelection("Metallness", temp, s_MetallnessHelpMsg))
								material->SetMetallnessTexture(temp);

							temp = material->GetNormalTexture();
							if (UI::DrawTexture2DSelection("Normal", temp))
								material->SetNormalTexture(temp);

							temp = material->GetRoughnessTexture();
							if (UI::DrawTexture2DSelection("Roughness", temp, s_RoughnessHelpMsg))
								material->SetRoughnessTexture(temp);

							temp = material->GetAOTexture();
							if (UI::DrawTexture2DSelection("AO", temp, s_AOHelpMsg))
								material->SetAOTexture(temp);

							temp = material->GetEmissiveTexture();
							if (UI::DrawTexture2DSelection("Emissive Color", temp))
								material->SetEmissiveTexture(temp);

							UI::PropertyColor("Emissive Intensity", material->EmissiveIntensity, true);
							UI::PropertyColor("Tint Color", material->TintColor);
							UI::PropertySlider("Tiling Factor", material->TilingFactor, 1.f, 128.f);
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

					UI::DrawTexture2DSelection("Texture", billboard.Texture);

					UI::EndPropertyGrid();
				});
				break;
			}

			case SelectedComponent::Text3D:
			{
				DrawComponentTransformNode(entity, entity.GetComponent<TextComponent>());
				DrawComponent<TextComponent>("Text", entity, [&entity, this](TextComponent& component)
				{
					glm::vec3 color = component.GetColor();
					float lineSpacing = component.GetLineSpacing();
					float kerning = component.GetKerning();
					float maxWidth = component.GetMaxWidth();
					std::string text = component.GetText();
					Ref<Font> font = component.GetFont();

					UI::BeginPropertyGrid("TextComponent");

					if (UI::DrawFontSelection("Font", font))
						component.SetFont(font);

					if (UI::PropertyTextMultiline("Text", text))
						component.SetText(text);

					if (UI::PropertyColor("Color", color, true))
						component.SetColor(color);
					if (UI::PropertyDrag("Line Spacing", lineSpacing, 0.1f))
						component.SetLineSpacing(lineSpacing);
					if (UI::PropertyDrag("Kerning", kerning, 0.1f))
						component.SetKerning(kerning);
					if (UI::PropertyDrag("Max Width", maxWidth, 0.1f))
						component.SetMaxWidth(maxWidth);

					UI::EndPropertyGrid();
				});
				break;
			}

			case SelectedComponent::Camera:
			{
				DrawComponentTransformNode(entity, entity.GetComponent<CameraComponent>());
				DrawComponent<CameraComponent>("Camera", entity, [&entity, this](auto& cameraComponent)
				{
					UI::BeginPropertyGrid("CameraComponent");
					auto& camera = cameraComponent.Camera;

					UI::Property("Primary", cameraComponent.Primary);

					static std::vector<std::string> projectionModesStrings = { "Perspective", "Orthographic" };

					int selectedIndex = 0;
					if (UI::Combo("Projection", (uint32_t)camera.GetProjectionMode(), projectionModesStrings, selectedIndex))
					{
						camera.SetProjectionMode((CameraProjectionMode)selectedIndex);
					}

					if (camera.GetProjectionMode() == CameraProjectionMode::Perspective)
					{
						float verticalFov = glm::degrees(camera.GetPerspectiveVerticalFOV());
						if (UI::PropertyDrag("Vertical FOV", verticalFov))
						{
							camera.SetPerspectiveVerticalFOV(glm::radians(verticalFov));
						}

						float perspectiveNear = camera.GetPerspectiveNearClip();
						if (UI::PropertyDrag("Near Clip", perspectiveNear))
						{
							camera.SetPerspectiveNearClip(perspectiveNear);
						}

						float perspectiveFar = camera.GetPerspectiveFarClip();
						if (UI::PropertyDrag("Far Clip", perspectiveFar))
						{
							camera.SetPerspectiveFarClip(perspectiveFar);
						}
					}
					else
					{
						float size = camera.GetOrthographicSize();
						if (UI::PropertyDrag("Size", size))
						{
							camera.SetOrthographicSize(size);
						}

						float orthoNear = camera.GetOrthographicNearClip();
						if (UI::PropertyDrag("Near Clip", orthoNear))
						{
							camera.SetOrthographicNearClip(orthoNear);
						}

						float orthoFar = camera.GetOrthographicFarClip();
						if (UI::PropertyDrag("Far Clip", orthoFar))
						{
							camera.SetOrthographicFarClip(orthoFar);
						}

						UI::Property("Fixed Aspect Ratio", cameraComponent.FixedAspectRatio);
					}
					
					UI::EndPropertyGrid();
				});
				break;
			}

			case SelectedComponent::PointLight:
			{
				DrawComponentTransformNode(entity, entity.GetComponent<PointLightComponent>());
				DrawComponent<PointLightComponent>("Point Light", entity, [&entity, this](auto& pointLight)
					{
						glm::vec3 lightColor = pointLight.GetLightColor();
						float intensity = pointLight.GetIntensity();
						float radius = pointLight.GetRadius();
						bool bAffectsWorld = pointLight.DoesAffectWorld();
						bool bVisualizeRadius = pointLight.VisualizeRadiusEnabled();

						UI::BeginPropertyGrid("PointLightComponent");
						if (UI::PropertyColor("Light Color", lightColor))
							pointLight.SetLightColor(lightColor);
						if (UI::PropertyDrag("Intensity", intensity, 0.1f, 0.f))
							pointLight.SetIntensity(intensity);
						if (UI::PropertyDrag("Attenuation Radius", radius, 0.1f, 0.f, 0.f, s_AttenuationRadiusHelpMsg))
							pointLight.SetRadius(radius);
						if (UI::Property("Affects World", bAffectsWorld))
							pointLight.SetAffectsWorld(bAffectsWorld);
						if (UI::Property("Visualize Radius", bVisualizeRadius))
							pointLight.SetVisualizeRadiusEnabled(bVisualizeRadius);
						UI::EndPropertyGrid();
					});
				break;
			}

			case SelectedComponent::DirectionalLight:
			{
				DrawComponentTransformNode(entity, entity.GetComponent<DirectionalLightComponent>());
				DrawComponent<DirectionalLightComponent>("Directional Light", entity, [&entity, this](auto& directionalLight)
					{
						glm::vec3 lightColor = directionalLight.GetLightColor();
						float intensity = directionalLight.GetIntensity();
						bool bAffectsWorld = directionalLight.DoesAffectWorld();
						UI::BeginPropertyGrid("DirectionalLightComponent");
						if (UI::PropertyColor("Light Color", lightColor))
							directionalLight.SetLightColor(lightColor);
						if (UI::PropertyDrag("Intensity", intensity, 0.1f, 0.f))
							directionalLight.SetIntensity(intensity);
						if (UI::Property("Affects world", bAffectsWorld))
							directionalLight.SetAffectsWorld(bAffectsWorld);
						UI::EndPropertyGrid();
					});
				break;
			}

			case SelectedComponent::SpotLight:
			{
				DrawComponentTransformNode(entity, entity.GetComponent<SpotLightComponent>());
				DrawComponent<SpotLightComponent>("Spot Light", entity, [&entity, this](auto& spotLight)
					{
						glm::vec3 lightColor = spotLight.GetLightColor();
						float intensity = spotLight.GetIntensity();
						float inner = spotLight.GetInnerCutOffAngle();
						float outer = spotLight.GetOuterCutOffAngle();
						float distance = spotLight.GetDistance();
						bool bVisualizeDistance = spotLight.VisualizeDistanceEnabled();
						bool bAffectsWorld = spotLight.DoesAffectWorld();

						UI::BeginPropertyGrid("SpotLightComponent");
						if (UI::PropertyColor("Light Color", lightColor))
							spotLight.SetLightColor(lightColor);
						if (UI::PropertySlider("Inner Angle", inner, 1.f, 80.f))
							spotLight.SetInnerCutOffAngle(inner);
						if (UI::PropertySlider("Outer Angle", outer, 1.f, 80.f))
							spotLight.SetOuterCutOffAngle(outer);
						if (UI::PropertyDrag("Intensity", intensity, 0.1f, 0.f))
							spotLight.SetIntensity(intensity);
						if (UI::PropertyDrag("Attenuation Distance", distance, 0.1f, 0.f, 0.f, s_AttenuationRadiusHelpMsg))
							spotLight.SetDistance(distance);
						if (UI::Property("Affects world", bAffectsWorld))
							spotLight.SetAffectsWorld(bAffectsWorld);
						if (UI::Property("Visualize Distance", bVisualizeDistance))
							spotLight.SetVisualizeDistanceEnabled(bVisualizeDistance);
						UI::EndPropertyGrid();
					});
				break;
			}
		
			case SelectedComponent::Script:
			{
				DrawComponent<ScriptComponent>("C# Script", entity, [&entity, this, bRuntime](auto& scriptComponent)
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
						}
						
						if (!bModuleExists)
							UI::PopFrameBGColor();

						if (bRuntime)
							UI::PopItemDisabled();

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
									}
									break;
								}
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
				DrawComponentTransformNode(entity, entity.GetComponent<RigidBodyComponent>());
				DrawComponent<RigidBodyComponent>("Rigid Body", entity, [&entity, this, bRuntime](auto& rigidBody)
					{
						static const std::vector<std::string> bodyTypes = { "Static", "Dynamic" };
						static const std::vector<std::string> collisionDetectionTypes = { "Discrete", "Continuous", "Continuous Speculative" };
						static const std::vector<std::string> lockStrings = { "X", "Y", "Z" };
						
						int inSelectedBodyType = 0;
						int inSelectedCollisionType = 0;

						UI::BeginPropertyGrid("RigidBodyComponent");

						if (bRuntime)
							UI::PushItemDisabled();

						if (UI::Combo("Body type", (uint32_t)rigidBody.BodyType, bodyTypes, inSelectedBodyType))
							rigidBody.BodyType = RigidBodyComponent::Type(inSelectedBodyType);

						if (bRuntime)
							UI::PopItemDisabled();
						
						if (rigidBody.BodyType == RigidBodyComponent::Type::Dynamic)
						{
							float mass = rigidBody.GetMass();
							float linearDamping = rigidBody.GetLinearDamping();
							float angularDamping = rigidBody.GetAngularDamping();
							bool bEnableGravity = rigidBody.IsGravityEnabled();
							bool bKinematic = rigidBody.IsKinematic();
							bool bLockPositions[3] = { rigidBody.IsPositionXLocked(), rigidBody.IsPositionYLocked(), rigidBody.IsPositionZLocked() };
							bool bLockRotations[3] = { rigidBody.IsRotationXLocked(), rigidBody.IsRotationYLocked(), rigidBody.IsRotationZLocked() };

							if (bRuntime)
								UI::PushItemDisabled();
							if (UI::Combo("Collision Detection", (uint32_t)rigidBody.CollisionDetection, collisionDetectionTypes, inSelectedCollisionType, {}, "When continuous collision detection (or CCD) is turned on, the affected rigid bodies will not go through other objects at high velocities (a problem also known as tunnelling). A cheaper but less robust approach is called speculative CCD"))
								rigidBody.CollisionDetection = RigidBodyComponent::CollisionDetectionType(inSelectedCollisionType);
							if (bRuntime)
								UI::PopItemDisabled();
							
							if (UI::PropertyDrag("Mass", mass, 0.1f))
								rigidBody.SetMass(mass);
							if (UI::PropertyDrag("Linear Damping", linearDamping, 0.1f))
								rigidBody.SetLinearDamping(linearDamping);
							if (UI::PropertyDrag("Angular Damping", angularDamping, 0.1f))
								rigidBody.SetAngularDamping(angularDamping);
							if (UI::Property("Enable Gravity", bEnableGravity))
								rigidBody.SetEnableGravity(bEnableGravity);
							if (UI::Property("Is Kinematic", bKinematic, "Sometimes controlling an actor using forces or constraints is not sufficiently robust, precise or flexible."
								" For example moving platforms or character controllers often need to manipulate an actor's position or have"
								" it exactly follow a specific path. Such a control scheme is provided by kinematic actors."))
							{
								rigidBody.SetIsKinematic(bKinematic);
							}
							if (UI::Property("Lock Position", lockStrings, bLockPositions))
								rigidBody.SetLockPosition(bLockPositions[0], bLockPositions[1], bLockPositions[2]);
							if (UI::Property("Lock Rotation", lockStrings, bLockRotations))
								rigidBody.SetLockRotation(bLockRotations[0], bLockRotations[1], bLockRotations[2]);
						}
						UI::EndPropertyGrid();
					}, bCanRemove);
				break;
			}

			case SelectedComponent::BoxCollider:
			{
				DrawComponentTransformNode(entity, entity.GetComponent<BoxColliderComponent>());
				DrawComponent<BoxColliderComponent>("Box Collider", entity, [&entity, this, bRuntime](auto& collider)
					{
						UI::BeginPropertyGrid("BoxColliderComponent");

						Ref<PhysicsMaterial> material = collider.GetPhysicsMaterial();
						glm::vec3 size = collider.GetSize();
						bool bTrigger = collider.IsTrigger();
						bool bPhysicsMaterialChanged = false;
						bool bShowCollision = collider.IsCollisionVisible();

						bPhysicsMaterialChanged |= UI::PropertyDrag("Static Friction", material->StaticFriction, 0.1f, 0.f, 0.f, s_StaticFrictionHelpMsg);
						bPhysicsMaterialChanged |= UI::PropertyDrag("Dynamic Friction", material->DynamicFriction, 0.1f, 0.f, 0.f, s_DynamicFrictionHelpMsg);
						bPhysicsMaterialChanged |= UI::PropertyDrag("Bounciness", material->Bounciness, 0.1f);

						if (bPhysicsMaterialChanged)
							collider.SetPhysicsMaterial(material);

						if (UI::Property("Is Trigger", bTrigger, s_TriggerHelpMsg))
							collider.SetIsTrigger(bTrigger);
						if (UI::PropertyDrag("Size", size, 0.05f))
							collider.SetSize(size);
						if (UI::Property("Is Collision Visible", bShowCollision))
							collider.SetShowCollision(bShowCollision);

						UI::EndPropertyGrid();
					});
				break;
			}

			case SelectedComponent::SphereCollider:
			{
				DrawComponentTransformNode(entity, entity.GetComponent<SphereColliderComponent>());
				DrawComponent<SphereColliderComponent>("Sphere Collider", entity, [&entity, this, bRuntime](auto& collider)
					{
						UI::BeginPropertyGrid("SphereColliderComponent");

						Ref<PhysicsMaterial> material = collider.GetPhysicsMaterial();
						float radius = collider.GetRadius();
						bool bTrigger = collider.IsTrigger();
						bool bPhysicsMaterialChanged = false;
						bool bShowCollision = collider.IsCollisionVisible();

						bPhysicsMaterialChanged |= UI::PropertyDrag("Static Friction", material->StaticFriction, 0.1f, 0.f, 0.f, s_StaticFrictionHelpMsg);
						bPhysicsMaterialChanged |= UI::PropertyDrag("Dynamic Friction", material->DynamicFriction, 0.1f, 0.f, 0.f, s_DynamicFrictionHelpMsg);
						bPhysicsMaterialChanged |= UI::PropertyDrag("Bounciness", material->Bounciness, 0.1f);
						
						if (UI::Property("Is Trigger", bTrigger, s_TriggerHelpMsg))
							collider.SetIsTrigger(bTrigger);
						if (UI::PropertyDrag("Radius", radius, 0.5f))
							collider.SetRadius(radius);
						if (UI::Property("Is Collision Visible", bShowCollision))
							collider.SetShowCollision(bShowCollision);
						
						if (bPhysicsMaterialChanged)
							collider.SetPhysicsMaterial(material);

						UI::EndPropertyGrid();
					});
				break;
			}

			case SelectedComponent::CapsuleCollider:
			{
				DrawComponentTransformNode(entity, entity.GetComponent<CapsuleColliderComponent>());
				DrawComponent<CapsuleColliderComponent>("Capsule Collider", entity, [&entity, this, bRuntime](auto& collider)
					{
						UI::BeginPropertyGrid("CapsuleColliderComponent");

						Ref<PhysicsMaterial> material = collider.GetPhysicsMaterial();
						float height = collider.GetHeight();
						float radius = collider.GetRadius();
						bool bTrigger = collider.IsTrigger();
						bool bPhysicsMaterialChanged = false;
						bool bShowCollision = collider.IsCollisionVisible();

						bPhysicsMaterialChanged |= UI::PropertyDrag("Static Friction", material->StaticFriction, 0.1f, 0.f, 0.f, s_StaticFrictionHelpMsg);
						bPhysicsMaterialChanged |= UI::PropertyDrag("Dynamic Friction", material->DynamicFriction, 0.1f, 0.f, 0.f, s_DynamicFrictionHelpMsg);
						bPhysicsMaterialChanged |= UI::PropertyDrag("Bounciness", material->Bounciness, 0.1f);

						if (UI::Property("Is Trigger", bTrigger, s_TriggerHelpMsg))
							collider.SetIsTrigger(bTrigger);

						if (UI::PropertyDrag("Radius", radius, 0.05f))
							collider.SetRadius(radius);
						if (UI::PropertyDrag("Height", height, 0.05f))
							collider.SetHeight(height);
						if (UI::Property("Is Collision Visible", bShowCollision))
							collider.SetShowCollision(bShowCollision);

						if (bPhysicsMaterialChanged)
							collider.SetPhysicsMaterial(material);

						UI::EndPropertyGrid();
					});
				break;
			}

			case SelectedComponent::MeshCollider:
			{
				DrawComponentTransformNode(entity, entity.GetComponent<MeshColliderComponent>());
				DrawComponent<MeshColliderComponent>("Mesh Collider", entity, [&entity, this, bRuntime](auto& collider)
					{
						UI::BeginPropertyGrid("MeshColliderComponent");

						Ref<PhysicsMaterial> material = collider.GetPhysicsMaterial();
						Ref<StaticMesh> collisionMesh = collider.GetCollisionMesh();
						bool bTrigger = collider.IsTrigger();
						bool bPhysicsMaterialChanged = false;
						bool bShowCollision = collider.IsCollisionVisible();
						bool bConvex = collider.IsConvex();

						if (UI::DrawStaticMeshSelection("Collision Mesh", collisionMesh, "Must be set. Set the mesh that will be used to generate collision data for it"))
							collider.SetCollisionMesh(collisionMesh);
						bPhysicsMaterialChanged |= UI::PropertyDrag("Static Friction", material->StaticFriction, 0.1f, 0.f, 0.f, s_StaticFrictionHelpMsg);
						bPhysicsMaterialChanged |= UI::PropertyDrag("Dynamic Friction", material->DynamicFriction, 0.1f, 0.f, 0.f, s_DynamicFrictionHelpMsg);
						bPhysicsMaterialChanged |= UI::PropertyDrag("Bounciness", material->Bounciness, 0.1f);
						if (UI::Property("Is Trigger", bTrigger, s_TriggerHelpMsg))
							collider.SetIsTrigger(bTrigger);
						if (UI::Property("Is Collision Visible", bShowCollision))
							collider.SetShowCollision(bShowCollision);
						if (UI::Property("Is Convex", bConvex, "Generates collision around the mesh.\nNon-convex mesh collider can be used only\nwith kinematic or static actors."))
							collider.SetIsConvex(bConvex);

						if (bPhysicsMaterialChanged)
							collider.SetPhysicsMaterial(material);

						UI::EndPropertyGrid();
					});
				break;
			}
		
			case SelectedComponent::AudioComponent:
			{
				DrawComponentTransformNode(entity, entity.GetComponent<AudioComponent>());
				DrawComponent<AudioComponent>("Audio", entity, [&entity, this, bRuntime](auto& audio)
					{
						static const std::vector<std::string> rollOffModels = { "Linear", "Inverse", "Linear Square", "InverseTapered" };
						static const std::vector<std::string> rollOffToolTips = 
						{ 
							"This sound will follow a linear rolloff model where MinDistance = full volume, MaxDistance = silence",
							"This sound will follow the inverse rolloff model where MinDistance = full volume, MaxDistance = where sound stops attenuating, and rolloff is fixed according to the global rolloff factor",
							"This sound will follow a linear-square rolloff model where mindistance = full volume, maxdistance = silence",
							"This sound will follow the inverse rolloff model at distances close to MinDistance and a linear-square rolloff close to MaxDistance" 
						};

						int inSelectedRollOffModel = 0;
						UI::BeginPropertyGrid("AudioComponent");

						Path soundPath;
						
						const Ref<Sound>& sound = audio.GetSound();
						float volume = audio.GetVolume();
						int loopCount = audio.GetLoopCount();
						bool bLooping = audio.IsLooping();
						bool bMuted = audio.IsMuted();
						bool bStreaming = audio.IsStreaming();
						float minDistance = audio.GetMinDistance();
						float maxDistance = audio.GetMaxDistance();
						uint32_t currentRollOff = (uint32_t)audio.GetRollOffModel();

						if (sound)
							soundPath = sound->GetSoundPath();

						if (UI::DrawSoundSelection("Sound", soundPath))
							audio.SetSound(soundPath);

						if (UI::Combo("Roll off", currentRollOff, rollOffModels, inSelectedRollOffModel, rollOffToolTips))
							audio.SetRollOffModel(RollOffModel(inSelectedRollOffModel));

						if (UI::PropertySlider("Volume", volume, 0.f, 1.f))
							audio.SetVolume(volume);

						if (UI::PropertyDrag("Loop Count", loopCount, 1.f, -1, 10, "-1 = Loop Endlessly; 0 = Play once; 1 = Play twice, etc..."))
							audio.SetLoopCount(loopCount);

						if (UI::PropertyDrag("Min Distance", minDistance, 1.f, 0.f, maxDistance, "The minimum distance is the point at which the sound starts attenuating."
							" If the listener is any closer to the source than the minimum distance, the sound will play at full volume."))
						{
							audio.SetMinDistance(minDistance);
						}

						if (UI::PropertyDrag("Max Distance", maxDistance, 1.f, 0.f, 100000.f, "The maximum distance is the point at which the sound stops"
							" attenuatingand its volume remains constant (a volume which is not necessarily zero)"))
						{
							audio.SetMaxDistance(maxDistance);
						}

						if (UI::Property("Is Looping?", bLooping))
							audio.SetLooping(bLooping);
						
						if (UI::Property("Is Streaming?", bStreaming, "When you stream a sound, you can only have one instance of it playing at any time."
							" This limitation exists because there is only one decode buffer per stream."
							" As a rule of thumb, streaming is great for music tracks, voice cues, and ambient tracks,"
							" while most sound effects should be loaded into memory"))
						{
							audio.SetStreaming(bStreaming);
						}

						if (UI::Property("Is Muted?", bMuted))
							audio.SetMuted(bMuted);

						UI::Property("Autoplay", audio.bAutoplay);
						UI::Property("Enable Doppler Effect", audio.bEnableDopplerEffect);

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
						float minDistance = reverb.Reverb->GetMinDistance();
						float maxDistance = reverb.Reverb->GetMaxDistance();
						bool bActive = reverb.Reverb->IsActive();
						UI::BeginPropertyGrid("ReverbComponent");

						if (UI::Combo("Preset", (uint32_t)reverb.Reverb->GetPreset(), presets, inSelectedPreset))
							reverb.Reverb->SetPreset(ReverbPreset(inSelectedPreset));
						if (UI::PropertyDrag("Min Distance", minDistance, 1.f, 0.f, maxDistance, "Reverb is at full volume within that radius"))
							reverb.Reverb->SetMinDistance(minDistance);
						if (UI::PropertyDrag("Max Distance", maxDistance, 1.f, 0.f, 100000.f, "Reverb is disabled outside that radius"))
							reverb.Reverb->SetMaxDistance(maxDistance);
						if (UI::Property("Is Active", bActive))
							reverb.Reverb->SetActive(bActive);

						UI::EndPropertyGrid();
					});
				break;
			}
		}
	}

	void SceneHierarchyPanel::DrawComponentTransformNode(Entity& entity, SceneComponent& sceneComponent)
	{
		Transform relativeTranform = sceneComponent.GetRelativeTransform();
		glm::vec3 rotationInDegrees = glm::degrees(relativeTranform.Rotation.EulerAngles());
		glm::vec3 rotationInDegreesOld = rotationInDegrees;
		bool bValueChanged = false;
		bool bRotationChanged = false;

		DrawComponent<TransformComponent>("Transform", entity, [&relativeTranform, &rotationInDegrees, &bValueChanged, &bRotationChanged](auto& transformComponent)
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
		}
	}

	void SceneHierarchyPanel::DrawEntityTransformNode(Entity& entity)
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

		DrawComponent<TransformComponent>("Transform", entity, [&transform, &rotationInDegrees, &bValueChanged, &bRotationChanged](auto& transformComponent)
			{
				bValueChanged |= UI::DrawVec3Control("Location", transform.Location, glm::vec3{ 0.f });
				bRotationChanged = UI::DrawVec3Control("Rotation", rotationInDegrees, glm::vec3{ 0.f });
				bValueChanged |= UI::DrawVec3Control("Scale", transform.Scale3D, glm::vec3{ 1.f });
				bValueChanged |= bRotationChanged;
			}, false);

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
		}
	}

	void SceneHierarchyPanel::OnEvent(Event& e)
	{
		if (e.GetEventType() == EventType::KeyPressed)
		{
			KeyPressedEvent& keyEvent = (KeyPressedEvent&)e;
			bool bLeftControlPressed = Input::IsKeyPressed(Key::LeftControl);

			if (!m_PropertiesHovered && (m_Editor.IsViewportFocused() || m_SceneHierarchyFocused) && m_SelectedEntity)
			{
				if (keyEvent.GetKey() == Key::Delete && !m_Scene->IsPlaying())
				{
					m_Scene->DestroyEntity(m_SelectedEntity);
					ClearSelection();
				}
				else if (bLeftControlPressed && keyEvent.GetKey() == Key::W)
				{
					m_SelectedEntity = m_Scene->CreateFromEntity(m_SelectedEntity);
				}
			}
		}
	}
}
