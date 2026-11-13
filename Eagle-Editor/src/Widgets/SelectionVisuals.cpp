#include "egpch.h"
#include "SelectionVisuals.h"

#include "Eagle/Core/Scene.h"
#include "Eagle/Core/Entity.h"
#include "Eagle/Components/Components.h"

namespace Eagle
{
	static constexpr uint64_t s_MaxWaitFrames = 60;

	void SelectionOutline::Update(const std::vector<Entity>& selectedEntities, const Ref<Scene>& scene, const OutlineSettings& settings, bool bEnabled)
	{
		m_SelectedGUIDsTemp.clear();
		if (bEnabled && scene)
		{
			m_SelectedGUIDsTemp.reserve(selectedEntities.size());
			for (const Entity& selectedEntity : selectedEntities)
			{
				if (selectedEntity.IsValid() && selectedEntity.GetScene() == scene.get())
					m_SelectedGUIDsTemp.push_back(selectedEntity.GetGUID());
			}
		}

		const Ref<Scene> appliedScene = m_Scene.lock();
		const bool bUpToDate = appliedScene == scene && m_EntityGUIDs == m_SelectedGUIDsTemp && m_Settings == settings;
		if (bUpToDate)
			return;

		Clear();

		if (scene)
		{
			for (const GUID& guid : m_SelectedGUIDsTemp)
			{
				if (Entity entity = scene->GetEntityByGUID(guid))
					scene->SetEntityOutline(entity, settings, OutlineLayer::Editor);
			}
		}

		m_Scene = scene;
		m_EntityGUIDs.swap(m_SelectedGUIDsTemp);
		m_Settings = settings;
	}

	void SelectionOutline::Clear()
	{
		if (const Ref<Scene> appliedScene = m_Scene.lock())
		{
			for (const GUID& guid : m_EntityGUIDs)
			{
				if (Entity entity = appliedScene->GetEntityByGUID(guid))
					appliedScene->RemoveEntityOutline(entity, OutlineLayer::Editor);
			}
		}

		m_Scene.reset();
		m_EntityGUIDs.clear();
	}

	void ViewportClickSelection::Update(const Ref<Scene>& scene, const glm::vec2& viewportMin, const glm::vec2& viewportMax, bool bCanClick,
		const std::function<void(Entity, bool)>& onClicked)
	{
		const uint64_t frame = RenderManager::GetFrameNumber_CPU();
		if (bCanClick && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
		{
			// Where the click happened, not where the mouse is now
			const ImGuiIO& io = ImGui::GetIO();
			const glm::vec2 pos = glm::vec2(io.MouseClickedPos[ImGuiMouseButton_Left].x, io.MouseClickedPos[ImGuiMouseButton_Left].y);

			if (pos.x >= viewportMin.x && pos.y >= viewportMin.y && pos.x < viewportMax.x && pos.y < viewportMax.y)
				m_Click = ClickData{ scene, glm::ivec2(pos - viewportMin), frame, io.KeyCtrl };
		}

		if (!m_Click)
			return;

		const Ref<Scene> clickedScene = m_Click->ClickedScene.lock();
		if (clickedScene != scene || !scene || !scene->GetSceneRenderer() || (frame - m_Click->Frame > s_MaxWaitFrames))
		{
			m_Click.reset();
			return;
		}

		int32_t objectID = -1;
		if (!scene->GetSceneRenderer()->GetObjectIDAtSince(m_Click->Coord, m_Click->Frame, objectID))
			return;

		const bool bCtrl = m_Click->bCtrl;
		m_Click.reset();

		Entity entity = objectID >= 0 ? Entity{ (entt::entity)objectID, scene.get() } : Entity::Null;
		if (!entity.IsValid())
			entity = Entity::Null;

		onClicked(entity, bCtrl);
	}
}

namespace Eagle::EditorUtils
{
	void DrawSelectedComponentHelpers(Scene& scene, const SceneComponent* component)
	{
		if (const DecalComponent* decal = dynamic_cast<const DecalComponent*>(component))
		{
			const AABB aabb(glm::vec3(-0.5f), glm::vec3(0.5f));
			scene.DrawBox(aabb, decal->GetWorldTransform());
			const auto& start = decal->GetWorldTransform().Location;
			scene.DrawArrow(start, start + decal->GetForwardVector() * 0.25f, decal->GetUpVector());
		}
		else if (const CameraComponent* camera = dynamic_cast<const CameraComponent*>(component))
		{
			scene.DrawFrustum(*camera);
		}
	}
}
