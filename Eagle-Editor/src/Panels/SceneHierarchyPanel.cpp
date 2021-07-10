#include "SceneHierarchyPanel.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <glm/gtc/type_ptr.hpp>
#include <Eagle/UI/UI.h>
#include <filesystem>

namespace Eagle
{
	static bool DrawVec3Control(const std::string& label, glm::vec3& values, const glm::vec3 resetValues = glm::vec3{0.f}, float columnWidth = 100.f)
	{
		bool bValueChanged = false;
		ImGuiIO& io = ImGui::GetIO();
		auto boldFont = io.Fonts->Fonts[0];

		ImGui::PushID(label.c_str());

		ImGui::Columns(2, nullptr, false);
		ImGui::SetColumnWidth(0, columnWidth);
		ImGui::Text(label.c_str());
		ImGui::NextColumn();

		ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{0.f, 0.f});

		float lineHeight = (GImGui->Font->FontSize * boldFont->Scale) + GImGui->Style.FramePadding.y * 2.f;
		ImVec2 buttonSize = {lineHeight + 3.f, lineHeight};

		//X
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.8f, 0.1f, 0.15f, 1.f});
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{0.9f, 0.2f, 0.2f, 1.f});
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{0.8f, 0.1f, 0.15f, 1.f});
		ImGui::PushFont(boldFont);
		if (ImGui::Button("X", buttonSize))
		{
			values.x = resetValues.x;
			bValueChanged = true;
		}
		ImGui::PopFont();
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		bValueChanged |= ImGui::DragFloat("##X", &values.x, 0.1f);
		ImGui::PopItemWidth();
		ImGui::SameLine();

		//Y
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.2f, 0.7f, 0.2f, 1.f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.3f, 0.8f, 0.3f, 1.f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.2f, 0.7f, 0.2f, 1.f });
		ImGui::PushFont(boldFont);
		if (ImGui::Button("Y", buttonSize))
		{
			values.y = resetValues.y;
			bValueChanged = true;
		}
		ImGui::PopFont();
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		bValueChanged |= ImGui::DragFloat("##Y", &values.y, 0.1f);
		ImGui::PopItemWidth();
		ImGui::SameLine();

		//Z
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.1f, 0.25f, 0.8f, 1.f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.2f, 0.35f, 0.9f, 1.f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.1f, 0.25f, 0.8f, 1.f });
		ImGui::PushFont(boldFont);
		if (ImGui::Button("Z", buttonSize))
		{
			values.z = resetValues.z;
			bValueChanged = true;
		}
		ImGui::PopFont();
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		bValueChanged |= ImGui::DragFloat("##Z", &values.z, 0.1f);
		ImGui::PopItemWidth();

		ImGui::PopStyleVar();
		
		ImGui::Columns(1);

		ImGui::PopID();

		return bValueChanged;
	}

	SceneHierarchyPanel::SceneHierarchyPanel(const Ref<Scene>& scene)
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
				droppedEntity.SetOwner(Entity::Null);
			}

			ImGui::EndDragDropTarget();
		}

		m_Scene->m_Registry.each([&](entt::entity entityID)
			{
				Entity entity = Entity(entityID, m_Scene.get());
				DrawEntityNode(entity);
			});

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
		if (entity.HasOwner()) //For drawing children use DrawChilds
			return;

		const auto& entityName = entity.GetComponent<EntitySceneNameComponent>().Name;
		uint32_t entityID = entity.GetID();

		//If selected child of this entity, open tree node
		if (m_SelectedEntity && m_SelectedEntity != entity)
		{
			if (entity.IsOwnerOf(m_SelectedEntity))
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
				if (droppedEntity.GetOwner() != entity)
					droppedEntity.SetOwner(entity);
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
		auto& ownershipComponent = entity.GetComponent<OwnershipComponent>();

		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;

		for (auto& child : ownershipComponent.Children)
		{
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
					
					if (droppedEntity.GetOwner() != child)
						droppedEntity.SetOwner(child);
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
		if (entity.HasComponent<EntitySceneNameComponent>())
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
		}
		
		ImGui::SameLine();
		ImGui::PushItemWidth(-1);

		if (ImGui::Button("Add"))
			ImGui::OpenPopup("AddComponent");

		if (ImGui::BeginPopup("AddComponent"))
		{
			DrawAddComponentMenuItem<CameraComponent>("Camera");
			DrawAddComponentMenuItem<SpriteComponent>("Sprite");
			DrawAddComponentMenuItem<StaticMeshComponent>("Static Mesh");
			DrawAddComponentMenuItem<PointLightComponent>("Point Light");
			DrawAddComponentMenuItem<DirectionalLightComponent>("Directional Light");
			DrawAddComponentMenuItem<SpotLightComponent>("Spot Light");
			
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
						auto& material = sprite.Material;
						bool bChecked = false;
						bool bChanged = false;

						bChecked = ImGui::Checkbox("Is SubTexture?", &sprite.bSubTexture);
						if (bChecked && sprite.bSubTexture && material->DiffuseTexture)
							sprite.SubTexture = SubTexture2D::CreateFromCoords(Cast<Texture2D>(material->DiffuseTexture), sprite.SubTextureCoords, sprite.SpriteSize, sprite.SpriteSizeCoef);
						
						bChanged |= UI::DrawTextureSelection(material->DiffuseTexture, sprite.bSubTexture ? "Atlas" : "Diffuse", true);

						if (sprite.bSubTexture && material->DiffuseTexture)
						{
							int subTC[2] = { (int)sprite.SubTextureCoords.x, (int)sprite.SubTextureCoords.y };
							int spriteSize[2] = { (int)sprite.SpriteSize.x, (int)sprite.SpriteSize.y };
							int spriteSizeCoef[2] = { (int)sprite.SpriteSizeCoef.x, (int)sprite.SpriteSizeCoef.y };
							if (ImGui::DragInt2("SubTexture Coords", subTC))
							{
								sprite.SubTextureCoords.x = (float)subTC[0];
								sprite.SubTextureCoords.y = (float)subTC[1];
								bChanged = true;
							}
							if (ImGui::DragInt2("Sprite Size", spriteSize))
							{
								sprite.SpriteSize.x = (float)spriteSize[0];
								sprite.SpriteSize.y = (float)spriteSize[1];
								bChanged = true;
							}
							if (ImGui::DragInt2("Sprite Size Coef", spriteSizeCoef))
							{
								sprite.SpriteSizeCoef.x = (float)spriteSizeCoef[0];
								sprite.SpriteSizeCoef.y = (float)spriteSizeCoef[1];
								bChanged = true;
							}
							glm::vec2 atlasSize = material->DiffuseTexture->GetSize();
							ImGui::Text("Atlas size: %dx%d", (int)atlasSize.x, (int)atlasSize.y);
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
							UI::DrawTextureSelection(material->SpecularTexture, "Specular", false);
						}
						if (!sprite.bSubTexture)
						{
							UI::DrawTextureSelection(material->NormalTexture, "Normal", false);
						}
						ImGui::SliderFloat("Tiling Factor", &material->TilingFactor, 1.f, 128.f);
						ImGui::SliderFloat("Shininess", &material->Shininess, 1.f, 128.f);
					});
				break;
			}

			case SelectedComponent::StaticMesh:
			{
				DrawComponentTransformNode(entity, entity.GetComponent<StaticMeshComponent>());
				DrawComponent<StaticMeshComponent>("Static Mesh", entity, [&entity, this](auto& smComponent)
					{
						auto& staticMesh = smComponent.StaticMesh;

						ImGui::Text("Static Mesh:");
						ImGui::SameLine();
						UI::DrawStaticMeshSelection(smComponent, staticMesh->GetName());

						auto& material = staticMesh->Material;

						UI::DrawTextureSelection(material->DiffuseTexture, "Diffuse", true);
						UI::DrawTextureSelection(material->SpecularTexture, "Specular", false);
						UI::DrawTextureSelection(material->NormalTexture, "Normal", false);

						ImGui::SliderFloat("Tiling Factor", &material->TilingFactor, 1.f, 128.f);
						ImGui::SliderFloat("Shininess", &material->Shininess, 1.f, 128.f);
					});
				break;
			}

			case SelectedComponent::Camera:
			{
				DrawComponentTransformNode(entity, entity.GetComponent<CameraComponent>());
				DrawComponent<CameraComponent>("Camera", entity, [&entity, this](auto& cameraComponent)
				{
					auto& camera = cameraComponent.Camera;

					ImGui::Checkbox("Primary", &cameraComponent.Primary);

					const char* projectionModesStrings[] = { "Perspective", "Orthographic" };
					const char* currentProjectionModeString = projectionModesStrings[(int)camera.GetProjectionMode()];

					if (ImGui::BeginCombo("Projection", currentProjectionModeString))
					{
						for (int i = 0; i < 2; ++i)
						{
							bool isSelected = (currentProjectionModeString == projectionModesStrings[i]);

							if (ImGui::Selectable(projectionModesStrings[i], isSelected))
							{
								currentProjectionModeString = projectionModesStrings[i];
								camera.SetProjectionMode((CameraProjectionMode)i);
							}

							if (isSelected)
							{
								ImGui::SetItemDefaultFocus();
							}
						}
						ImGui::EndCombo();
					}

					if (camera.GetProjectionMode() == CameraProjectionMode::Perspective)
					{
						float verticalFov = glm::degrees(camera.GetPerspectiveVerticalFOV());
						if (ImGui::DragFloat("Vertical FOV", &verticalFov))
						{
							camera.SetPerspectiveVerticalFOV(glm::radians(verticalFov));
						}

						float perspectiveNear = camera.GetPerspectiveNearClip();
						if (ImGui::DragFloat("Near Clip", &perspectiveNear))
						{
							camera.SetPerspectiveNearClip(perspectiveNear);
						}

						float perspectiveFar = camera.GetPerspectiveFarClip();
						if (ImGui::DragFloat("Far Clip", &perspectiveFar))
						{
							camera.SetPerspectiveFarClip(perspectiveFar);
						}
					}
					else
					{
						float size = camera.GetOrthographicSize();
						if (ImGui::DragFloat("Size", &size))
						{
							camera.SetOrthographicSize(size);
						}

						float orthoNear = camera.GetOrthographicNearClip();
						if (ImGui::DragFloat("Near Clip", &orthoNear))
						{
							camera.SetOrthographicNearClip(orthoNear);
						}

						float orthoFar = camera.GetOrthographicFarClip();
						if (ImGui::DragFloat("Far Clip", &orthoFar))
						{
							camera.SetOrthographicFarClip(orthoFar);
						}

						ImGui::Checkbox("Fixed Aspect Ratio", &cameraComponent.FixedAspectRatio);
					}
				});
				break;
			}

			case SelectedComponent::PointLight:
			{
				DrawComponentTransformNode(entity, entity.GetComponent<PointLightComponent>());
				DrawComponent<PointLightComponent>("Point Light", entity, [&entity, this](auto& pointLight)
					{
						auto& color = pointLight.LightColor;
						
						ImGui::ColorEdit4("Light Color", glm::value_ptr(color));
						ImGui::SliderFloat3("Ambient", glm::value_ptr(pointLight.Ambient), 0.0f, 1.f);
						ImGui::SliderFloat3("Specular", glm::value_ptr(pointLight.Specular), 0.00001f, 1.f);
						ImGui::DragFloat("Distance", &pointLight.Distance, 0.1f, 0.f);
					});
				break;
			}

			case SelectedComponent::DirectionalLight:
			{
				DrawComponentTransformNode(entity, entity.GetComponent<DirectionalLightComponent>());
				DrawComponent<DirectionalLightComponent>("Directional Light", entity, [&entity, this](auto& directionalLight)
					{
						auto& color = directionalLight.LightColor;

						ImGui::ColorEdit4("Light Color", glm::value_ptr(color));
						ImGui::SliderFloat3("Ambient", glm::value_ptr(directionalLight.Ambient), 0.0f, 1.f);
						ImGui::SliderFloat3("Specular", glm::value_ptr(directionalLight.Specular), 0.0f, 1.f);
					});
				break;
			}

			case SelectedComponent::SpotLight:
			{
				DrawComponentTransformNode(entity, entity.GetComponent<SpotLightComponent>());
				DrawComponent<SpotLightComponent>("Spot Light", entity, [&entity, this](auto& spotLight)
					{
						auto& color = spotLight.LightColor;

						ImGui::ColorEdit4("Light Color", glm::value_ptr(color));
						ImGui::SliderFloat3("Ambient", glm::value_ptr(spotLight.Ambient), 0.0f, 1.f);
						ImGui::SliderFloat3("Specular", glm::value_ptr(spotLight.Specular), 0.0f, 1.f);
						ImGui::SliderFloat("Inner Angle", &spotLight.InnerCutOffAngle, 0.f, 180.f);
						ImGui::SliderFloat("Outer Angle", &spotLight.OuterCutOffAngle, spotLight.InnerCutOffAngle, 180.001f);

						spotLight.OuterCutOffAngle = std::max(spotLight.OuterCutOffAngle, spotLight.InnerCutOffAngle);
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
			bValueChanged |= DrawVec3Control("Translation", relativeTranform.Translation, glm::vec3{0.f});
			bValueChanged |= DrawVec3Control("Rotation", rotationInDegrees, glm::vec3{0.f});
			bValueChanged |= DrawVec3Control("Scale", relativeTranform.Scale3D, glm::vec3{1.f});
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

		if (Entity owner = entity.GetOwner())
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
				bValueChanged |= DrawVec3Control("Translation", transform.Translation, glm::vec3{ 0.f });
				bValueChanged |= DrawVec3Control("Rotation", rotationInDegrees, glm::vec3{ 0.f });
				bValueChanged |= DrawVec3Control("Scale", transform.Scale3D, glm::vec3{ 1.f });
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
