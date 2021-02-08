#pragma once

#include "Eagle.h"
#include <functional>

namespace Eagle
{
	class SceneHierarchyPanel
	{
	public:
		SceneHierarchyPanel() = default;
		SceneHierarchyPanel(const Ref<Scene>& context);

		void SetContext(const Ref<Scene>& context);

		void OnImGuiRender();

	private:
		void DrawEntityNode(Entity entity);
		void DrawProperties(Entity entity);

		template <typename T>
		void DrawComponent(const std::string& name, Entity entity, std::function<void()> function, bool canRemove = true)
		{
			if (entity.HasComponent<T>())
			{
				ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{4, 4});
				ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowItemOverlap;
				bool treeOpened = ImGui::TreeNodeEx((void*)typeid(T).hash_code(), ImGuiTreeNodeFlags_DefaultOpen, name.c_str());
				
				if (canRemove)
				{
					ImGui::SameLine(ImGui::GetWindowWidth() - 50.f);
					if (ImGui::Button("...", ImVec2{30.f, 20.f}))
					{
						ImGui::OpenPopup("ComponentSettings");
					}
				}
				ImGui::PopStyleVar();

				if (treeOpened)
				{
					function();

					ImGui::TreePop();
				}

				if (canRemove)
				{
					if (ImGui::BeginPopup("ComponentSettings"))
					{
						if (ImGui::MenuItem("Remove Component"))
						{
							entity.RemoveComponent<T>();
						}
						ImGui::EndPopup();
					}
				}
			}
		}


		void DrawTransformNode(Entity entity, SceneComponent& sceneComponent);

	private:
		Ref<Scene> m_Context;
		Entity m_SelectedEntity;
	};
}