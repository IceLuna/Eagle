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
			m_bSelectedOutside = true;
			m_SelectedEntity = entity;
			m_Properties.SetEntitySelected(entity, component);
			m_ScrollToSelected = true;
		}
	}

	bool SceneHierarchyPanel::OnImGuiRender(const Ref<Scene>& scene, bool bScenePlaying, bool bAllowOnlySingleRoot, const bool* bVolumetricsEnabledOverride)
	{
		EG_CPU_TIMING_SCOPED("Scene Hierarchy Panel");

		UpdateScene(scene);
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
		if (m_SelectedEntity && !m_SelectedEntity.IsValid())
			ClearSelection();

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

		// TODO: Replace to "Drop on empty space"
		if (!m_AllowOnlySingleRoot && ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_ENTITY_CELL"))
			{
				uint32_t payload_n = *(uint32_t*)payload->Data;

				Entity droppedEntity((entt::entity)payload_n, scene.get());
				SceneTransaction transaction(m_UndoHistory, scene, "Detach Entity");
				transaction.Track(droppedEntity);
				droppedEntity.SetParent(Entity::Null);
				bChanged = true;
			}

			ImGui::EndDragDropTarget();
		}

		// Save some space for the refresh button
		const float padding = 2.0;
		const float width = ImGui::GetContentRegionAvail().x;
		const float buttonSize = ImGui::GetFrameHeight();
		ImGui::SetNextItemWidth(width - buttonSize - padding);
		const bool bSearchInputChanged = UI::InputTextWithHint("##search", m_Search, "Search...");

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

		auto view = scene->GetAllEntitiesWith<EntitySceneNameComponent>();

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

		if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered())
		{
			ClearSelection();
		}

		//Right-click on empty space in Scene Hierarchy
		if (ImGui::BeginPopupContextWindow("SceneHierarchyCreateEntity", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverExistingPopup | ImGuiPopupFlags_NoOpenOverItems))
		{
			if (ImGui::MenuItem("Create Entity"))
			{
				SceneTransaction transaction(m_UndoHistory, scene, "Create Entity");
				Entity newEntity = scene->CreateEntity("New Entity");
				if (m_AllowOnlySingleRoot)
				{
					newEntity.SetParent(m_RootEntity);
				}
				transaction.TrackCreated(newEntity);
				SetEntitySelected(newEntity);
				bChanged = true;
			}

			ImGui::EndPopup();
		}

		ImGui::EndChild();

		m_UpdateSearchResults = false;

		return bChanged;
	}

	bool SceneHierarchyPanel::DrawEntityNode(const Ref<Scene>& scene, Entity entity, bool bFilteredOnly)
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

		ImGuiTreeNodeFlags flags = (m_SelectedEntity == entity ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_OpenOnArrow
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
				ImGui::Separator();
				if (ImGui::MenuItem("Delete Entity"))
				{
					if (m_SelectedEntity == entity)
						ClearSelection();
					DestroyEntity(scene, entity, false);
					bChanged = true;
				}
				if (ImGui::MenuItem("Delete Entity and its Children"))
				{
					if (m_SelectedEntity == entity)
						ClearSelection();
					DestroyEntity(scene, entity, true);
					bChanged = true;
				}
			}
			ImGui::EndPopup();
		}

		if (!m_AllowOnlySingleRoot && ImGui::BeginDragDropSource())
		{
			uint32_t selectedEntityID = m_SelectedEntity.GetID();
			const auto& selectedEntityName = entity.GetName();

			ImGui::SetDragDropPayload("HIERARCHY_ENTITY_CELL", &selectedEntityID, sizeof(uint32_t));
			ImGui::Text(selectedEntityName.c_str());

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
				if (ImGui::MenuItem("Delete Entity"))
				{
					if (m_SelectedEntity == child)
						ClearSelection();
					DestroyEntity(scene, child, false);
					bChanged = true;
				}
				if (ImGui::MenuItem("Delete Entity and its Children"))
				{
					if (m_SelectedEntity == child)
						ClearSelection();
					DestroyEntity(scene, child, true);
					bChanged = true;
				}

				ImGui::EndPopup();
			}

			if (ImGui::BeginDragDropSource())
			{
				uint32_t selectedEntityID = m_SelectedEntity.GetID();
				const auto& selectedEntityName = child.GetName();

				ImGui::SetDragDropPayload("HIERARCHY_ENTITY_CELL", &selectedEntityID, sizeof(uint32_t));
				ImGui::Text(selectedEntityName.c_str());

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

	bool SceneHierarchyPanel::OnKeyPressed(const Ref<Scene>& scene, KeyPressedEvent& e, bool bViewportFocused)
	{
		const bool bAllowAction = m_SelectedEntity && (!m_AllowOnlySingleRoot || m_SelectedEntity.HasParent());
		const bool bShift = Input::IsKeyPressed(Key::LeftShift) || Input::IsKeyPressed(Key::RightShift);

		if (bAllowAction && !m_PropertiesHovered && (bViewportFocused || m_SceneHierarchyFocused))
		{
			if (e.GetKey() == Key::Delete)
			{
				const bool bDeleteChildren = bShift;
				DestroyEntity(scene, m_SelectedEntity, bDeleteChildren);
				ClearSelection();
				return true;
			}
			else if (Input::IsKeyPressed(Key::LeftControl) && (e.GetKey() == Key::D) && !scene->IsPlaying())
			{
				SceneTransaction transaction(m_UndoHistory, scene, "Duplicate Entity");
				Entity newEntity = scene->CreateFromEntity(m_SelectedEntity);
				newEntity.SetParent(m_SelectedEntity.GetParent());
				transaction.TrackCreated(newEntity);
				m_SelectedEntity = newEntity;
				return true;
			}
		}

		return false;
	}
}
