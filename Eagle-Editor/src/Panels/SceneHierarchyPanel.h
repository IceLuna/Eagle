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
		void DrawComponent(const std::string& name, Entity entity, std::function<void()> function)
		{
			if (entity.HasComponent<T>())
			{
				if (ImGui::TreeNodeEx((void*)typeid(T).hash_code(), ImGuiTreeNodeFlags_DefaultOpen, name.c_str()))
				{
					function();

					ImGui::TreePop();
				}
			}
		}

		template <typename T>
		void DrawComponent(const std::string& name, std::function<void()> function)
		{
			if (ImGui::TreeNodeEx((void*)typeid(T).hash_code(), ImGuiTreeNodeFlags_DefaultOpen, name.c_str()))
			{
				function();

				ImGui::TreePop();
			}
		}

		void DrawTransformNode(SceneComponent& sceneComponent);

	private:
		Ref<Scene> m_Context;
		Entity m_SelectedEntity;
	};
}