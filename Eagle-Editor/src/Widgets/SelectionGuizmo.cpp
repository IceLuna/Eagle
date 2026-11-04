#include "egpch.h"
#include "SelectionGuizmo.h"
#include "../Undo/UndoUI.h"

#include "Eagle/Core/Scene.h"
#include "Eagle/Components/Components.h"
#include "Eagle/Math/Math.h"

#include <ImGuizmo.h>
#include <glm/gtc/type_ptr.hpp>

namespace Eagle
{
	bool SelectionGuizmo::UpdateLastSelection(const Selection& selection)
	{
		const bool bChanged = selection.Active != m_LastActive || selection.Component != m_LastComponent
			|| !std::equal(selection.Entities.begin(), selection.Entities.end(), m_LastEntities.begin(), m_LastEntities.end());
		if (bChanged)
		{
			m_LastActive = selection.Active;
			m_LastEntities.assign(selection.Entities.begin(), selection.Entities.end());
			m_LastComponent = selection.Component;
		}
		return bChanged;
	}

	bool SelectionGuizmo::Update(const Selection& selection, const Ref<Scene>& scene, const Viewport& viewport, const Settings& settings,
		UndoHistory* history, const Ref<Scene>& transactionScene)
	{
		// The gizmo moves to the new selection.
		// When selection changes, guizmo might appear right under the mouse, which immediately starts a drag as if the user clicked the guizmo.
		// To fix it, skipping the guizmo for this frame
		const bool bSelectionChanged = UpdateLastSelection(selection);
		if (bSelectionChanged || !scene || !selection.Active || settings.Operation == -1)
		{
			m_bUsingOrHovered = false;
			return false;
		}

		ImGuizmo::PushID(this);
		ImGuizmo::SetDrawlist();
		ImGuizmo::SetRect(viewport.Min.x, viewport.Min.y, viewport.Max.x - viewport.Min.x, viewport.Max.y - viewport.Min.y);

		// Snapping
		const bool bSnap = settings.Snap && settings.Snap->IsSnapping(settings.Operation);
		const float snapValue = settings.Snap ? settings.Snap->GetValue(settings.Operation) : 0.f;
		const float snapValues[3] = { snapValue, snapValue, snapValue };
		const float* snap = bSnap ? snapValues : nullptr;

		const Ref<Scene>& recordedScene = transactionScene ? transactionScene : scene;
		const bool bChanged = selection.Entities.size() > 1 ? UpdateGroup(selection, scene, viewport, settings, snap, history, recordedScene)
			: UpdateSingle(selection, viewport, settings, snap, history, recordedScene);

		ImGuizmo::PopID();
		return bChanged;
	}

