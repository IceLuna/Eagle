#include "SceneHierarchyPanel.h"

#include <imgui/imgui.h>
#include <glm/gtc/type_ptr.hpp>

namespace Eagle
{
	SceneHierarchyPanel::SceneHierarchyPanel(const Ref<Scene>& context)
	{
		SetContext(context);
	}

	void SceneHierarchyPanel::SetContext(const Ref<Scene>& context)
	{
		m_Context = context;
	}

	void SceneHierarchyPanel::OnImGuiRender()
	{
		ImGui::Begin("Scene Hierarchy");
		m_Context->m_Registry.each([&](entt::entity entityID)
		{
			Entity entity = Entity(entityID, m_Context.get());
			DrawEntityNode(entity);
		});

		if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered())
		{
			m_SelectedEntity = Entity::Null;
		}

		ImGui::End();

		ImGui::Begin("Properties");
		if (m_SelectedEntity)
		{
			DrawProperties(m_SelectedEntity);
		}
		ImGui::End();
	}

	void SceneHierarchyPanel::DrawEntityNode(Entity entity)
	{
		const auto& entityName = entity.GetComponent<EntitySceneNameComponent>().Name;

		ImGuiTreeNodeFlags flags = (m_SelectedEntity == entity ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_OpenOnArrow;
		bool opened = ImGui::TreeNodeEx((void*)(uint64_t)entity.GetID(), flags, entityName.c_str());
		
		if (ImGui::IsItemClicked())
		{
			m_SelectedEntity = entity;
		}
		
		if (opened)
		{
			ImGui::TreePop();
		}
	}

	void SceneHierarchyPanel::DrawProperties(Entity entity)
	{
		if (entity.HasComponent<EntitySceneNameComponent>())
		{
			auto& entityName = entity.GetComponent<EntitySceneNameComponent>().Name;
			char buffer[256];
			memset(buffer, 0, sizeof(buffer));
			strcpy_s(buffer, sizeof(buffer), entityName.c_str());

			if (ImGui::InputText("Name", buffer, sizeof(buffer)))
			{
				//TODO: Add Check for emtpy input
				entityName = std::string(buffer);
			}
		}
		
		if (entity.HasComponent<TransformComponent>())
		{
			auto& transform = entity.GetComponent<TransformComponent>().Transform;
			DrawTransformNode(transform);
		}

		if (entity.HasComponent<SpriteComponent>())
		{
			if (ImGui::TreeNodeEx((void*)typeid(SpriteComponent).hash_code(), ImGuiTreeNodeFlags_DefaultOpen, "Sprite"))
			{
				auto& sprite= entity.GetComponent<SpriteComponent>();
				auto& color = sprite.Color;

				auto& transform = sprite.Transform;

				if (ImGui::DragFloat3("Translation", glm::value_ptr(transform.Translation), 0.1f))
				{
				}
				if (ImGui::DragFloat3("Rotation", glm::value_ptr(transform.Rotation), 0.1f))
				{
				}
				if (ImGui::DragFloat3("Scale", glm::value_ptr(transform.Scale3D), 0.1f))
				{
				}

				if (ImGui::ColorEdit4("Color", glm::value_ptr(color)))
				{}

				ImGui::TreePop();
			}
		}

		if (entity.HasComponent<CameraComponent>())
		{
			if (ImGui::TreeNodeEx((void*)typeid(CameraComponent).hash_code(), ImGuiTreeNodeFlags_DefaultOpen, "Camera Component"))
			{
				auto& cameraComponent = entity.GetComponent<CameraComponent>();
				auto& camera = cameraComponent.Camera;

				DrawTransformNode(cameraComponent.Transform);

				ImGui::Checkbox("Primary", &cameraComponent.Primary);

				const char* projectionModesStrings[] = {"Perspective", "Orthographic"};
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

				ImGui::TreePop();
			}
		}
	}
	void SceneHierarchyPanel::DrawTransformNode(Transform& transform)
	{
		if (ImGui::TreeNodeEx((void*)typeid(TransformComponent).hash_code(), ImGuiTreeNodeFlags_DefaultOpen, "Transform"))
		{
			if (ImGui::DragFloat3("Translation", glm::value_ptr(transform.Translation), 0.1f))
			{
			}
			if (ImGui::DragFloat3("Rotation", glm::value_ptr(transform.Rotation), 0.1f))
			{
			}
			if (ImGui::DragFloat3("Scale", glm::value_ptr(transform.Scale3D), 0.1f))
			{
			}

			ImGui::TreePop();
		}
	}
}