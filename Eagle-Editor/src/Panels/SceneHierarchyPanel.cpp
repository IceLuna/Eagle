#include "SceneHierarchyPanel.h"

#include "../EditorLayer.h"
#include <Eagle/UI/UI.h>
#include <Eagle/Script/ScriptEngine.h>
#include <Eagle/Debug/CPUTimings.h>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <glm/gtc/type_ptr.hpp>

namespace Eagle
{
	SceneHierarchyPanel::SceneHierarchyPanel()
	{
		m_RefreshIcon = Texture2D::Create(Application::GetCorePath() / "assets/textures/Editor/refresh.png");
		SetHashID(0);
	}

	void SceneHierarchyPanel::SetHashID(uint64_t uniqueID)
	{
		m_SceneHierarchyWindowName = uniqueID == 0u ? "Scene Hierarchy" : "Scene Hierarchy###" + std::to_string(uniqueID);
		m_PropertiesWindowName = uniqueID == 0u ? "Properties" : "Properties###" + std::to_string(uniqueID);
	}

	void SceneHierarchyPanel::SetMultiSelectionEnabled(bool bEnabled)
	{
		m_bMultiSelection = bEnabled;
		if (!bEnabled && m_SelectedEntities.size() > 1)
			m_SelectedEntities.erase(m_SelectedEntities.begin(), m_SelectedEntities.end() - 1); // Keep the active one
	}

	void SceneHierarchyPanel::ClearSelection()
	{
		m_SelectedEntity = Entity::Null;
		m_SelectedEntities.clear();
		m_Properties.SetEntitySelected(Entity::Null, SelectedComponent::None);
	}

	bool SceneHierarchyPanel::IsEntitySelected(const Entity& entity) const
	{
		return entity && std::ranges::contains(m_SelectedEntities, entity);
	}

	void SceneHierarchyPanel::SetEntitySelected(Entity entity, SelectedComponent component)
	{
		ClearSelection();

		if (entity)
		{
			m_bSelectedOutside = true;
			m_SelectedEntity = entity;
			m_SelectedEntities.push_back(entity);
			m_Properties.SetEntitySelected(entity, component);
			m_ScrollToSelected = true;
		}
	}

	void SceneHierarchyPanel::SetEntitiesSelected(const std::vector<Entity>& entities, SelectedComponent component)
	{
		if (!m_bMultiSelection)
		{
			SetEntitySelected(entities.empty() ? Entity::Null : entities.back(), component);
			return;
		}

		ClearSelection();
		for (const Entity& entity : entities)
		{
			if (entity && !IsEntitySelected(entity))
				m_SelectedEntities.push_back(entity);
		}

		if (m_SelectedEntities.empty())
			return;

		m_bSelectedOutside = true;
		m_SelectedEntity = m_SelectedEntities.back();
		m_Properties.SetEntitySelected(m_SelectedEntity, component);
		m_ScrollToSelected = true;
	}

	void SceneHierarchyPanel::ToggleEntitySelected(Entity entity)
	{
		if (!entity)
			return;

		if (!m_bMultiSelection)
		{
			SetEntitySelected(entity);
			return;
		}

		m_bSelectedOutside = true;
		if (ToggleSelection(entity))
			m_ScrollToSelected = true;
	}

	void SceneHierarchyPanel::OnViewportEntityClicked(Entity entity, bool bCtrl)
	{
		if (m_bMultiSelection && bCtrl)
		{
			if (entity)
			{
				ToggleEntitySelected(entity);
			}
			return;
		}

		SetEntitySelected(entity);
	}

	bool SceneHierarchyPanel::ToggleSelection(Entity entity)
	{
		if (IsEntitySelected(entity))
		{
			DeselectEntity(entity);
			return false;
		}

		m_SelectedEntities.push_back(entity);
		SetActiveEntity(entity);
		return true;
	}

	void SceneHierarchyPanel::OnEntityRowClicked(Entity entity)
	{
		if (m_bMultiSelection && ImGui::GetIO().KeyCtrl)
		{
			ToggleSelection(entity);
			return;
		}

		ClearSelection();
		m_SelectedEntity = entity;
		m_SelectedEntities.push_back(entity);
	}

