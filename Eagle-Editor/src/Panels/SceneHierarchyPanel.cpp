#include "SceneHierarchyPanel.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <glm/gtc/type_ptr.hpp>

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

		float lineHeight = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.f;
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
		m_Scene = scene;
		ClearSelection();
	}

	void SceneHierarchyPanel::ClearSelection()
	{
		m_SelectedEntity = Entity::Null;
	}

	void SceneHierarchyPanel::SetEntitySelected(int entityID)
	{
		if (entityID < 0)
		{
			m_SelectedEntity = Entity::Null;
		}
		else
		{
			entt::entity enttID = (entt::entity)entityID;
			m_SelectedEntity = Entity{enttID, m_Scene.get()};
		}
	}

	void SceneHierarchyPanel::OnImGuiRender()
	{
		ImGui::Begin("Scene Hierarchy");
		m_Scene->m_Registry.each([&](entt::entity entityID)
		{
			Entity entity = Entity(entityID, m_Scene.get());
			DrawEntityNode(entity);
		});

		if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered())
		{
			m_SelectedEntity = Entity::Null;
		}

		//Right-click on empty space in Scene Hierarchy
		if (ImGui::BeginPopupContextWindow(0, 1, false))
		{
			if (ImGui::MenuItem("Create Entity"))
				m_Scene->CreateEntity("Empty Entity");

			ImGui::EndPopup();
		}

		ImGui::End(); //Scene Hierarchy
		
		ImGui::Begin("Properties");
		if (m_SelectedEntity)
		{
			DrawComponents(m_SelectedEntity);
		}
		ImGui::End(); //Properties
	}

	void SceneHierarchyPanel::DrawEntityNode(Entity entity)
	{
		if (entity.HasOwner())
			return;

		const auto& entityName = entity.GetComponent<EntitySceneNameComponent>().Name;
		uint32_t entityID = entity.GetID();

		ImGuiTreeNodeFlags flags = (m_SelectedEntity == entity ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_OpenOnArrow
			| ImGuiTreeNodeFlags_SpanAvailWidth | (entity.HasChildren() ? 0 : ImGuiTreeNodeFlags_Leaf);
		bool opened = ImGui::TreeNodeEx((void*)(uint64_t)entity.GetID(), flags, entityName.c_str());

		if (ImGui::IsItemClicked())
		{
			m_SelectedEntity = entity;
		}

		std::string popupID = std::to_string(entityID);
		if (ImGui::BeginPopupContextItem(popupID.c_str()))
		{
			if (ImGui::MenuItem("Delete Entity"))
			{
				if (m_SelectedEntity == entity)
					m_SelectedEntity = Entity::Null;
				m_Scene->DestroyEntity(entity);
			}
			ImGui::EndPopup();
		}

		if (opened)
		{
			DrawChilds(entity);
			ImGui::TreePop();
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
				if (droppedEntity.HasOwner())
				{
					droppedEntity.GetOwner().RemoveChildren(droppedEntity);
				}
				droppedEntity.SetOwner(entity);
				
				entity.AddChildren(droppedEntity);
			}

			ImGui::EndDragDropTarget();
		}
	}

	void SceneHierarchyPanel::DrawChilds(Entity entity)
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
				m_SelectedEntity = child;
			}

			std::string popupID = std::to_string(child.GetID());
			if (ImGui::BeginPopupContextItem(popupID.c_str()))
			{
				if (ImGui::MenuItem("Delete Entity"))
				{
					if (m_SelectedEntity == child)
						m_SelectedEntity = Entity::Null;
					m_Scene->DestroyEntity(child);
				}

				ImGui::EndPopup();
			}

			if (openedChild)
			{
				DrawChilds(child);
				ImGui::TreePop();
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
					if (droppedEntity.HasOwner())
					{
						droppedEntity.GetOwner().RemoveChildren(droppedEntity);
					}
					droppedEntity.SetOwner(child);

					child.AddChildren(droppedEntity);
				}

				ImGui::EndDragDropTarget();
			}
		}
	}

	void SceneHierarchyPanel::DrawComponents(Entity entity)
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
			if (ImGui::MenuItem("Camera"))
			{
				if (m_SelectedEntity.HasComponent<CameraComponent>() == false)
				{
					m_SelectedEntity.AddComponent<CameraComponent>();
				}
				else
				{
					EG_CORE_WARN("This entity already has this component!");
				}
				ImGui::CloseCurrentPopup();
			}

			if (ImGui::MenuItem("Sprite"))
			{
				if (m_SelectedEntity.HasComponent<SpriteComponent>() == false)
				{
					m_SelectedEntity.AddComponent<SpriteComponent>();
				}
				else
				{
					EG_CORE_WARN("This entity already has this component!");
				}
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}

		ImGui::PopItemWidth();

		if (entity.HasComponent<TransformComponent>())
		{
			auto& transformComponent = entity.GetComponent<TransformComponent>();
			DrawEntityTransformNode(entity, transformComponent);
		}

		DrawComponent<SpriteComponent>("Sprite", entity, [&entity, this](auto& sprite)
		{
			auto& color = sprite.Color;

			DrawComponentTransformNode(entity, sprite);
			ImGui::ColorEdit4("Color", glm::value_ptr(color));
		});

		DrawComponent<CameraComponent>("Camera", entity, [&entity, this](auto& cameraComponent)
		{
				auto& camera = cameraComponent.Camera;

				DrawComponentTransformNode(entity, cameraComponent);

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
	}
	
	void SceneHierarchyPanel::DrawComponentTransformNode(Entity entity, SceneComponent& sceneComponent)
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

	void SceneHierarchyPanel::DrawEntityTransformNode(Entity entity, TransformComponent& transformComponent)
	{
		Transform* transform = nullptr;
		bool bValueChanged = false;

		if (Entity owner = entity.GetOwner())
		{
			transform = &transformComponent.RelativeTransform;
		}
		else
		{
			transform = &transformComponent.WorldTransform;
		}

		glm::vec3 rotationInDegrees = glm::degrees((*transform).Rotation);

		DrawComponent<TransformComponent>("Transform", entity, [transform, &rotationInDegrees, &bValueChanged](auto& transformComponent)
			{
				bValueChanged |= DrawVec3Control("Translation", (*transform).Translation, glm::vec3{ 0.f });
				bValueChanged |= DrawVec3Control("Rotation", rotationInDegrees, glm::vec3{ 0.f });
				bValueChanged |= DrawVec3Control("Scale", (*transform).Scale3D, glm::vec3{ 1.f });
			}, false);

		if (bValueChanged)
		{
			(*transform).Rotation = glm::radians(rotationInDegrees);
			
			if (entity.HasComponent<CameraComponent>())
			{
				entity.GetComponent<CameraComponent>().UpdateTransform();
			}
			if (entity.HasComponent<SpriteComponent>())
			{
				entity.GetComponent<SpriteComponent>().UpdateTransform();
			}
		}
	}
}
