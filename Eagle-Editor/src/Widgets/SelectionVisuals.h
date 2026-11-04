#pragma once

#include "Eagle/Core/GUID.h"
#include "Eagle/Renderer/RendererUtils.h"

namespace Eagle
{
	class Entity;
	class Scene;
	class SceneComponent;

	// Handles outline rendering of selected entities
	class SelectionOutline
	{
	public:
		void Update(const std::vector<Entity>& selectedEntities, const Ref<Scene>& scene, const OutlineSettings& settings, bool bEnabled);
		void Clear();

	private:
		Weak<Scene> m_Scene;
		std::vector<GUID> m_EntityGUIDs;
		std::vector<GUID> m_SelectedGUIDsTemp; // To avoid reallocations
		OutlineSettings m_Settings{};
	};
}

namespace Eagle::EditorUtils
{
	void DrawSelectedComponentHelpers(Scene& scene, const SceneComponent* component);
}
