#pragma once

#include "Eagle.h"
#include "EntityPropertiesPanel.h"
#include "../Undo/SceneUndo.h"

struct ImGuiWindowClass;

namespace Eagle
{
	class EditorLayer;
	class Material;

	class SceneHierarchyPanel
	{
	public:
		SceneHierarchyPanel();

		void SetHashID(uint64_t uniqueID);

		// If set, it's applied to the panel's windows to restrict where they can be docked.
		// The pointer must stay valid while the panel is used
		void SetWindowClass(const ImGuiWindowClass* windowClass) { m_WindowClass = windowClass; }

		// Draws the properties of the selected entity under the hierarchy, in the hierarchy's window, instead of in a window of their own.
		void SetPropertiesInHierarchyWindow(bool bEnabled) { m_bPropertiesInHierarchyWindow = bEnabled; }

		// Note: even if it's enabled, only one of them is active (the one selected last).
		// Meaning only its properties are drawn
		void SetMultiSelectionEnabled(bool bEnabled);
		bool IsMultiSelectionEnabled() const { return m_bMultiSelection; }

		void ClearSelection();

		bool OnEvent(const Ref<Scene>& scene, Event& e, bool bViewportFocused);

		Entity GetSelectedEntity() const { return m_SelectedEntity; }

		// In the order they were selected, the active one is the last
		const std::vector<Entity>& GetSelectedEntities() const { return m_SelectedEntities; }
		bool IsEntitySelected(const Entity& entity) const;

		// Selects only this entity
		void SetEntitySelected(Entity entity, SelectedComponent component = SelectedComponent::None);

		// Selects these entities. The last one becomes active, with the component selected.
		// Only the last one is selected if multi selection is disabled
		void SetEntitiesSelected(const std::vector<Entity>& entities, SelectedComponent component = SelectedComponent::None);

		// Ctrl+click. Selects the entity, which becomes active, if it isn't selected. Otherwise, deselects it.
		// Selects only this entity if multi selection is disabled
		void ToggleEntitySelected(Entity entity);

		// An entity was clicked in a viewport. Null - nothing was clicked.
		// If Ctrl was held during the click (`bCtrl`) and the multi selection is enabled, it's added to the selection or removed from it, and a click on nothing keeps the selection.
		// Otherwise, only the clicked entity is selected
		void OnViewportEntityClicked(Entity entity, bool bCtrl);

		const std::string& GetSceneHierarchyWindowName() const { return m_SceneHierarchyWindowName; }
		const std::string& GetPropertiesWindowName() const { return m_PropertiesWindowName; }

		SceneComponent* GetSelectedComponent()
		{
			if (!m_SelectedEntity)
				return nullptr;

			return m_Properties.GetSelectedComponent();
		}
		
		SelectedComponent GetSelectedComponentType() const { return m_Properties.GetSelectedComponentType(); }

		bool OnImGuiRender(const Ref<Scene>& scene, bool bScenePlaying, bool bAllowOnlySingleRoot = false, const bool* bVolumetricsEnabledOverride = nullptr);

		// The history that the changes made through the panel are recorded into. Can be null, they aren't recorded then
		void SetUndoHistory(UndoHistory* history) { m_UndoHistory = history; }

		// Must be called after a transaction of the history was undone or redone. Updates the selection:
		// if the selected entity no longer exists, it's deselected. If nothing is selected, the changed entity is selected (for example, a restored one)
		void OnUndoApplied(const UndoTransaction& transaction);

	private:
		// Called with the scene that's shown or whose event is handled. If it's another scene than the last time (or the last one was destroyed),
		// the state of the panel is of the previous scene, so it's reset
		void UpdateScene(const Ref<Scene>& scene);

		bool DrawSceneHierarchy(const Ref<Scene>& scene, float listHeight);
		bool DrawProperties(const Ref<Scene>& scene, bool bScenePlaying, const bool* bVolumetricsEnabledOverride);
		bool DrawEntityNode(const Ref<Scene>& scene, Entity entity, bool bFilteredOnly);
		bool DrawChilds(const Ref<Scene>& scene, Entity entity, bool bFilteredOnly);
		bool OnKeyPressed(const Ref<Scene>& scene, KeyPressedEvent& e, bool bViewportFocused);
		void DestroyEntity(const Ref<Scene>& scene, Entity entity, bool bDestroyChildren);
		void DestroySelectedEntities(const Ref<Scene>& scene, bool bDestroyChildren);
		void DuplicateSelectedEntities(const Ref<Scene>& scene);

		void CreateEmptyEntity(const Ref<Scene>& scene);
		void UpdateEntityRowHovered();

		void OnEntityRowClicked(Entity entity);
		bool ToggleSelection(Entity entity);
		void DeselectEntity(const Entity& entity);
		void RemoveDestroyedFromSelection();
		void SetActiveEntity(Entity entity);

		void AddSearchingEntity(const Entity& entity, std::unordered_set<Entity>& output);

		template <typename T>
		void GatherSearchingEntities(Scene* scene, const T& view, const std::string& search, std::unordered_set<Entity>& output)
		{
			for (auto& entt : view)
			{
				Entity entity = Entity(entt, scene);
				const std::string& name = entity.GetName();

				std::size_t pos = Utils::FindSubstringI(name, search);
				if (pos != std::string::npos)
				{
					AddSearchingEntity(entity, output);
				}
			}
		}

	private:
		EntityPropertiesPanel m_Properties;
		Weak<Scene> m_Scene; // The scene passed to the last `OnImGuiRender()` or `OnEvent()`
		UndoHistory* m_UndoHistory = nullptr;
		EntityEditTracker m_EditTracker;
		Entity m_SelectedEntity;
		std::vector<Entity> m_SelectedEntities;
		Entity m_RootEntity; // Only valid when `m_AllowOnlySingleRoot` is set to true
		Ref<Texture2D> m_RefreshIcon;

		std::string m_SceneHierarchyWindowName;
		std::string m_PropertiesWindowName;
		const ImGuiWindowClass* m_WindowClass = nullptr;
		std::string m_Search;
		std::unordered_set<Entity> m_AllowedForDisplayEntities; // Only valid when searching

		bool m_SceneHierarchyHovered = false;
		bool m_SceneHierarchyFocused = false;
		bool m_PropertiesHovered = false;
		bool m_bPropertiesInHierarchyWindow = false;
		float m_EntitiesListHeight = 0.f; // When the properties are in the hierarchy's window. 0 - the default height
		bool m_ScrollToSelected = false;
		bool m_bSelectedOutside = false; // `SetEntitySelected()` was called since the last `OnImGuiRender()` or `OnEvent()`, from outside of them
		bool m_AllowOnlySingleRoot = false;
		bool m_UpdateSearchResults = false;
		bool m_bMultiSelection = false;
		bool m_bEntityRowHovered = false; // In this frame, the mouse is over the row of an entity
	};
}
