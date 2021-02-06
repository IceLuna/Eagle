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
			if (ImGui::TreeNodeEx((void*)typeid(TransformComponent).hash_code(), ImGuiTreeNodeFlags_DefaultOpen, "Transform"))
			{
				auto& transform = entity.GetComponent<TransformComponent>().Transform;

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
	}
}