#pragma once

#include "ViewportToolbar.h"
#include "../Undo/SceneUndo.h"

#include "Eagle/Core/Entity.h"
#include "Eagle/Math/Transform.h"

#include <glm/glm.hpp>

namespace Eagle
{
	class Scene;
	class SceneComponent;
	class UndoHistory;

	// Handles guizmo transformation of entities.
	// Also, a drag is recorded as one transaction of the undo history. It's committed when the drag ends
	class SelectionGuizmo
	{
	public:
		struct Selection
		{
			Entity Active; // The one selected last
			std::span<const Entity> Entities; // All the selected ones, including the active one
			SceneComponent* Component = nullptr; // The selected component of the active entity. It's transformed instead of the entity if only one entity is selected
		};

		struct Settings
		{
			int Operation = -1; // ImGuizmo::OPERATION: TRANSLATE, ROTATE, SCALE, or -1 (no gizmo)
			int Mode = 1;       // ImGuizmo::MODE: WORLD or LOCAL
			GuizmoPivot Pivot = GuizmoPivot::ActiveEntity;
			GuizmoGroupTransform GroupTransform = GuizmoGroupTransform::AroundPivot;
			const SnapSettings* Snap = nullptr;
		};

		struct Viewport
		{
			glm::vec2 Min = glm::vec2(0.f); // The bounds of the viewport's image, in screen space
			glm::vec2 Max = glm::vec2(0.f);
			glm::mat4 View = glm::mat4(1.f);
			glm::mat4 Projection = glm::mat4(1.f); // Not reversed, and not flipped (ImGuizmo expects Y up)
		};

		// Draws the gizmo into the current window.
		// Returns true if the selection was transformed
		bool Update(const Selection& selection, const Ref<Scene>& scene, const Viewport& viewport, const Settings& settings,
			UndoHistory* history, const Ref<Scene>& transactionScene = nullptr);

		// Must be called every frame, after `Update()` even if `Update()` wasn't called.
		// The drag has ended if the gizmo wasn't dragged or drawn this frame, so its undo transaction is committed
		void EndFrame();

		void Reset();

		bool IsUsingOrHovered() const { return m_bUsingOrHovered; }

	private:
		// Returns true if the selection differs from the one of the last `Update()`
		bool UpdateLastSelection(const Selection& selection);

		bool UpdateSingle(const Selection& selection, const Viewport& viewport, const Settings& settings, const float* snap,
			UndoHistory* history, const Ref<Scene>& transactionScene);
		bool UpdateGroup(const Selection& selection, const Ref<Scene>& scene, const Viewport& viewport, const Settings& settings, const float* snap,
			UndoHistory* history, const Ref<Scene>& transactionScene);

	private:
		struct GroupDrag
		{
			GroupDrag() : Pivot(1.f) {}

			Transform StartPivot;
			glm::mat4 Pivot; // As dragged
			std::vector<std::pair<Entity, Transform>> Entities; // With their world transforms when the drag started
		};

		std::optional<SceneTransaction> m_Transaction; // Open while the gizmo is dragged
		std::optional<GroupDrag> m_GroupDrag;

		// The selection of the last `Update()`
		Entity m_LastActive;
		std::vector<Entity> m_LastEntities;
		const SceneComponent* m_LastComponent = nullptr;

		bool m_bUsed = false;
		bool m_bUsingOrHovered = false;
	};
}

namespace Eagle::GuizmoGroup
{
	// Where the gizmo is: either at the active entity, or at the center of the bounds of the entities locations.
	// Its rotation is the active entity's (the axes of the local space)
	Transform GetPivot(const std::vector<Transform>& worldTransforms, const Transform& activeWorldTransform, GuizmoPivot pivot);

	// The world transform of an entity after the gizmo was moved from `startPivot` to `pivot`. `start` is its world transform before.
	Transform Apply(const Transform& start, const Transform& startPivot, const Transform& pivot, int guizmoOperation, int guizmoMode, GuizmoGroupTransform groupTransform);
}
