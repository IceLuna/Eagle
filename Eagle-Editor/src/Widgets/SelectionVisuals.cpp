#include "egpch.h"
#include "SelectionVisuals.h"

#include "Eagle/Core/Scene.h"
#include "Eagle/Core/Entity.h"
#include "Eagle/Components/Components.h"

namespace Eagle
{
	void SelectionOutline::Update(const std::vector<Entity>& selectedEntities, const Ref<Scene>& scene, const OutlineSettings& settings, bool bEnabled)
	{
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
		m_EntityGUIDs = std::move(m_SelectedGUIDsTemp);
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
