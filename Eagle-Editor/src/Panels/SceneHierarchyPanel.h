#pragma once

#include "Eagle.h"
#include "EntityPropertiesPanel.h"

#include <functional>

namespace Eagle
{
	class EditorLayer;
	class Material;

	class SceneHierarchyPanel
	{
	public:
		SceneHierarchyPanel() = default;
		SceneHierarchyPanel(const Ref<Scene>& scene);

		// @uniqueID. Can be used to 
		void SetContext(const Ref<Scene>& scene, uint64_t uniqueID = 0);
		void ClearSelection();

		void OnEvent(Event& e, bool bViewportFocused);

		Entity GetSelectedEntity() const { return m_SelectedEntity; }
		void SetEntitySelected(Entity entity, SelectedComponent component = SelectedComponent::None);

		const std::string& GetSceneHierarchyWindowName() const { return m_SceneHierarchyWindowName; }
		const std::string& GetPropertiesWindowName() const { return m_PropertiesWindowName; }

		SceneComponent* GetSelectedComponent()
		{
			if (!m_SelectedEntity)
				return nullptr;

			return m_Properties.GetSelectedComponent();
		}
		
		SelectedComponent GetSelectedComponentType() const { return m_Properties.GetSelectedComponentType(); }

		bool OnImGuiRender(bool bScenePlaying, bool bAllowOnlySingleRoot = false, const bool* bVolumetricsEnabledOverride = nullptr);

	private:
		bool DrawSceneHierarchy();
		bool DrawEntityNode(Entity& entity);
		bool DrawChilds(Entity& entity);

	private:
		EntityPropertiesPanel m_Properties;
		Ref<Scene> m_Scene;
		Entity m_SelectedEntity;
		Entity m_RootEntity; // Only valid when `m_AllowOnlySingleRoot` is set to true

		std::string m_SceneHierarchyWindowName;
		std::string m_PropertiesWindowName;

		bool m_SceneHierarchyHovered = false;
		bool m_SceneHierarchyFocused = false;
		bool m_PropertiesHovered = false;
		bool m_ScrollToSelected = false;
		bool m_AllowOnlySingleRoot = false;
	};
}
