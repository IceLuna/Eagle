#pragma once

#include "Eagle.h"

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

		void DrawTransformNode(Transform& transform);

	private:
		Ref<Scene> m_Context;
		Entity m_SelectedEntity;
	};
}