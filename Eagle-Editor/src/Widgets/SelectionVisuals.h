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

	// Responsibe for handling entity selection in a viewport
	class ViewportClickSelection
	{
	public:
		// Should be called every frame from the viewport's ImGui window
		void Update(const Ref<Scene>& scene, const glm::vec2& viewportMin, const glm::vec2& viewportMax, bool bCanClick, const std::function<void(Entity, bool)>& onClicked);

	private:
		struct ClickData
		{
			Weak<Scene> ClickedScene;
			glm::ivec2 Coord;
			uint64_t Frame;
			bool bCtrl;
		};
		std::optional<ClickData> m_Click;
	};
}

namespace Eagle::EditorUtils
{
	void DrawSelectedComponentHelpers(Scene& scene, const SceneComponent* component);
}