	bool SelectionGuizmo::UpdateSingle(const Selection& selection, const Viewport& viewport, const Settings& settings, const float* snap,
		UndoHistory* history, const Ref<Scene>& transactionScene)
	{
		Entity selectedEntity = selection.Active;
		SceneComponent* selectedComponent = selection.Component;
		const int operation = settings.Operation;

		Transform transform;
		bool bRelative = false; // Only used for rotations
		if (selectedComponent)
		{
			transform = selectedComponent->GetWorldTransform();
			bRelative = operation == ImGuizmo::OPERATION::ROTATE;
		}
		else
		{
			transform = selectedEntity.GetWorldTransform();
			bRelative = selectedEntity.HasParent() && (operation == ImGuizmo::OPERATION::ROTATE);
		}
		Transform finalTransform = transform;

		// If relative, we only get relative rotation, since other params need to be in world coords.
		// Otherwise, for example, guizmo will be renderer in the position if we used relative location
		if (bRelative)
			transform.Rotation = (selectedComponent ? selectedComponent->GetRelativeTransform() : selectedEntity.GetRelativeTransform()).Rotation;

		glm::mat4 transformMatrix = Math::ToTransformMatrix(transform);
		ImGuizmo::Manipulate(glm::value_ptr(viewport.View), glm::value_ptr(viewport.Projection), (ImGuizmo::OPERATION)operation,
			(ImGuizmo::MODE)settings.Mode, glm::value_ptr(transformMatrix), nullptr, snap);

		m_bUsingOrHovered = ImGuizmo::IsUsing() || ImGuizmo::IsOver();
		if (!ImGuizmo::IsUsing())
			return false;

		const Transform dragged = Math::DecomposeTransformMatrix(transformMatrix);
		if (operation == ImGuizmo::OPERATION::TRANSLATE)
			finalTransform.Location = dragged.Location;
		else if (operation == ImGuizmo::OPERATION::ROTATE)
		{
			if (bRelative)
				finalTransform = selectedComponent ? selectedComponent->GetRelativeTransform() : selectedEntity.GetRelativeTransform();
			finalTransform.Rotation = dragged.Rotation;
		}
		else if (operation == ImGuizmo::OPERATION::SCALE)
			finalTransform.Scale3D = dragged.Scale3D;

		// The whole drag is one transaction. Its state is captured before the first change
		m_bUsed = true;
		if (!m_Transaction && history)
		{
			m_Transaction.emplace(history, transactionScene, UndoUI::GetGuizmoTransactionName(operation, selectedComponent != nullptr));
			m_Transaction->Track(selectedEntity);
		}

		if (selectedComponent)
			bRelative ? selectedComponent->SetRelativeTransform(finalTransform) : selectedComponent->SetWorldTransform(finalTransform);
		else
			bRelative ? selectedEntity.SetRelativeTransform(finalTransform) : selectedEntity.SetWorldTransform(finalTransform);

		return true;
	}

	bool SelectionGuizmo::UpdateGroup(const Selection& selection, const Ref<Scene>& scene, const Viewport& viewport, const Settings& settings, const float* snap,
		UndoHistory* history, const Ref<Scene>& transactionScene)
	{
		const Entity activeEntity = selection.Active;

		std::vector<Entity> entities;
		for (const Entity& entity : selection.Entities)
		{
			if (entity.IsValid() && entity.GetScene() == scene.get() && !scene->IsPendingDestroy(entity))
				entities.push_back(entity);
		}
		entities = Entity::GetTopmostEntities(entities);

		if (entities.empty() || !activeEntity.IsValid() || activeEntity.GetScene() != scene.get())
		{
			m_bUsingOrHovered = false;
			m_GroupDrag.reset();
			return false;
		}

		Transform pivot;
		glm::mat4 pivotMatrix;
		if (m_GroupDrag)
		{
			pivotMatrix = m_GroupDrag->Pivot;
		}
		else
		{
			std::vector<Transform> worldTransforms;
			worldTransforms.reserve(entities.size());
			for (const Entity& entity : entities)
				worldTransforms.push_back(entity.GetWorldTransform());

			pivot = GuizmoGroup::GetPivot(worldTransforms, activeEntity.GetWorldTransform(), settings.Pivot);
			pivotMatrix = Math::ToTransformMatrix(pivot);
		}

		ImGuizmo::Manipulate(glm::value_ptr(viewport.View), glm::value_ptr(viewport.Projection), (ImGuizmo::OPERATION)settings.Operation,
			(ImGuizmo::MODE)settings.Mode, glm::value_ptr(pivotMatrix), nullptr, snap);

		m_bUsingOrHovered = ImGuizmo::IsUsing() || ImGuizmo::IsOver();
		if (!ImGuizmo::IsUsing())
		{
			m_GroupDrag.reset();
			return false;
		}

		if (!m_GroupDrag)
		{
			// The drag starts. The entities are moved from where they are now
			GroupDrag& drag = m_GroupDrag.emplace();
			drag.StartPivot = pivot;
			drag.Entities.reserve(entities.size());
			for (const Entity& entity : entities)
				drag.Entities.emplace_back(entity, entity.GetWorldTransform());

			// The whole drag is one transaction. Its state is captured before the first change
			if (!m_Transaction && history)
			{
				m_Transaction.emplace(history, transactionScene, UndoUI::GetGuizmoTransactionName(settings.Operation, false, entities.size()));
				for (const Entity& entity : entities)
					m_Transaction->Track(entity);
			}
		}

		GroupDrag& drag = *m_GroupDrag;
		drag.Pivot = pivotMatrix;
		m_bUsed = true;

		const Transform draggedPivot = Math::DecomposeTransformMatrix(pivotMatrix);
		for (auto& [entity, startTransform] : drag.Entities)
		{
			if (entity.IsValid())
				entity.SetWorldTransform(GuizmoGroup::Apply(startTransform, drag.StartPivot, draggedPivot, settings.Operation, settings.Mode, settings.GroupTransform));
		}

		return true;
	}

