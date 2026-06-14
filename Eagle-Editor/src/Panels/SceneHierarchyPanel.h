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

		bool OnEvent(Event& e, bool bViewportFocused);

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
		bool DrawEntityNode(Entity entity, bool bFilteredOnly);
		bool DrawChilds(Entity entity, bool bFilteredOnly);
		bool OnKeyPressed(KeyPressedEvent& e, bool bViewportFocused);

		void AddSearchingEntity(const Entity& entity, std::unordered_set<Entity>& output);

		template <typename T>
		void GatherSearchingEntities(const T& view, const std::string& search, std::unordered_set<Entity>& output)
		{
			for (auto& entt : view)
			{
				Entity entity = Entity(entt, m_Scene.get());
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
		Ref<Scene> m_Scene;
		Entity m_SelectedEntity;
		Entity m_RootEntity; // Only valid when `m_AllowOnlySingleRoot` is set to true

		std::string m_SceneHierarchyWindowName;
		std::string m_PropertiesWindowName;
		std::string m_Search;
		std::unordered_set<Entity> m_AllowedForDisplayEntities; // Only valid when searching

		bool m_SceneHierarchyHovered = false;
		bool m_SceneHierarchyFocused = false;
		bool m_PropertiesHovered = false;
		bool m_ScrollToSelected = false;
		bool m_AllowOnlySingleRoot = false;
	};
}
