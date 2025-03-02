#include "SceneHierarchyPanel.h"

#include "../EditorLayer.h"
#include <Eagle/UI/UI.h>
#include <Eagle/Script/ScriptEngine.h>
#include <Eagle/Debug/CPUTimings.h>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <glm/gtc/type_ptr.hpp>

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
		m_Properties = {};
	}

	void SceneHierarchyPanel::ClearSelection()
	{
		m_SelectedEntity = Entity::Null;
		m_Properties.SetSelectedComponent(SelectedComponent::None);
	}

	void SceneHierarchyPanel::SetEntitySelected(Entity entity, SelectedComponent component)
	{
		ClearSelection();

		if (entity)
		{
			m_SelectedEntity = entity;
			m_Properties.SetSelectedComponent(component);
			m_ScrollToSelected = true;
		}
	}

	bool SceneHierarchyPanel::OnImGuiRender()
	{
		EG_CPU_TIMING_SCOPED("Scene Hierarchy Panel");

		bool bChanged = false;
		bChanged |= DrawSceneHierarchy();
		
		ImGui::Begin("Properties");
		m_PropertiesHovered = ImGui::IsWindowHovered();
		if (m_SelectedEntity)
		{
			const bool bRuntime = (m_Editor.GetEditorState() == EditorState::Play);
			const bool bVolumetricsEnabled = m_Scene->GetSceneRenderer()->GetOptions().VolumetricSettings.bEnable;

			bChanged |= m_Properties.OnImGuiRender(m_SelectedEntity, bRuntime, bVolumetricsEnabled);
		}
		ImGui::End(); //Properties
		return bChanged;
	}

	bool SceneHierarchyPanel::DrawSceneHierarchy()
	{
		bool bChanged = false;

		ImGui::Begin("Scene Hierarchy");
		m_SceneHierarchyHovered = ImGui::IsWindowHovered();
		m_SceneHierarchyFocused = ImGui::IsWindowFocused();
		//TODO: Replace to "Drop on empty space"
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_ENTITY_CELL"))
			{
				uint32_t payload_n = *(uint32_t*)payload->Data;

				Entity droppedEntity((entt::entity)payload_n, m_Scene.get());
				droppedEntity.SetParent(Entity::Null);
				bChanged = true;
			}

			ImGui::EndDragDropTarget();
		}

		auto view = m_Scene->GetAllEntitiesWith<EntitySceneNameComponent>();
		for (auto& entity : view)
		{
			bChanged |= DrawEntityNode(Entity(entity, m_Scene.get()));
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
				bChanged = true;
				EG_CORE_TRACE("Created Entity");
			}

			ImGui::EndPopup();
		}

		ImGui::End(); //Scene Hierarchy

		return bChanged;
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

	bool SceneHierarchyPanel::DrawEntityNode(Entity& entity)
	{
		if (entity.HasParent()) //For drawing children use DrawChilds
			return false;

		bool bChanged = false;
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
				bChanged |= DrawChilds(entity);
				ImGui::TreePop();
			}
			return bChanged;
		}

		if (ImGui::IsItemClicked())
		{
			ClearSelection();
			m_SelectedEntity = entity;
		}

		std::string popupID = std::to_string(entity.GetID());
		if (ImGui::BeginPopupContextItem(popupID.c_str()))
		{
			if (ImGui::MenuItem("Create Entity"))
			{
				Entity newEntity = m_Scene->CreateEntity("Empty Entity");
				newEntity.SetWorldTransform(entity.GetWorldTransform());
				newEntity.SetParent(entity);
				bChanged = true;
				EG_CORE_TRACE("Created Entity");
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Delete Entity"))
			{
				if (m_SelectedEntity == entity)
					ClearSelection();
				m_Scene->DestroyEntity(entity);
				bChanged = true;
			}
			ImGui::EndPopup();
		}

		if (ImGui::BeginDragDropSource())
		{
			uint32_t selectedEntityID = m_SelectedEntity.GetID();
			const auto& selectedEntityName = entity.GetComponent<EntitySceneNameComponent>().Name;

			ImGui::SetDragDropPayload("HIERARCHY_ENTITY_CELL", &selectedEntityID, sizeof(uint32_t));
			ImGui::Text(selectedEntityName.c_str());

			ImGui::EndDragDropSource();
		}

		if (ImGui::BeginDragDropTarget()) 
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_ENTITY_CELL"))
			{
				uint32_t payload_n = *(uint32_t*)payload->Data;

				Entity droppedEntity((entt::entity)payload_n, m_Scene.get());
				if (droppedEntity.GetParent() != entity)
				{
					droppedEntity.SetParent(entity);
					bChanged = true;
				}
			}

			ImGui::EndDragDropTarget();
		}
	
		if (opened)
		{
			bChanged |= DrawChilds(entity);
			ImGui::TreePop();
		}

		return bChanged;
	}

	bool SceneHierarchyPanel::DrawChilds(Entity& entity)
	{
		bool bChanged = false;
		auto& children = entity.GetComponent<OwnershipComponent>().Children;

		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;

		for (int i = 0; i < children.size(); ++i)
		{
			Entity child = children[i];
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
					bChanged |= DrawChilds(child);
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
				if (ImGui::MenuItem("Create Entity"))
				{
					Entity newEntity = m_Scene->CreateEntity("Empty Entity");
					newEntity.SetWorldTransform(child .GetWorldTransform());
					newEntity.SetParent(child);
					EG_CORE_TRACE("Created Entity");
					bChanged = true;
				}
				if (ImGui::MenuItem("Detach from parent"))
				{
					child.SetParent(Entity::Null);
					bChanged = true;
				}
				ImGui::Separator();
				if (ImGui::MenuItem("Delete Entity"))
				{
					if (m_SelectedEntity == child)
						ClearSelection();
					m_Scene->DestroyEntity(child);
					bChanged = true;
				}

				ImGui::EndPopup();
			}

			if (ImGui::BeginDragDropSource())
			{
				uint32_t selectedEntityID = m_SelectedEntity.GetID();
				const auto& selectedEntityName = child.GetComponent<EntitySceneNameComponent>().Name;

				ImGui::SetDragDropPayload("HIERARCHY_ENTITY_CELL", &selectedEntityID, sizeof(uint32_t));
				ImGui::Text(selectedEntityName.c_str());

				ImGui::EndDragDropSource();
			}

			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_ENTITY_CELL"))
				{
					uint32_t payload_n = *(uint32_t*)payload->Data;

					Entity droppedEntity((entt::entity)payload_n, m_Scene.get());
					
					if (droppedEntity.GetParent() != child)
					{
						droppedEntity.SetParent(child);
						bChanged = true;
					}
				}

				ImGui::EndDragDropTarget();
			}

			if (openedChild)
			{
				bChanged |= DrawChilds(child);
				ImGui::TreePop();
			}
		}
	
		return bChanged;
	}

	void SceneHierarchyPanel::OnEvent(Event& e)
	{
		if (e.GetEventType() == EventType::KeyPressed)
		{
			KeyPressedEvent& keyEvent = (KeyPressedEvent&)e;
			bool bLeftControlPressed = Input::IsKeyPressed(Key::LeftControl);

			if (!m_PropertiesHovered && (m_Editor.IsViewportFocused() || m_SceneHierarchyFocused) && m_SelectedEntity)
			{
				if (keyEvent.GetKey() == Key::Delete)
				{
					m_Scene->DestroyEntity(m_SelectedEntity);
					ClearSelection();
				}
				else if (bLeftControlPressed && (keyEvent.GetKey() == Key::D) && !m_Scene->IsPlaying())
				{
					m_SelectedEntity = m_Scene->CreateFromEntity(m_SelectedEntity);
				}
			}
		}
	}
}
