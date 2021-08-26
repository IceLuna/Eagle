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
				case SelectedComponent::None:
					return nullptr;

				case SelectedComponent::Sprite:
				{
					SpriteComponent& sprite = m_SelectedEntity.GetComponent<SpriteComponent>();
					return &sprite;
				}

				case SelectedComponent::StaticMesh:
				{
					StaticMeshComponent& sm = m_SelectedEntity.GetComponent<StaticMeshComponent>();
					return &sm;
				}

				case SelectedComponent::Camera:
				{
					CameraComponent& camera = m_SelectedEntity.GetComponent<CameraComponent>();
					return &camera;
				}

				case SelectedComponent::PointLight:
				{
					PointLightComponent& light = m_SelectedEntity.GetComponent<PointLightComponent>();
					return &light;
				}

				case SelectedComponent::DirectionalLight:
				{
					DirectionalLightComponent& light = m_SelectedEntity.GetComponent<DirectionalLightComponent>();
					return &light;
				}

				case SelectedComponent::SpotLight:
				{
					SpotLightComponent& light = m_SelectedEntity.GetComponent<SpotLightComponent>();
					return &light;
				}
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
				const std::string typeName(typeid(T).name());
				const std::string imguiPopupID = std::string("ComponentSettings") + typeName;
				ImGui::PushID(imguiPopupID.c_str());

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
			Camera,
			PointLight,
			DirectionalLight,
			SpotLight,
			Script
		};

	private:
		const EditorLayer& m_Editor;
		Ref<Scene> m_Scene;
		SelectedComponent m_SelectedComponent = SelectedComponent::None;
		Entity m_SelectedEntity;
		bool m_SceneHierarchyHovered = false;
		bool m_PropertiesHovered = false;
	};
}