	void SelectionGuizmo::EndFrame()
	{
		// The drag has ended
		if (!m_bUsed)
		{
			if (m_Transaction)
			{
				m_Transaction->Commit();
				m_Transaction.reset();
			}
			m_GroupDrag.reset();
		}

		m_bUsed = false;
	}

	void SelectionGuizmo::Reset()
	{
		if (m_Transaction)
		{
			m_Transaction->Cancel();
			m_Transaction.reset();
		}
		m_GroupDrag.reset();
		m_bUsed = false;
		m_bUsingOrHovered = false;
	}
}

namespace Eagle::GuizmoGroup
{
	Transform GetPivot(const std::vector<Transform>& worldTransforms, const Transform& activeWorldTransform, GuizmoPivot pivot)
	{
		Transform result(activeWorldTransform.Location, activeWorldTransform.Rotation);

		if (pivot == GuizmoPivot::SelectionCenter && !worldTransforms.empty())
		{
			glm::vec3 min = worldTransforms[0].Location;
			glm::vec3 max = min;
			for (const Transform& transform : worldTransforms)
			{
				min = glm::min(min, transform.Location);
				max = glm::max(max, transform.Location);
			}
			result.Location = (min + max) * 0.5f;
		}

		return result;
	}

	Transform Apply(const Transform& start, const Transform& startPivot, const Transform& pivot, int guizmoOperation, int guizmoMode, GuizmoGroupTransform groupTransform)
	{
		Transform result = start;
		const bool bAroundPivot = groupTransform == GuizmoGroupTransform::AroundPivot;
		const glm::quat& startPivotRotation = startPivot.Rotation.GetQuat();

		if (guizmoOperation == ImGuizmo::OPERATION::TRANSLATE)
		{
			result.Location = start.Location + (pivot.Location - startPivot.Location);
		}
		else if (guizmoOperation == ImGuizmo::OPERATION::ROTATE)
		{
			// The rotation of the gizmo, in the world space
			const glm::quat delta = glm::normalize(pivot.Rotation.GetQuat() * glm::inverse(startPivotRotation));

			if (bAroundPivot)
			{
				result.Location = startPivot.Location + delta * (start.Location - startPivot.Location);
				result.Rotation = glm::normalize(delta * start.Rotation.GetQuat());
			}
			else if (guizmoMode == ImGuizmo::MODE::LOCAL)
			{
				// Each entity turns around its own axis that's the gizmo's axis in the gizmo's space
				const glm::quat localDelta = glm::normalize(glm::inverse(startPivotRotation) * pivot.Rotation.GetQuat());
				result.Rotation = glm::normalize(start.Rotation.GetQuat() * localDelta);
			}
			else
			{
				result.Rotation = glm::normalize(delta * start.Rotation.GetQuat());
			}
		}
		else if (guizmoOperation == ImGuizmo::OPERATION::SCALE)
		{
			// Along the gizmo's axes
			const glm::vec3 factor = pivot.Scale3D / startPivot.Scale3D;
			result.Scale3D = start.Scale3D * factor;

			if (bAroundPivot)
			{
				// The distances to the pivot are scaled too
				const glm::vec3 offset = glm::inverse(startPivotRotation) * (start.Location - startPivot.Location);
				result.Location = startPivot.Location + startPivotRotation * (offset * factor);
			}
		}

		return result;
	}
}
