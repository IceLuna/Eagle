#pragma once

#include "Eagle.h"
#include <functional>

namespace Eagle
{
	class EditorLayer;

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
			switch (m_SelectedComponent)
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
				case SelectedComponent::RigidBody: return &m_SelectedEntity.GetComponent<RigidBodyComponent>();
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
		void DrawEntityNode(Entity& entity);
		void DrawComponents(Entity& entity);

		template <typename T, typename UIFunction>
		void DrawComponent(const std::string& name, Entity& entity, UIFunction function, bool canRemove = true)
		{
			if (entity.HasComponent<T>())
			{
				ImGui::PushID(int(typeid(T).hash_code()));

				const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth
												| ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_AllowItemOverlap;

				ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();
													
				ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{4, 4});
				float lineHeight = (GImGui->Font->FontSize * GImGui->Font->Scale)+ GImGui->Style.FramePadding.y * 2.f;
				ImGui::Separator();
				bool treeOpened = ImGui::TreeNodeEx((void*)typeid(T).hash_code(), flags, name.c_str());
				
				ImGui::PopStyleVar();

				bool bRemoveComponent = false;
				
				if (canRemove)
				{
					ImGui::SameLine(contentRegionAvailable.x - lineHeight * 0.5f);
					if (ImGui::Button("...", ImVec2{ lineHeight, lineHeight })) //•••
					{
						ImGui::OpenPopup("ComponentSettings");
					}

					if (ImGui::BeginPopup("ComponentSettings"))
					{
						if (ImGui::MenuItem("Remove Component"))
						{
							bRemoveComponent = true;
							m_SelectedComponent = SelectedComponent::None;
						}
						ImGui::EndPopup();
					}
				}

				ImGui::PopID();

				if (treeOpened)
				{
					T& component = entity.GetComponent<T>();
					function(component);

					ImGui::TreePop();
				}

				if (bRemoveComponent)
					entity.RemoveComponent<T>();
			}
		}

		template <typename T>
		bool DrawComponentLine(const std::string& name, Entity& entity, bool selected)
		{
			bool bClicked = false;
			if (entity.HasComponent<T>())
			{
				ImGuiTreeNodeFlags childFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_Leaf 
												| ImGuiTreeNodeFlags_SpanAvailWidth | (selected ? ImGuiTreeNodeFlags_Selected : 0);
				bool treeOpened = ImGui::TreeNodeEx((void*)(typeid(T).hash_code() + typeid(Entity).hash_code()), childFlags, name.c_str());
				
				bClicked = ImGui::IsItemClicked();
				
				if (treeOpened)
				{
					ImGui::TreePop();
				}
			}
			return bClicked;
		}

		template <typename T>
		void DrawAddComponentMenuItem(const std::string& name)
		{
			if (!m_SelectedEntity) return;

			if (m_SelectedEntity.HasComponent<T>() == false)
			{
				if (ImGui::MenuItem(name.c_str()))
				{
					m_SelectedEntity.AddComponent<T>();
#ifdef EG_WITH_EDITOR
					std::string componentName = typeid(T).name();
					const size_t pos = componentName.find_last_of("::");
					if (pos != std::string::npos)
						componentName = componentName.substr(pos + 1);
					EG_EDITOR_TRACE("Add '{}' to {}", componentName, m_SelectedEntity.GetSceneName());
#endif
					ImGui::CloseCurrentPopup();
				}
			}
		}
		
		void DrawSceneHierarchy();

		void DrawComponentTransformNode(Entity& entity, SceneComponent& sceneComponent);

		void DrawEntityTransformNode(Entity& entity);

		void DrawChilds(Entity& entity);

	private:
		enum class SelectedComponent
		{
			None,
			Sprite,
			StaticMesh,
			Billboard,
			Text3D,
			Camera,
			PointLight,
			DirectionalLight,
			SpotLight,
			Script,
			RigidBody,
			BoxCollider,
			SphereCollider,
			CapsuleCollider,
			MeshCollider,
			AudioComponent,
			ReverbComponent
		};

	private:
		std::unordered_map<Entity, bool> m_InvertEntityRotation;
		std::unordered_map<Entity, bool> m_InvertComponentRotation;
		const EditorLayer& m_Editor;
		Ref<Scene> m_Scene;
		SelectedComponent m_SelectedComponent = SelectedComponent::None;
		Entity m_SelectedEntity;
		bool m_SceneHierarchyHovered = false;
		bool m_SceneHierarchyFocused = false;
		bool m_PropertiesHovered = false;
		bool m_ScrollToSelected = false;
	};
}
