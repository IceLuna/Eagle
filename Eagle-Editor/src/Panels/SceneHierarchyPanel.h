#pragma once

#include "Eagle.h"
#include <functional>

namespace Eagle
{
	class SceneHierarchyPanel
	{
	public:
		SceneHierarchyPanel() = default;
		SceneHierarchyPanel(const Ref<Scene>& scene);

		void SetContext(const Ref<Scene>& scene);
		void ClearSelection();

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

				case SelectedComponent::Camera:
				{
					CameraComponent& camera = m_SelectedEntity.GetComponent<CameraComponent>();
					return &camera;
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
				float lineHeight = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.f;
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
					auto& component = entity.GetComponent<T>();
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
				ImGuiTreeNodeFlags childFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | (selected ? ImGuiTreeNodeFlags_Selected : 0);
				bool treeOpened = ImGui::TreeNodeEx((void*)(typeid(T).hash_code() + typeid(Entity).hash_code()), childFlags, name.c_str());
				
				bClicked = ImGui::IsItemClicked();
				
				if (treeOpened)
				{
					ImGui::TreePop();
				}
			}
			return bClicked;
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
			Camera
		};

	private:
		SelectedComponent m_SelectedComponent = SelectedComponent::None;
		Ref<Scene> m_Scene;
		Entity m_SelectedEntity;
	};
}
