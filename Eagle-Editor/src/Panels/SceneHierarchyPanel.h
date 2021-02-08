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
		void DrawComponents(Entity entity);

		template <typename T>
		void DrawComponent(const std::string& name, Entity entity, std::function<void(T&)> function, bool canRemove = true)
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


		void DrawTransformNode(Entity entity, SceneComponent& sceneComponent);

	private:
		Ref<Scene> m_Context;
		Entity m_SelectedEntity;
	};
}