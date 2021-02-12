#include "SceneHierarchyPanel.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <glm/gtc/type_ptr.hpp>

namespace Eagle
{
	static void DrawVec3Control(const std::string& label, glm::vec3& values, const glm::vec3 resetValues = glm::vec3{0.f}, float columnWidth = 100.f)
	{
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
			values.x = resetValues.x;
		ImGui::PopFont();
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		ImGui::DragFloat("##X", &values.x, 0.1f);
		ImGui::PopItemWidth();
		ImGui::SameLine();

		//Y
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.2f, 0.7f, 0.2f, 1.f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.3f, 0.8f, 0.3f, 1.f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.2f, 0.7f, 0.2f, 1.f });
		ImGui::PushFont(boldFont);
		if (ImGui::Button("Y", buttonSize))
			values.y = resetValues.y;
		ImGui::PopFont();
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		ImGui::DragFloat("##Y", &values.y, 0.1f);
		ImGui::PopItemWidth();
		ImGui::SameLine();

		//Z
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.1f, 0.25f, 0.8f, 1.f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.2f, 0.35f, 0.9f, 1.f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.1f, 0.25f, 0.8f, 1.f });
		ImGui::PushFont(boldFont);
		if (ImGui::Button("Z", buttonSize))
			values.z = resetValues.z;
		ImGui::PopFont();
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		ImGui::DragFloat("##Z", &values.z, 0.1f);
		ImGui::PopItemWidth();

		ImGui::PopStyleVar();
		
		ImGui::Columns(1);

		ImGui::PopID();
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
		const auto& entityName = entity.GetComponent<EntitySceneNameComponent>().Name;

		ImGuiTreeNodeFlags flags = (m_SelectedEntity == entity ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_OpenOnArrow;
		flags |= ImGuiTreeNodeFlags_SpanAvailWidth;
		bool opened = ImGui::TreeNodeEx((void*)(uint64_t)entity.GetID(), flags, entityName.c_str());
		
		if (ImGui::IsItemClicked())
		{
			m_SelectedEntity = entity;
		}
		
		if (opened)
		{
			ImGui::TreePop();
		}

		if (ImGui::BeginPopupContextItem())
		{
			if (ImGui::MenuItem("Delete Entity"))
			{
				if (m_SelectedEntity == entity)
					m_SelectedEntity = Entity::Null;
				m_Scene->DestroyEntity(entity);
			}

			ImGui::EndPopup();
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
			DrawTransformNode(entity, transformComponent);
		}

		DrawComponent<SpriteComponent>("Sprite", entity, [&entity, this](auto& sprite)
		{
			auto& color = sprite.Color;

			DrawTransformNode(entity, sprite);
			ImGui::ColorEdit4("Color", glm::value_ptr(color));
		});

		DrawComponent<CameraComponent>("Camera", entity, [&entity, this](auto& cameraComponent)
		{
				auto& camera = cameraComponent.Camera;

				DrawTransformNode(entity, cameraComponent);

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
	
	void SceneHierarchyPanel::DrawTransformNode(Entity entity, SceneComponent& sceneComponent)
	{
		auto& transform = sceneComponent.Transform;
		glm::vec3 rotationInDegrees = glm::degrees(transform.Rotation);

		DrawComponent<TransformComponent>("Transform", entity, [&transform, &rotationInDegrees](auto& transformComponent)
		{
			DrawVec3Control("Translation", transform.Translation, glm::vec3{0.f});
			DrawVec3Control("Rotation", rotationInDegrees, glm::vec3{0.f});
			DrawVec3Control("Scale", transform.Scale3D, glm::vec3{1.f});
			
			transform.Rotation = glm::radians(rotationInDegrees);
		}, false);
	}
}