	void SceneHierarchyPanel::DeselectEntity(const Entity& entity)
	{
		auto it = std::find(m_SelectedEntities.begin(), m_SelectedEntities.end(), entity);
		if (it == m_SelectedEntities.end())
			return;

		m_SelectedEntities.erase(it);
		if (m_SelectedEntity != entity)
			return;

		if (m_SelectedEntities.empty())
			ClearSelection();
		else
			SetActiveEntity(m_SelectedEntities.back());
	}

	void SceneHierarchyPanel::RemoveDestroyedFromSelection()
	{
		if (m_SelectedEntities.empty())
			return;

		const Entity active = m_SelectedEntity;
		std::erase_if(m_SelectedEntities, [](const Entity& entity) { return !entity.IsValid() || entity.GetScene()->IsPendingDestroy(entity); });

		if (m_SelectedEntities.empty())
			ClearSelection();
		else if (m_SelectedEntities.back() != active)
			SetActiveEntity(m_SelectedEntities.back());
	}

	void SceneHierarchyPanel::SetActiveEntity(Entity entity)
	{
		m_SelectedEntity = entity;
		m_Properties.SetEntitySelected(entity, SelectedComponent::None);
	}

	bool SceneHierarchyPanel::OnImGuiRender(const Ref<Scene>& scene, bool bScenePlaying, bool bAllowOnlySingleRoot, const bool* bVolumetricsEnabledOverride)
	{
		EG_CPU_TIMING_SCOPED("Scene Hierarchy Panel");

		UpdateScene(scene);
		RemoveDestroyedFromSelection();
		m_AllowOnlySingleRoot = bAllowOnlySingleRoot;
		m_UpdateSearchResults |= scene->HasEntityListChanged() || scene->AnyEntityNameChanged();

		bool bChanged = false;
		if (m_WindowClass)
			ImGui::SetNextWindowClass(m_WindowClass);

		ImGui::Begin(m_SceneHierarchyWindowName.c_str());
		if (m_bPropertiesInHierarchyWindow)
		{
			// The list of entities has its own height, the user can change it with the splitter under it. The properties take the rest.
			// The limits are applied every frame, so that the properties stay visible when the window gets smaller
			const float rowHeight = ImGui::GetTextLineHeightWithSpacing();
			const float splitterThickness = ImGui::GetStyle().ItemSpacing.y * 2.f + 2.f;
			const float searchBarHeight = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y; // And the separator under it
			const float minListHeight = rowHeight * 2.f;
			const float minPropertiesHeight = rowHeight * 4.f;
			const float maxListHeight = glm::max(minListHeight, ImGui::GetContentRegionAvail().y - searchBarHeight - splitterThickness - minPropertiesHeight);

			constexpr float defaultListRows = 6.f;
			if (m_EntitiesListHeight <= 0.f)
				m_EntitiesListHeight = rowHeight * defaultListRows;
			const float listHeight = glm::clamp(m_EntitiesListHeight, minListHeight, maxListHeight);

			bChanged |= DrawSceneHierarchy(scene, listHeight);

			// Splitter
			{
				ImGui::InvisibleButton("##EntitiesSplitter", ImVec2(glm::max(1.f, ImGui::GetContentRegionAvail().x), splitterThickness));
				const bool bActive = ImGui::IsItemActive();
				const bool bHovered = ImGui::IsItemHovered();
				if (bActive || bHovered)
					ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);

				// What the user chose is kept even if the window is too small for it right now, so it's restored when the window gets bigger
				if (bActive)
					m_EntitiesListHeight = glm::clamp(listHeight + ImGui::GetIO().MouseDelta.y, minListHeight, maxListHeight);

				const ImVec2 min = ImGui::GetItemRectMin();
				const ImVec2 max = ImGui::GetItemRectMax();
				const float y = (min.y + max.y) * 0.5f;
				ImGui::GetWindowDrawList()->AddLine(ImVec2(min.x, y), ImVec2(max.x, y),
					ImGui::GetColorU32(bActive ? ImGuiCol_SeparatorActive : bHovered ? ImGuiCol_SeparatorHovered : ImGuiCol_Separator), bActive || bHovered ? 2.f : 1.f);
			}

			if (ImGui::BeginChild("##EntityProperties", ImVec2(0.f, 0.f), ImGuiChildFlags_None))
				bChanged |= DrawProperties(scene, bScenePlaying, bVolumetricsEnabledOverride);
			ImGui::EndChild();
		}
		else
		{
			bChanged |= DrawSceneHierarchy(scene, 0.f);
		}
		ImGui::End(); //Scene Hierarchy

