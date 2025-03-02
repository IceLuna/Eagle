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
		SceneHierarchyPanel(const EditorLayer& editor);
		SceneHierarchyPanel(const EditorLayer& editor, const Ref<Scene>& scene);

		void SetContext(const Ref<Scene>& scene);
		void ClearSelection();

		void OnEvent(Event& e);

		Entity GetSelectedEntity() const { return m_SelectedEntity; }
		void SetEntitySelected(Entity entity, SelectedComponent component = SelectedComponent::None);

		SceneComponent* GetSelectedComponent()
		{
			if (!m_SelectedEntity)
				return nullptr;

			return m_Properties.GetSelectedComponent();
		}
		
		SelectedComponent GetSelectedComponentType() const { return m_Properties.GetSelectedComponentType(); }

		bool OnImGuiRender();

	private:
		bool DrawSceneHierarchy();
		bool DrawEntityNode(Entity& entity);
		bool DrawChilds(Entity& entity);

	private:
		const EditorLayer& m_Editor;
		EntityPropertiesPanel m_Properties;
		Ref<Scene> m_Scene;
		Entity m_SelectedEntity;
		bool m_SceneHierarchyHovered = false;
		bool m_SceneHierarchyFocused = false;
		bool m_PropertiesHovered = false;
		bool m_ScrollToSelected = false;
	};
}
