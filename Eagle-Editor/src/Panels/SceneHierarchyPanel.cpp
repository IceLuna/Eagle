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
	SceneHierarchyPanel::SceneHierarchyPanel(const EditorLayer& editor) : m_Editor(editor)
	{}

	SceneHierarchyPanel::SceneHierarchyPanel(const EditorLayer& editor, const Ref<Scene>& scene) : m_Editor(editor)
	{
		SetContext(scene);
	}

	void SceneHierarchyPanel::SetContext(const Ref<Scene>& scene)
	{
		ClearSelection();
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

		for (auto& it : m_Scene->m_AliveEntities)
		{
			DrawEntityNode(it.second);
		}

		if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered())
		{
			ClearSelection();
		}

		//Right-click on empty space in Scene Hierarchy
		if (ImGui::BeginPopupContextWindow(0, 1, false))
		{
			if (ImGui::MenuItem("Create Entity"))
				m_Scene->CreateEntity("Empty Entity");

			ImGui::EndPopup();
		}

		ImGui::End(); //Scene Hierarchy
	}

	void SceneHierarchyPanel::DrawEntityNode(Entity& entity)
	{
		if (entity.HasParent()) //For drawing children use DrawChilds
			return;

		const auto& entityName = entity.GetComponent<EntitySceneNameComponent>().Name;
		uint32_t entityID = entity.GetID();

		//If selected child of this entity, open tree node
		if (m_SelectedEntity && m_SelectedEntity != entity)
		{
			if (entity.IsParentOf(m_SelectedEntity))
			{
				ImGui::SetNextItemOpen(true);
			}
		}

		ImGuiTreeNodeFlags flags = (m_SelectedEntity == entity ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_OpenOnArrow
			| ImGuiTreeNodeFlags_SpanAvailWidth | (entity.HasChildren() ? 0 : ImGuiTreeNodeFlags_Leaf);
		bool opened = ImGui::TreeNodeEx((void*)(uint64_t)entity.GetID(), flags, entityName.c_str());

		if (ImGui::IsItemClicked())
		{
			ClearSelection();
			m_SelectedEntity = entity;
		}

		const std::string popupID = std::to_string(entityID);
		if (ImGui::BeginPopupContextItem(popupID.c_str()))
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

			const auto& childName = child.GetComponent<EntitySceneNameComponent>().Name;
			bool openedChild = ImGui::TreeNodeEx((void*)(uint64_t)child.GetID(), childTreeFlags, childName.c_str());
			
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
		std::strncpy(buffer, entityName.c_str(), sizeof(buffer));
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
			DrawAddComponentMenuItem<ScriptComponent>("Script");
			DrawAddComponentMenuItem<RigidBodyComponent>("Rigid Body");
			DrawAddComponentMenuItem<BoxColliderComponent>("Box Collider");
			DrawAddComponentMenuItem<SphereColliderComponent>("Sphere Collider");
			DrawAddComponentMenuItem<CapsuleColliderComponent>("Capsule Collider");
			DrawAddComponentMenuItem<MeshColliderComponent>("Mesh Collider");
			DrawAddComponentMenuItem<CameraComponent>("Camera");
			DrawAddComponentMenuItem<SpriteComponent>("Sprite");
			DrawAddComponentMenuItem<StaticMeshComponent>("Static Mesh");
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
				if (DrawComponentLine<ScriptComponent>("Script", entity, m_SelectedComponent == SelectedComponent::Script))
				{
					m_SelectedComponent = SelectedComponent::Script;
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
				DrawComponent<SpriteComponent>("Sprite", entity, [&entity, this](auto& sprite)
					{
						UI::BeginPropertyGrid("SpriteComponent");

						auto& material = sprite.Material;
						bool bChecked = false;
						bool bChanged = false;

						bChecked = UI::Property("Is SubTexture?", sprite.bSubTexture);
						if (bChecked && sprite.bSubTexture && material->DiffuseTexture)
							sprite.SubTexture = SubTexture2D::CreateFromCoords(Cast<Texture2D>(material->DiffuseTexture), sprite.SubTextureCoords, sprite.SpriteSize, sprite.SpriteSizeCoef);
						
						bChanged |= UI::DrawTextureSelection(sprite.bSubTexture ? "Atlas" : "Diffuse", material->DiffuseTexture, true);

						if (sprite.bSubTexture && material->DiffuseTexture)
						{
							bChanged |= UI::PropertyDrag("SubTexture Coords", sprite.SubTextureCoords);
							bChanged |= UI::PropertyDrag("Sprite Size", sprite.SpriteSize);
							bChanged |= UI::PropertyDrag("Sprite Size Coef", sprite.SpriteSizeCoef);

							glm::vec2 atlasSize = material->DiffuseTexture->GetSize();
							std::string atlasSizeString = std::to_string((int)atlasSize.x) + "x" + std::to_string((int)atlasSize.y);
							UI::PropertyText("Atlas size", atlasSizeString);
						}
						if (bChanged)
						{
							if (sprite.bSubTexture && material->DiffuseTexture)
								sprite.SubTexture = SubTexture2D::CreateFromCoords(Cast<Texture2D>(material->DiffuseTexture), sprite.SubTextureCoords, sprite.SpriteSize, sprite.SpriteSizeCoef);
							else
								sprite.SubTexture.reset();
						}

						if (!sprite.bSubTexture)
						{
							UI::DrawTextureSelection("Specular", material->SpecularTexture, false);
						}
						if (!sprite.bSubTexture)
						{
							UI::DrawTextureSelection("Normal", material->NormalTexture, false);
						}
						UI::PropertyColor("Tint Color", material->TintColor);
						UI::PropertySlider("Tiling Factor", material->TilingFactor, 1.f, 10.f);
						UI::PropertySlider("Shininess", material->Shininess, 1.f, 128.f);

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
						auto& staticMesh = smComponent.StaticMesh;

						UI::DrawStaticMeshSelection("Static Mesh", smComponent);

						auto& material = staticMesh->Material;

						UI::DrawTextureSelection("Diffuse", material->DiffuseTexture, true);
						UI::DrawTextureSelection("Specular", material->SpecularTexture, false);
						UI::DrawTextureSelection("Normal", material->NormalTexture, false);

						UI::PropertyColor("Tint Color", material->TintColor);
						UI::PropertySlider("Tiling Factor", material->TilingFactor, 1.f, 128.f);
						UI::PropertySlider("Shininess", material->Shininess, 1.f, 128.f);
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
					const std::string& currentProjectionModeString = projectionModesStrings[(int)camera.GetProjectionMode()];

					int selectedIndex = 0;
					if (UI::Combo("Projection", currentProjectionModeString, projectionModesStrings, selectedIndex))
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
						auto& color = pointLight.LightColor;
						
						UI::BeginPropertyGrid("PointLightComponent");
						UI::PropertyColor("Light Color", color);
						UI::PropertySlider("Ambient", pointLight.Ambient, 0.0f, 1.f);
						UI::PropertySlider("Specular", pointLight.Specular, 0.00001f, 1.f);
						UI::PropertyDrag("Distance", pointLight.Distance, 0.1f, 0.f);
						UI::EndPropertyGrid();
					});
				break;
			}

			case SelectedComponent::DirectionalLight:
			{
				DrawComponentTransformNode(entity, entity.GetComponent<DirectionalLightComponent>());
				DrawComponent<DirectionalLightComponent>("Directional Light", entity, [&entity, this](auto& directionalLight)
					{
						auto& color = directionalLight.LightColor;

						UI::BeginPropertyGrid("DirectionalLightComponent");
						UI::PropertyColor("Light Color", color);
						UI::PropertySlider("Ambient", directionalLight.Ambient, 0.0f, 1.f);
						UI::PropertySlider("Specular", directionalLight.Specular, 0.0f, 1.f);
						UI::EndPropertyGrid();
					});
				break;
			}

			case SelectedComponent::SpotLight:
			{
				DrawComponentTransformNode(entity, entity.GetComponent<SpotLightComponent>());
				DrawComponent<SpotLightComponent>("Spot Light", entity, [&entity, this](auto& spotLight)
					{
						auto& color = spotLight.LightColor;

						UI::BeginPropertyGrid("SpotLightComponent");
						UI::PropertyColor("Light Color", color);
						UI::PropertySlider("Ambient", spotLight.Ambient, 0.0f, 1.f);
						UI::PropertySlider("Specular", spotLight.Specular, 0.0f, 1.f);
						UI::PropertySlider("Inner Angle", spotLight.InnerCutOffAngle, 0.f, 180.f);
						UI::PropertySlider("Outer Angle", spotLight.OuterCutOffAngle, spotLight.InnerCutOffAngle, 180.001f);
						UI::EndPropertyGrid();

						spotLight.OuterCutOffAngle = std::max(spotLight.OuterCutOffAngle, spotLight.InnerCutOffAngle);
					});
				break;
			}
		
			case SelectedComponent::Script:
			{
				DrawComponent<ScriptComponent>("Script", entity, [&entity, this](auto& scriptComponent)
					{
						UI::BeginPropertyGrid("ScriptComponent");
						const bool bRuntime = (m_Editor.GetEditorState() == EditorState::Play);
						EntityInstance& entityInstance = ScriptEngine::GetEntityInstanceData(entity).Instance;
						if (bRuntime)
							UI::PushItemDisabled();

						if (UI::Property("Script Class", scriptComponent.ModuleName, "Specify the namespace & class names. For example, 'Sandbox.Rolling'"))
							ScriptEngine::InitEntityScript(entity);

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
									if (UI::Property(field.Name.c_str(), value))
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
				DrawComponentTransformNode(entity, entity.GetComponent<RigidBodyComponent>());
				DrawComponent<RigidBodyComponent>("Rigid Body", entity, [&entity, this](auto& rigidBody)
					{
						UI::BeginPropertyGrid("RigidBodyComponent");
						UI::PropertyDrag("Mass", rigidBody.Mass);
						UI::Property("Disable Gravity", rigidBody.DisableGravity);
						UI::EndPropertyGrid();
					});
				break;
			}

			case SelectedComponent::BoxCollider:
			{
				DrawComponentTransformNode(entity, entity.GetComponent<BoxColliderComponent>());
				DrawComponent<BoxColliderComponent>("Box Collider", entity, [&entity, this](auto& collider)
					{
						UI::BeginPropertyGrid("BoxColliderComponent");
						UI::PropertyDrag("Size", collider.Size);
						UI::EndPropertyGrid();
					});
				break;
			}

			case SelectedComponent::SphereCollider:
			{
				DrawComponentTransformNode(entity, entity.GetComponent<SphereColliderComponent>());
				DrawComponent<SphereColliderComponent>("Sphere Collider", entity, [&entity, this](auto& collider)
					{
						UI::BeginPropertyGrid("SphereColliderComponent");
						UI::EndPropertyGrid();
					});
				break;
			}

			case SelectedComponent::CapsuleCollider:
			{
				DrawComponentTransformNode(entity, entity.GetComponent<CapsuleColliderComponent>());
				DrawComponent<CapsuleColliderComponent>("Capsule Collider", entity, [&entity, this](auto& collider)
					{
						UI::BeginPropertyGrid("CapsuleColliderComponent");
						UI::EndPropertyGrid();
					});
				break;
			}

			case SelectedComponent::MeshCollider:
			{
				DrawComponentTransformNode(entity, entity.GetComponent<MeshColliderComponent>());
				DrawComponent<MeshColliderComponent>("Mesh Collider", entity, [&entity, this](auto& collider)
					{
						UI::BeginPropertyGrid("MeshColliderComponent");
						UI::EndPropertyGrid();
					});
				break;
			}
		}
	}

	void SceneHierarchyPanel::DrawComponentTransformNode(Entity& entity, SceneComponent& sceneComponent)
	{
		Transform relativeTranform = sceneComponent.GetRelativeTransform();
		glm::vec3 rotationInDegrees = glm::degrees(relativeTranform.Rotation);
		bool bValueChanged = false;

		DrawComponent<TransformComponent>("Transform", entity, [&relativeTranform, &rotationInDegrees, &bValueChanged](auto& transformComponent)
		{
			bValueChanged |= UI::DrawVec3Control("Location", relativeTranform.Location, glm::vec3{0.f});
			bValueChanged |= UI::DrawVec3Control("Rotation", rotationInDegrees, glm::vec3{0.f});
			bValueChanged |= UI::DrawVec3Control("Scale", relativeTranform.Scale3D, glm::vec3{1.f});
		}, false);

		if (bValueChanged)
		{
			relativeTranform.Rotation = glm::radians(rotationInDegrees);
			sceneComponent.SetRelativeTransform(relativeTranform);
		}
	}

	void SceneHierarchyPanel::DrawEntityTransformNode(Entity& entity)
	{
		Transform transform;
		bool bValueChanged = false;
		bool bUsedRelativeTransform = false;

		if (Entity parent = entity.GetParent())
		{
			transform = entity.GetRelativeTransform();
			bUsedRelativeTransform = true;
		}
		else
		{
			transform = entity.GetWorldTransform();
		}

		glm::vec3 rotationInDegrees = glm::degrees(transform.Rotation);

		DrawComponent<TransformComponent>("Transform", entity, [&transform, &rotationInDegrees, &bValueChanged](auto& transformComponent)
			{
				bValueChanged |= UI::DrawVec3Control("Location", transform.Location, glm::vec3{ 0.f });
				bValueChanged |= UI::DrawVec3Control("Rotation", rotationInDegrees, glm::vec3{ 0.f });
				bValueChanged |= UI::DrawVec3Control("Scale", transform.Scale3D, glm::vec3{ 1.f });
			}, false);

		if (bValueChanged)
		{
			transform.Rotation = glm::radians(rotationInDegrees);
			
			if (bUsedRelativeTransform)
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
			
			if (keyEvent.GetKeyCode() == Key::Delete)
			{
				if (!m_PropertiesHovered && m_SelectedEntity)
				{
					m_Scene->DestroyEntity(m_SelectedEntity);
					ClearSelection();
				}
			}
		}
	}
}