		if (!m_bPropertiesInHierarchyWindow)
		{
			if (m_WindowClass)
				ImGui::SetNextWindowClass(m_WindowClass);

			ImGui::Begin(m_PropertiesWindowName.c_str());
			bChanged |= DrawProperties(scene, bScenePlaying, bVolumetricsEnabledOverride);
			ImGui::End();
		}
		m_EditTracker.EndFrame();

		m_bSelectedOutside = false; // Selected by the panel. It's an entity of the scene
		return bChanged;
	}

	void SceneHierarchyPanel::UpdateScene(const Ref<Scene>& scene)
	{
		if (m_Scene.lock() != scene)
		{
			const bool bKeepSelection = m_bSelectedOutside && m_SelectedEntity && m_SelectedEntity.GetScene() == scene.get();
			if (!bKeepSelection)
			{
				ClearSelection();
				m_Properties = {};
			}
			m_EditTracker.Reset();
			m_UpdateSearchResults = true;
			m_Scene = scene;
		}
		m_bSelectedOutside = false;
	}

	void SceneHierarchyPanel::OnUndoApplied(const UndoTransaction& transaction)
	{
		RemoveDestroyedFromSelection();

		if (m_SelectedEntity)
			return;

		for (const auto& command : transaction.Commands)
		{
			const SceneEntitiesCommand* sceneCommand = Cast<SceneEntitiesCommand>(command);
			if (!sceneCommand)
				continue;

			const Ref<Scene> scene = sceneCommand->GetScene();
			if (!scene || scene != m_Scene.lock()) // For example, the entities of entity assets
				continue;

			for (const SceneEntitiesCommand::Change& change : sceneCommand->GetChanges())
			{
				Entity entity = scene->GetEntityByGUID(change.EntityGUID);
				if (entity && !scene->IsPendingDestroy(entity))
				{
					SetEntitySelected(entity);
					return;
				}
			}
		}
	}

	bool SceneHierarchyPanel::DrawProperties(const Ref<Scene>& scene, bool bScenePlaying, const bool* bVolumetricsEnabledOverride)
	{
		bool bChanged = false;
		m_PropertiesHovered = ImGui::IsWindowHovered();
		if (m_SelectedEntity)
		{
			const bool bRuntime = bScenePlaying;
			const bool bVolumetricsEnabled = bVolumetricsEnabledOverride ? *bVolumetricsEnabledOverride : scene->GetSceneRenderer()->GetOptions().VolumetricSettings.bEnable;

			if (m_SelectedEntities.size() > 1)
				ImGui::TextDisabled("%zu entities are selected. These are the properties of the active one", m_SelectedEntities.size());

			Entity newEntitySelection = Entity::Null;
			m_Properties.SetUndoContext(m_UndoHistory, scene);
			m_EditTracker.BeginFrame(m_UndoHistory, scene, m_SelectedEntity);
			bChanged |= m_Properties.OnImGuiRender(m_SelectedEntity, bRuntime, bVolumetricsEnabled, &newEntitySelection);
			m_UpdateSearchResults |= scene->HasEntityListChanged() || scene->AnyEntityNameChanged();
			if (newEntitySelection != Entity::Null)
			{
				SetEntitySelected(newEntitySelection, m_Properties.GetSelectedComponentType());
			}
		}
		return bChanged;
	}

	void SceneHierarchyPanel::AddSearchingEntity(const Entity& entity, std::unordered_set<Entity>& output)
	{
		output.insert(entity);
		if (entity.HasParent())
			AddSearchingEntity(entity.GetParent(), output);
	}

	// Within the hierarchy's window
	// @listHeight. Height of the list of entities. 0 - the rest of the window
	bool SceneHierarchyPanel::DrawSceneHierarchy(const Ref<Scene>& scene, float listHeight)
	{
		bool bChanged = false;
		m_RootEntity = Entity::Null;
		m_bEntityRowHovered = false;

		auto view = scene->GetAllEntitiesWith<EntitySceneNameComponent>();
		
		if (m_AllowOnlySingleRoot)
		{
			for (auto& entity : view)
			{
				if (!Entity(entity, scene.get()).HasParent())
				{
					m_RootEntity = Entity(entity, scene.get());
					break;
				}
			}
		}

		// Save some space for the create and the refresh buttons
		const float padding = 2.0;
		const float width = ImGui::GetContentRegionAvail().x;
		const float buttonSize = ImGui::GetFrameHeight();

		ImGui::SetNextItemWidth(width - (buttonSize + padding) * 2.f);
		const bool bSearchInputChanged = UI::InputTextWithHint("##search", m_Search, "Search...");

		// Add button
		{
			ImGui::SameLine(0, padding);
			if (ImGui::Button("+##CreateEntity", ImVec2(buttonSize, buttonSize)))
			{
				CreateEmptyEntity(scene);
				bChanged = true;
			}
			UI::Tooltip("Create Entity");
		}

		// Refresh button
		{
			ImGui::SameLine(0, padding);

			const bool bDisableRefreshButton = m_Search.empty();
			if (bDisableRefreshButton)
				UI::PushItemDisabled();
			if (UI::ImageButton(m_RefreshIcon, ImVec2(buttonSize, buttonSize) - ImGui::GetStyle().FramePadding * 2))
				m_UpdateSearchResults = true;
			if (bDisableRefreshButton)
				UI::PopItemDisabled();
		}

		ImGui::Separator();

		if (m_Search.empty())
			m_AllowedForDisplayEntities.clear();

		const bool bPerformSearch = !m_Search.empty() && (bSearchInputChanged || m_UpdateSearchResults);
		if (bPerformSearch)
		{
			m_AllowedForDisplayEntities.clear();
			GatherSearchingEntities(scene.get(), view, m_Search, m_AllowedForDisplayEntities);
		}

		ImGui::BeginChild("SceneHierarchyScrollingRegion", ImVec2(0.f, listHeight), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);
		m_SceneHierarchyHovered = ImGui::IsWindowHovered();
		m_SceneHierarchyFocused = ImGui::IsWindowFocused();

		for (auto& entity : view)
		{
			bChanged |= DrawEntityNode(scene, Entity(entity, scene.get()), !m_Search.empty());
		}

		// There's always some empty space under the entities, even when the list is scrolled to the end.
		// It can be right-clicked to create an entity that isn't attached to another one, and an entity can be dropped onto it to detach it
		ImGui::Dummy(ImVec2(1.f, ImGui::GetTextLineHeightWithSpacing() * 2.f));

		// An entity dropped onto the empty space is detached from its parent. In the single root mode, it's attached to the root then.
		// The rows are drop targets of their own (the entity is attached to the row's entity), so this one isn't used over them
		if (!m_bEntityRowHovered && ImGui::BeginDragDropTargetCustom(ImGui::GetCurrentWindow()->InnerClipRect, ImGui::GetID("##DetachDropTarget")))
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_ENTITY_CELL"))
			{
				const uint32_t droppedEntityID = *(const uint32_t*)payload->Data;
				Entity droppedEntity((entt::entity)droppedEntityID, scene.get());
				const Entity newParent = m_AllowOnlySingleRoot ? m_RootEntity : Entity::Null;

				if (droppedEntity.IsValid() && droppedEntity != newParent && droppedEntity.GetParent() != newParent)
				{
					SceneTransaction transaction(m_UndoHistory, scene, "Detach Entity");
					transaction.Track(droppedEntity);
					droppedEntity.SetParent(newParent);
					bChanged = true;
				}
			}

			ImGui::EndDragDropTarget();
		}

		// A click on empty space deselects
		if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered() && !(m_bMultiSelection && ImGui::GetIO().KeyCtrl))
		{
			ClearSelection();
		}

		// Right-click on empty space in Scene Hierarchy
		if (ImGui::BeginPopupContextWindow("SceneHierarchyCreateEntity", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverExistingPopup | ImGuiPopupFlags_NoOpenOverItems))
		{
			if (ImGui::MenuItem("Create Entity"))
			{
				CreateEmptyEntity(scene);
				bChanged = true;
			}

			ImGui::EndPopup();
		}

		ImGui::EndChild();

		m_UpdateSearchResults = false;

		return bChanged;
	}

	void SceneHierarchyPanel::CreateEmptyEntity(const Ref<Scene>& scene)
	{
		SceneTransaction transaction(m_UndoHistory, scene, "Create Entity");

		Entity newEntity = scene->CreateEntity("New Entity");
		if (m_AllowOnlySingleRoot && m_RootEntity)
			newEntity.SetParent(m_RootEntity);

		transaction.TrackCreated(newEntity);
		SetEntitySelected(newEntity);
	}

	void SceneHierarchyPanel::UpdateEntityRowHovered()
	{
		// The row of the last item, the tree node. The mouse is tested against the visible part of it (the clip rect).
		// The gap between rows counts as the rows, so the empty space's drop target doesn't flicker while an entity is dragged over the list
		const float halfSpacing = ImGui::GetStyle().ItemSpacing.y * 0.5f;
		const ImVec2 min = ImGui::GetItemRectMin() - ImVec2(0.f, halfSpacing);
		const ImVec2 max = ImGui::GetItemRectMax() + ImVec2(0.f, halfSpacing);
		if (ImGui::IsMouseHoveringRect(min, max))
			m_bEntityRowHovered = true;
	}

	bool SceneHierarchyPanel::DrawEntityNode(const Ref<Scene>& scene, Entity entity, bool bFilteredOnly)
	{
		if (entity.HasParent()) //For drawing children use DrawChilds
			return false;

		if (bFilteredOnly && !m_AllowedForDisplayEntities.contains(entity))
		{
			return false;
		}

		EG_CORE_ASSERT(!m_AllowOnlySingleRoot || entity == m_RootEntity, "Only one entity can be at the root");

		bool bChanged = false;
		const auto& entityName = entity.GetName();

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

		ImGuiTreeNodeFlags flags = (IsEntitySelected(entity) ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_OpenOnArrow
			| ImGuiTreeNodeFlags_SpanAvailWidth | (entity.HasChildren() ? 0 : ImGuiTreeNodeFlags_Leaf);
		bool opened = ImGui::TreeNodeEx((void*)(uint64_t)entity.GetID(), flags, entityName.c_str());

		if (!ImGui::IsItemVisible()) // Early-exit if it's not visible
		{
			if (opened)
			{
				bChanged |= DrawChilds(scene, entity, bFilteredOnly);
				ImGui::TreePop();
			}
			return bChanged;
		}
		UpdateEntityRowHovered();

		if (ImGui::IsItemClicked())
			OnEntityRowClicked(entity);

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
				SceneTransaction transaction(m_UndoHistory, scene, "Create Entity");
				Entity newEntity = scene->CreateEntity("New Entity");
				newEntity.SetWorldTransform(entity.GetWorldTransform());
				newEntity.SetParent(entity);
				transaction.TrackCreated(newEntity);
				SetEntitySelected(newEntity);
				bChanged = true;
			}
			if (bCanDelete)
			{
				const bool bSelection = m_SelectedEntities.size() > 1 && IsEntitySelected(entity);
				ImGui::Separator();
				if (ImGui::MenuItem(bSelection ? "Delete Selected Entities" : "Delete Entity"))
				{
					if (bSelection)
					{
						DestroySelectedEntities(scene, false);
					}
					else
					{
						DeselectEntity(entity);
						DestroyEntity(scene, entity, false);
					}
					bChanged = true;
				}
				if (ImGui::MenuItem(bSelection ? "Delete Selected Entities and their Children" : "Delete Entity and its Children"))
				{
					if (bSelection)
					{
						DestroySelectedEntities(scene, true);
					}
					else
					{
						DeselectEntity(entity);
						DestroyEntity(scene, entity, true);
					}
					bChanged = true;
				}
			}
			ImGui::EndPopup();
		}

		if (!m_AllowOnlySingleRoot && ImGui::BeginDragDropSource())
		{
			uint32_t draggedEntityID = entity.GetID();
			const auto& draggedEntityName = entity.GetName();

			ImGui::SetDragDropPayload("HIERARCHY_ENTITY_CELL", &draggedEntityID, sizeof(uint32_t));
			ImGui::Text(draggedEntityName.c_str());

			ImGui::EndDragDropSource();
		}

		if (ImGui::BeginDragDropTarget()) 
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_ENTITY_CELL"))
			{
				uint32_t payload_n = *(uint32_t*)payload->Data;

				Entity droppedEntity((entt::entity)payload_n, scene.get());
				if (!droppedEntity.IsParentOf(entity) && droppedEntity.GetParent() != entity)
				{
					SceneTransaction transaction(m_UndoHistory, scene, "Attach Entity");
					transaction.Track(droppedEntity);
					droppedEntity.SetParent(entity);
					bChanged = true;
				}
			}

			ImGui::EndDragDropTarget();
		}
	
		if (opened)
		{
			bChanged |= DrawChilds(scene, entity, bFilteredOnly);
			ImGui::TreePop();
		}

		return bChanged;
	}

	bool SceneHierarchyPanel::DrawChilds(const Ref<Scene>& scene, Entity entity, bool bFilteredOnly)
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

			ImGuiTreeNodeFlags childTreeFlags = flags | (child.HasChildren() ? 0 : ImGuiTreeNodeFlags_Leaf) | (IsEntitySelected(child) ? ImGuiTreeNodeFlags_Selected : 0);

			//If selected child of this entity, open tree node
			if (m_SelectedEntity && m_SelectedEntity != child)
				if (child.IsParentOf(m_SelectedEntity))
					ImGui::SetNextItemOpen(true);
			if (m_SelectedEntity == child && m_ScrollToSelected)
			{
				m_ScrollToSelected = false;
				ImGui::SetScrollHereY(0.f);
			}

			const auto& childName = child.GetName();
			bool openedChild = ImGui::TreeNodeEx((void*)(uint64_t)child.GetID(), childTreeFlags, childName.c_str());

			if (!ImGui::IsItemVisible()) // Early-exit
			{
				if (openedChild)
				{
					bChanged |= DrawChilds(scene, child, bFilteredOnly);
					ImGui::TreePop();
				}
				continue;
			}
			UpdateEntityRowHovered();

			if (ImGui::IsItemClicked())
				OnEntityRowClicked(child);

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
					SceneTransaction transaction(m_UndoHistory, scene, "Create Entity");
					Entity newEntity = scene->CreateEntity("New Entity");
					newEntity.SetWorldTransform(child.GetWorldTransform());
					newEntity.SetParent(child);
					transaction.TrackCreated(newEntity);
					SetEntitySelected(newEntity);
					bChanged = true;
				}
				const bool bDisableDetach = m_AllowOnlySingleRoot && child.GetParent() == m_RootEntity;
				if (bDisableDetach)
					UI::PushItemDisabled();
				if (ImGui::MenuItem("Detach from parent"))
				{
					SceneTransaction transaction(m_UndoHistory, scene, "Detach Entity");
					transaction.Track(child);
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

				const bool bSelection = m_SelectedEntities.size() > 1 && IsEntitySelected(child);
				if (ImGui::MenuItem(bSelection ? "Delete Selected Entities" : "Delete Entity"))
				{
					if (bSelection)
					{
						DestroySelectedEntities(scene, false);
					}
					else
					{
						DeselectEntity(child);
						DestroyEntity(scene, child, false);
					}
					bChanged = true;
				}
				if (ImGui::MenuItem(bSelection ? "Delete Selected Entities and their Children" : "Delete Entity and its Children"))
				{
					if (bSelection)
					{
						DestroySelectedEntities(scene, true);
					}
					else
					{
						DeselectEntity(child);
						DestroyEntity(scene, child, true);
					}
					bChanged = true;
				}

				ImGui::EndPopup();
			}

			if (ImGui::BeginDragDropSource())
			{
				uint32_t draggedEntityID = child.GetID();
				const auto& draggedEntityName = child.GetName();

				ImGui::SetDragDropPayload("HIERARCHY_ENTITY_CELL", &draggedEntityID, sizeof(uint32_t));
				ImGui::Text(draggedEntityName.c_str());

				ImGui::EndDragDropSource();
			}

			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_ENTITY_CELL"))
				{
					uint32_t payload_n = *(uint32_t*)payload->Data;

					Entity droppedEntity((entt::entity)payload_n, scene.get());
					
					if (!droppedEntity.IsParentOf(child) && droppedEntity.GetParent() != child)
					{
						SceneTransaction transaction(m_UndoHistory, scene, "Attach Entity");
						transaction.Track(droppedEntity);
						droppedEntity.SetParent(child);
						bChanged = true;
					}
				}

				ImGui::EndDragDropTarget();
			}

			if (openedChild)
			{
				bChanged |= DrawChilds(scene, child, bFilteredOnly);
				ImGui::TreePop();
			}
		}
	
		return bChanged;
	}

	bool SceneHierarchyPanel::OnEvent(const Ref<Scene>& scene, Event& e, bool bViewportFocused)
	{
		if (!scene)
			return false;

		UpdateScene(scene);
		RemoveDestroyedFromSelection();
		const bool bChanged = Event::Dispatch<KeyPressedEvent>(e, [this, &scene, bViewportFocused](KeyPressedEvent& keyEvent) { return OnKeyPressed(scene, keyEvent, bViewportFocused); });
		m_bSelectedOutside = false; // Selected by the panel

		return bChanged;
	}

	void SceneHierarchyPanel::DestroyEntity(const Ref<Scene>& scene, Entity entity, bool bDestroyChildren)
	{
		SceneTransaction transaction(m_UndoHistory, scene, bDestroyChildren ? "Delete Entity and Children" : "Delete Entity");
		transaction.Track(entity, true); // Its children are tracked either way. They're either destroyed too, or moved to its parent
		scene->DestroyEntity(entity, bDestroyChildren);
	}

	void SceneHierarchyPanel::DestroySelectedEntities(const Ref<Scene>& scene, bool bDestroyChildren)
	{
		// The root entity can't be deleted
		std::vector<Entity> entities = m_SelectedEntities;
		if (m_AllowOnlySingleRoot)
			std::erase_if(entities, [](const Entity& entity) { return !entity.HasParent(); });

		// Children are destroyed with their parents
		if (bDestroyChildren)
			entities = Entity::GetTopmostEntities(entities);

		if (entities.empty())
			return;

		if (entities.size() == 1)
		{
			DestroyEntity(scene, entities[0], bDestroyChildren);
		}
		else
		{
			SceneTransaction transaction(m_UndoHistory, scene, bDestroyChildren ? "Delete Entities and Children" : "Delete Entities");
			for (const Entity& entity : entities)
				transaction.Track(entity, true); // Their children are tracked either way. They're either destroyed too, or moved to their parents
			for (const Entity& entity : entities)
				scene->DestroyEntity(entity, bDestroyChildren);
		}
		ClearSelection();
	}

	void SceneHierarchyPanel::DuplicateSelectedEntities(const Ref<Scene>& scene)
	{
		// An entity is duplicated with its children
		std::vector<Entity> sources = Entity::GetTopmostEntities(m_SelectedEntities);
		if (m_AllowOnlySingleRoot)
			std::erase_if(sources, [](const Entity& entity) { return !entity.HasParent(); });

		if (sources.empty())
			return;

		SceneTransaction transaction(m_UndoHistory, scene, sources.size() == 1 ? "Duplicate Entity" : "Duplicate Entities");
		std::vector<Entity> duplicates;
		duplicates.reserve(sources.size());
		for (Entity& source : sources)
		{
			Entity newEntity = scene->CreateFromEntity(source);
			newEntity.SetParent(source.GetParent());
			transaction.TrackCreated(newEntity);
			duplicates.push_back(newEntity);
		}

		// The duplicates are selected in the order of their sources
		if (duplicates.size() == 1)
		{
			ClearSelection();
			m_SelectedEntity = duplicates[0];
			m_SelectedEntities.push_back(duplicates[0]);
		}
		else
		{
			SetEntitiesSelected(duplicates);
		}
	}

	bool SceneHierarchyPanel::OnKeyPressed(const Ref<Scene>& scene, KeyPressedEvent& e, bool bViewportFocused)
	{
		const bool bAllowAction = m_SelectedEntity && (!m_AllowOnlySingleRoot ||
			std::any_of(m_SelectedEntities.begin(), m_SelectedEntities.end(), [](const Entity& entity) { return entity.HasParent(); }));
		const bool bShift = Input::IsKeyPressed(Key::LeftShift) || Input::IsKeyPressed(Key::RightShift);

		if (bAllowAction && !m_PropertiesHovered && (bViewportFocused || m_SceneHierarchyFocused))
		{
			if (e.GetKey() == Key::Delete)
			{
				const bool bDeleteChildren = bShift;
				DestroySelectedEntities(scene, bDeleteChildren);
				return true;
			}
			else if (Input::IsKeyPressed(Key::LeftControl) && (e.GetKey() == Key::D) && !scene->IsPlaying())
			{
				DuplicateSelectedEntities(scene);
				return true;
			}
		}

		return false;
	}
}
