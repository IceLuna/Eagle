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
		void SetEntitySelected(int entityID);

		SceneComponent* GetSelectedComponent()
		{
			if (!m_SelectedEntity)
				return nullptr;

			switch (m_Properties.GetSelectedComponent())
			{
				case SelectedComponent::None: return nullptr;
				case SelectedComponent::Sprite: return &m_SelectedEntity.GetComponent<SpriteComponent>();
				case SelectedComponent::StaticMesh: return &m_SelectedEntity.GetComponent<StaticMeshComponent>();
				case SelectedComponent::Billboard: return &m_SelectedEntity.GetComponent<BillboardComponent>();
				case SelectedComponent::Text3D: return &m_SelectedEntity.GetComponent<TextComponent>();
				case SelectedComponent::Camera: return &m_SelectedEntity.GetComponent<CameraComponent>();
				case SelectedComponent::PointLight: return &m_SelectedEntity.GetComponent<PointLightComponent>();
				case SelectedComponent::DirectionalLight: return &m_SelectedEntity.GetComponent<DirectionalLightComponent>();
				case SelectedComponent::SpotLight: return &m_SelectedEntity.GetComponent<SpotLightComponent>();
				case SelectedComponent::BoxCollider: return &m_SelectedEntity.GetComponent<BoxColliderComponent>();
				case SelectedComponent::SphereCollider: return &m_SelectedEntity.GetComponent<SphereColliderComponent>();
				case SelectedComponent::CapsuleCollider: return &m_SelectedEntity.GetComponent<CapsuleColliderComponent>();
				case SelectedComponent::MeshCollider: return &m_SelectedEntity.GetComponent<MeshColliderComponent>();
				case SelectedComponent::AudioComponent: return &m_SelectedEntity.GetComponent<AudioComponent>();
				case SelectedComponent::ReverbComponent: return &m_SelectedEntity.GetComponent<ReverbComponent>();
			}
			return nullptr;
		}

		void OnImGuiRender();

	private:
		void DrawSceneHierarchy();
		void DrawEntityNode(Entity& entity);
		void DrawChilds(Entity& entity);

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
