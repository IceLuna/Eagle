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
	SceneHierarchyPanel::SceneHierarchyPanel()
	{
		SetHashID(0);
	}

	void SceneHierarchyPanel::SetHashID(uint64_t uniqueID)
	{
		m_SceneHierarchyWindowName = uniqueID == 0u ? "Scene Hierarchy" : "Scene Hierarchy##" + std::to_string(uniqueID);
		m_PropertiesWindowName = uniqueID == 0u ? "Properties" : "Properties##" + std::to_string(uniqueID);
	}

	void SceneHierarchyPanel::ClearSelection()
	{
		m_SelectedEntity = Entity::Null;
		m_Properties.SetEntitySelected(Entity::Null, SelectedComponent::None);
	}

	void SceneHierarchyPanel::SetEntitySelected(Entity entity, SelectedComponent component)
	{
		ClearSelection();

		if (entity)
		{
			m_Scene = entity.GetScene();
			m_SelectedEntity = entity;
			m_Properties.SetEntitySelected(entity, component);
			m_ScrollToSelected = true;
		}
	}

	bool SceneHierarchyPanel::OnImGuiRender(const Ref<Scene>& scene, bool bScenePlaying, bool bAllowOnlySingleRoot, const bool* bVolumetricsEnabledOverride)
	{
		EG_CPU_TIMING_SCOPED("Scene Hierarchy Panel");

		if (m_Scene != scene.get())
		{
			ClearSelection();
			m_Properties = {};
		}
		m_Scene = scene.get();
		m_AllowOnlySingleRoot = bAllowOnlySingleRoot;
		bool bChanged = false;
		bChanged |= DrawSceneHierarchy();
		
		ImGui::Begin(m_PropertiesWindowName.c_str());
		m_PropertiesHovered = ImGui::IsWindowHovered();
		if (m_SelectedEntity)
		{
			const bool bRuntime = bScenePlaying;
			const bool bVolumetricsEnabled = bVolumetricsEnabledOverride ? *bVolumetricsEnabledOverride : m_Scene->GetSceneRenderer()->GetOptions().VolumetricSettings.bEnable;

			bChanged |= m_Properties.OnImGuiRender(m_SelectedEntity, bRuntime, bVolumetricsEnabled);
		}
		ImGui::End(); //Properties

		return bChanged;
	}

	void SceneHierarchyPanel::AddSearchingEntity(const Entity& entity, std::unordered_set<Entity>& output)
	{
		output.insert(entity);
		if (entity.HasParent())
			AddSearchingEntity(entity.GetParent(), output);
	}

	bool SceneHierarchyPanel::DrawSceneHierarchy()
	{
		bool bChanged = false;
		m_RootEntity = Entity::Null;

		ImGui::Begin(m_SceneHierarchyWindowName.c_str());
		m_SceneHierarchyHovered = ImGui::IsWindowHovered();
		m_SceneHierarchyFocused = ImGui::IsWindowFocused();
		//TODO: Replace to "Drop on empty space"
		if (!m_AllowOnlySingleRoot && ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_ENTITY_CELL"))
			{
				uint32_t payload_n = *(uint32_t*)payload->Data;

				Entity droppedEntity((entt::entity)payload_n, m_Scene);
				droppedEntity.SetParent(Entity::Null);
				bChanged = true;
			}

			ImGui::EndDragDropTarget();
		}

		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
		const bool bSearchInputChanged = UI::InputTextWithHint("##search", m_Search, "Search...");
		ImGui::Separator();

		auto view = m_Scene->GetAllEntitiesWith<EntitySceneNameComponent>();

		m_AllowedForDisplayEntities.clear();
		if (!m_Search.empty())
		{
			GatherSearchingEntities(view, m_Search, m_AllowedForDisplayEntities);
		}

		for (auto& entity : view)
		{
			bChanged |= DrawEntityNode(Entity(entity, m_Scene), !m_Search.empty());
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
				Entity newEntity = m_Scene->CreateEntity("New Entity");
				if (m_AllowOnlySingleRoot)
				{
					newEntity.SetParent(m_RootEntity);
				}
				SetEntitySelected(newEntity);
				bChanged = true;
			}

			ImGui::EndPopup();
		}

		ImGui::End(); //Scene Hierarchy

		return bChanged;
	}

	bool SceneHierarchyPanel::DrawEntityNode(Entity entity, bool bFilteredOnly)
	{
		if (entity.HasParent()) //For drawing children use DrawChilds
			return false;

		if (bFilteredOnly && !m_AllowedForDisplayEntities.contains(entity))
		{
			return false;
		}

		if (m_AllowOnlySingleRoot)
		{
			EG_CORE_ASSERT(!m_RootEntity);
			m_RootEntity = entity;
		}

		bool bChanged = false;
		const auto& entityName = entity.GetComponent<EntitySceneNameComponent>().Name;

		//If selected child of this entity, open tree node
		if (m_SelectedEntity && m_SelectedEntity != entity)
		{
			if (entity.IsParentOf(m_SelectedEntity))
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
				bChanged |= DrawChilds(entity, bFilteredOnly);
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
			if (!m_AllowOnlySingleRoot)
			{
				if (ImGui::MenuItem("Create Entity Asset from this"))
				{
					auto& cb = ContentBrowserPanel::Get();
					AssetEntity::Create(cb.GetCurrentRelativeDirectory(), entity.GetName(), entity);
					cb.RefreshBrowserContent();
				}
				ImGui::Separator();
			}

			const bool bCanDelete = !m_AllowOnlySingleRoot || entity.HasParent(); // Can't delete the root entity
			if (ImGui::MenuItem("Create Entity"))
			{
				Entity newEntity = m_Scene->CreateEntity("New Entity");
				newEntity.SetWorldTransform(entity.GetWorldTransform());
				newEntity.SetParent(entity);
				SetEntitySelected(newEntity);
				bChanged = true;
			}
			if (bCanDelete)
			{
				ImGui::Separator();
				if (ImGui::MenuItem("Delete Entity"))
				{
					if (m_SelectedEntity == entity)
						ClearSelection();
					m_Scene->DestroyEntity(entity);
					bChanged = true;
				}
				if (ImGui::MenuItem("Delete Entity and its Children"))
				{
					if (m_SelectedEntity == entity)
						ClearSelection();
					m_Scene->DestroyEntity(entity, true);
					bChanged = true;
				}
			}
			ImGui::EndPopup();
		}

		if (!m_AllowOnlySingleRoot && ImGui::BeginDragDropSource())
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

				Entity droppedEntity((entt::entity)payload_n, m_Scene);
				if (!droppedEntity.IsParentOf(entity) && droppedEntity.GetParent() != entity)
				{
					droppedEntity.SetParent(entity);
					bChanged = true;
				}
			}

			ImGui::EndDragDropTarget();
		}
	
		if (opened)
		{
			bChanged |= DrawChilds(entity, bFilteredOnly);
			ImGui::TreePop();
		}

		return bChanged;
	}

	bool SceneHierarchyPanel::DrawChilds(Entity entity, bool bFilteredOnly)
	{
		bool bChanged = false;
		auto& children = entity.GetComponent<OwnershipComponent>().Children;

		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;

		for (int i = 0; i < children.size(); ++i)
		{
			Entity child = children[i];
			if (bFilteredOnly && !m_AllowedForDisplayEntities.contains(child))
			{
				continue;
			}

			ImGuiTreeNodeFlags childTreeFlags = flags | (child.HasChildren() ? 0 : ImGuiTreeNodeFlags_Leaf) | (m_SelectedEntity == child ? ImGuiTreeNodeFlags_Selected : 0);

			//If selected child of this entity, open tree node
			if (m_SelectedEntity && m_SelectedEntity != child)
				if (child.IsParentOf(m_SelectedEntity))
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
					bChanged |= DrawChilds(child, bFilteredOnly);
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
				if (!m_AllowOnlySingleRoot)
				{
					if (ImGui::MenuItem("Create Entity Asset from this"))
					{
						auto& cb = ContentBrowserPanel::Get();
						AssetEntity::Create(cb.GetCurrentRelativeDirectory(), entity.GetName(), entity);
						cb.RefreshBrowserContent();
					}
					ImGui::Separator();
				}
				if (ImGui::MenuItem("Create Entity"))
				{
					Entity newEntity = m_Scene->CreateEntity("New Entity");
					newEntity.SetWorldTransform(child.GetWorldTransform());
					newEntity.SetParent(child);
					SetEntitySelected(newEntity);
					bChanged = true;
				}
				const bool bDisableDetach = m_AllowOnlySingleRoot && child.GetParent() == m_RootEntity;
				if (bDisableDetach)
					UI::PushItemDisabled();
				if (ImGui::MenuItem("Detach from parent"))
				{
					if (m_AllowOnlySingleRoot)
					{
						child.SetParent(m_RootEntity);
					}
					else
					{
						child.SetParent(Entity::Null);
					}
					bChanged = true;
				}
				if (bDisableDetach)
					UI::PopItemDisabled();
				ImGui::Separator();
				if (ImGui::MenuItem("Delete Entity"))
				{
					if (m_SelectedEntity == child)
						ClearSelection();
					m_Scene->DestroyEntity(child);
					bChanged = true;
				}
				if (ImGui::MenuItem("Delete Entity and its Children"))
				{
					if (m_SelectedEntity == child)
						ClearSelection();
					m_Scene->DestroyEntity(child, true);
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

					Entity droppedEntity((entt::entity)payload_n, m_Scene);
					
					if (!droppedEntity.IsParentOf(child) && droppedEntity.GetParent() != child)
					{
						droppedEntity.SetParent(child);
						bChanged = true;
					}
				}

				ImGui::EndDragDropTarget();
			}

			if (openedChild)
			{
				bChanged |= DrawChilds(child, bFilteredOnly);
				ImGui::TreePop();
			}
		}
	
		return bChanged;
	}

	bool SceneHierarchyPanel::OnEvent(const Ref<Scene>& scene, Event& e, bool bViewportFocused)
	{
		m_Scene = scene.get();
		return Event::Dispatch<KeyPressedEvent>(e, EG_BIND_FN(SceneHierarchyPanel::OnKeyPressed), bViewportFocused);
	}

	bool SceneHierarchyPanel::OnKeyPressed(KeyPressedEvent& e, bool bViewportFocused)
	{
		const bool bAllowAction = m_SelectedEntity && (!m_AllowOnlySingleRoot || m_SelectedEntity.HasParent());
		const bool bShift = Input::IsKeyPressed(Key::LeftShift) || Input::IsKeyPressed(Key::RightShift);

		if (bAllowAction && !m_PropertiesHovered && (bViewportFocused || m_SceneHierarchyFocused))
		{
			if (e.GetKey() == Key::Delete)
			{
				const bool bDeleteChildren = bShift;
				m_Scene->DestroyEntity(m_SelectedEntity, bDeleteChildren);
				ClearSelection();
				return true;
			}
			else if (Input::IsKeyPressed(Key::LeftControl) && (e.GetKey() == Key::D) && !m_Scene->IsPlaying())
			{
				Entity newEntity = m_Scene->CreateFromEntity(m_SelectedEntity);
				newEntity.SetParent(m_SelectedEntity.GetParent());
				m_SelectedEntity = newEntity;
				return true;
			}
		}

		return false;
	}
}
