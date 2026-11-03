#include "SceneUndo.h"

#include "Eagle/Core/Scene.h"
#include "Eagle/Core/Serializer.h"
#include "Eagle/Components/Components.h"

#include <yaml-cpp/yaml.h>

namespace Eagle
{
	namespace
	{
		bool IsDestroyed(const Scene& scene, const Entity& entity)
		{
			return !entity || scene.IsPendingDestroy(entity);
		}

		std::string GetComponentDisplayName(std::string_view key)
		{
			if (key == "ScriptComponent")
				return "C# Script";

			return Utils::ToDisplayName(key);
		}

		bool IsMetadataKey(std::string_view key)
		{
			return key == "CollisionGroupGUIDs";
		}

		bool NodesEqual(const YAML::Node& a, const YAML::Node& b)
		{
			if (!a || !b)
				return bool(a) == bool(b);
			return YAML::Dump(a) == YAML::Dump(b);
		}

		// Keys of the maps whose values differ
		std::vector<std::string> GetDifferingKeys(const YAML::Node& before, const YAML::Node& after)
		{
			std::vector<std::string> keys;
			auto Add = [&keys](const std::string& key)
			{
				if (!IsMetadataKey(key) && std::find(keys.begin(), keys.end(), key) == keys.end())
					keys.push_back(key);
			};

			if (before.IsMap())
			{
				for (const auto& it : before)
				{
					if (!NodesEqual(it.second, after[it.first.Scalar()]))
						Add(it.first.Scalar());
				}
			}

			if (after.IsMap())
			{
				for (const auto& it : after)
				{
					if (!NodesEqual(before[it.first.Scalar()], it.second))
						Add(it.first.Scalar());
				}
			}

			return keys;
		}

		// The most specific property that changed. Empty if several did
		std::string GetChangedProperty(YAML::Node before, YAML::Node after)
		{
			std::string property;
			while (before.IsMap() && after.IsMap())
			{
				const std::vector<std::string> keys = GetDifferingKeys(before, after);
				if (keys.size() != 1)
					break;

				property = keys[0];
				before = before[property];
				after = after[property];
			}

			return property;
		}

		std::string DescribeComponentChanges(const std::string& before, const std::string& after)
		{
			const YAML::Node beforeNode = YAML::Load(before);
			const YAML::Node afterNode = YAML::Load(after);
			const std::vector<std::string> keys = GetDifferingKeys(beforeNode, afterNode);
			if (keys.size() != 1)
				return "Edit Components";

			const std::string& key = keys[0];
			const std::string componentName = GetComponentDisplayName(key);
			if (!beforeNode[key])
				return "Add " + componentName + " Component";

			if (!afterNode[key])
				return "Remove " + componentName + " Component";

			const std::string property = GetChangedProperty(beforeNode[key], afterNode[key]);
			return property.empty() ? "Edit " + componentName : "Edit " + componentName + " " + Utils::ToDisplayName(property);
		}

		int GetDepth(Entity entity)
		{
			int depth = 0;
			for (Entity parent = entity.GetParent(); parent; parent = parent.GetParent())
				++depth;

			return depth;
		}
	}

	// --------------
	// EntitySnapshot
	// --------------
	EntitySnapshot EntitySnapshot::Capture(const Entity& entity)
	{
		EG_CORE_ASSERT(entity, "Invalid entity");
		const Scene& scene = *entity.GetScene();

		EntitySnapshot snapshot;
		snapshot.Name = entity.GetName();

		// When an entity is destroyed, its children are moved to its parent
		Entity parent = entity.GetParent();
		while (parent && scene.IsPendingDestroy(parent))
			parent = parent.GetParent();

		if (parent)
			snapshot.Parent = parent.GetGUID();

		snapshot.WorldTransform = entity.GetWorldTransform();

		YAML::Emitter out;
		Serializer::SerializeEntityComponents(out, entity);
		snapshot.Components = out.c_str();
		return snapshot;
	}

	std::optional<EntitySnapshot> EntitySnapshot::Capture(const Scene& scene, const GUID& entityGUID)
	{
		const Entity entity = scene.GetEntityByGUID(entityGUID);
		if (IsDestroyed(scene, entity))
			return {};

		return Capture(entity);
	}

	// --------------------
	// SceneEntitiesCommand
	// --------------------
	bool SceneEntitiesCommand::CanApply(bool bUndo) const
	{
		const Ref<Scene> scene = m_Scene.lock();
		if (!scene || scene->IsPlaying())
			return false;

		// The entities have to be in the state the command left them in (or found them in, if it's redone)
		for (const Change& change : m_Changes)
		{
			const bool bExpected = bUndo ? change.After.has_value() : change.Before.has_value();
			const bool bExists = !IsDestroyed(*scene, scene->GetEntityByGUID(change.EntityGUID));
			if (bExists != bExpected)
				return false;
		}
		return true;
	}

	size_t SceneEntitiesCommand::GetMemoryUsage() const
	{
		size_t usage = sizeof(*this) + m_Changes.capacity() * sizeof(Change);
		for (const Change& change : m_Changes)
		{
			if (change.Before)
				usage += change.Before->GetMemoryUsage();
			if (change.After)
				usage += change.After->GetMemoryUsage();
		}
		return usage;
	}

	void SceneEntitiesCommand::Apply(bool bUndo)
	{
		const Ref<Scene> scene = m_Scene.lock();
		if (!scene)
			return;

		auto GetState = [bUndo](const Change& change) -> const std::optional<EntitySnapshot>& { return bUndo ? change.Before : change.After; };

		// 1. Destroy the entities that don't exist in the state. Their children are moved to their parents. The tracked ones get their parents restored below
		for (const Change& change : m_Changes)
		{
			if (GetState(change))
				continue;

			if (Entity entity = scene->GetEntityByGUID(change.EntityGUID))
				scene->DestroyEntityImmediately(entity, false);
		}

		// 2. Create the missing entities, restore names and components
		std::vector<std::pair<Entity, const EntitySnapshot*>> entities;
		entities.reserve(m_Changes.size());
		for (const Change& change : m_Changes)
		{
			const std::optional<EntitySnapshot>& state = GetState(change);
			if (!state)
				continue;

			Entity entity = scene->GetEntityByGUID(change.EntityGUID);
			if (entity && scene->IsPendingDestroy(entity))
			{
				scene->DestroyEntityImmediately(entity, false);
				entity = Entity::Null;
			}

			if (!entity)
				entity = scene->CreateEntityWithGUID(change.EntityGUID, state->Name);
			else if (entity.GetName() != state->Name)
				entity.SetName(state->Name);

			Serializer::DeserializeEntityComponents(entity, YAML::Load(state->Components));
			entities.emplace_back(entity, &*state);
		}

		// 3. Parents. The entities are detached first, so that there are no cycles in between (for example, if a child and its parent swap places)
		std::vector<std::pair<Entity, const EntitySnapshot*>> reparented;
		for (auto& [entity, state] : entities)
		{
			const Entity parent = entity.GetParent();
			const GUID parentGUID = parent ? parent.GetGUID() : GUID(0, 0);
			if (parentGUID != state->Parent)
			{
				if (parent)
					entity.SetParent(Entity::Null);

				reparented.emplace_back(entity, state);
			}
		}
		for (auto& [entity, state] : reparented)
		{
			if (!state->Parent.IsNull())
			{
				Entity parent = scene->GetEntityByGUID(state->Parent);
				if (parent)
					entity.SetParent(parent);

				EG_CORE_ASSERT(parent, "The parent of a restored entity doesn't exist");
			}
		}

		// 4. Transforms. Parents first, so that the children's world transforms are final
		std::vector<std::pair<int, size_t>> order; // Depth, index
		order.reserve(entities.size());
		for (size_t i = 0; i < entities.size(); ++i)
			order.emplace_back(GetDepth(entities[i].first), i);

		std::sort(order.begin(), order.end());
		for (const auto& [depth, index] : order)
		{
			auto& [entity, state] = entities[index];
			if (!(entity.GetWorldTransform() == state->WorldTransform))
				entity.SetWorldTransform(state->WorldTransform);
		}
	}

	std::string SceneEntitiesCommand::Describe(const std::vector<Change>& changes)
	{
		if (changes.empty())
			return "Edit Entities";

		if (changes.size() > 1)
		{
			const bool bAllCreated = std::all_of(changes.begin(), changes.end(), [](const Change& change) { return !change.Before; });
			const bool bAllDestroyed = std::all_of(changes.begin(), changes.end(), [](const Change& change) { return !change.After; });
			return bAllCreated ? "Create Entities" : bAllDestroyed ? "Delete Entities" : "Edit Entities";
		}

		const Change& change = changes[0];
		if (!change.Before)
			return "Create Entity";
		if (!change.After)
			return "Delete Entity";

		const EntitySnapshot& before = *change.Before;
		const EntitySnapshot& after = *change.After;

		std::vector<std::string> descriptions;
		if (before.Name != after.Name)
			descriptions.push_back("Rename Entity");

		if (before.Parent != after.Parent)
			descriptions.push_back(after.Parent.IsNull() ? "Detach Entity" : "Attach Entity");
		else if (!(before.WorldTransform == after.WorldTransform))
			descriptions.push_back("Edit Transform"); // Reparenting changes the transform too

		if (before.Components != after.Components)
			descriptions.push_back(DescribeComponentChanges(before.Components, after.Components));

		return descriptions.size() == 1 ? descriptions[0] : "Edit Entity";
	}

	// ----------------
	// SceneTransaction
	// ----------------
	SceneTransaction::SceneTransaction(UndoHistory* history, const Ref<Scene>& scene, std::string name)
		: m_Name(std::move(name))
	{
		if (history && scene && !scene->IsPlaying() && !history->IsApplying())
		{
			m_History = history;
			m_Scene = scene;

			// The changes are made after it's created. The edits in progress are recorded first, otherwise they're dropped when it's recorded
			history->FlushPendingEdits();
		}
	}

	void SceneTransaction::Track(const Entity& entity, bool bWithChildren)
	{
		TrackEntity(entity, bWithChildren, false);
	}

	void SceneTransaction::TrackCreated(const Entity& entity, bool bWithChildren)
	{
		TrackEntity(entity, bWithChildren, true);
	}

	void SceneTransaction::TrackEntity(const Entity& entity, bool bWithChildren, bool bCreated)
	{
		if (!m_History || !entity)
			return;

		const Ref<Scene> scene = m_Scene.lock();
		if (!scene || entity.GetScene() != scene.get())
			return;

		const GUID guid = entity.GetGUID();
		if (m_Before.find(guid) == m_Before.end())
		{
			m_Before.emplace(guid, bCreated ? std::optional<EntitySnapshot>() : EntitySnapshot::Capture(*scene, guid));
			m_Tracked.push_back(guid);
		}

		if (bWithChildren)
		{
			for (const Entity& child : entity.GetChildren())
				TrackEntity(child, true, bCreated);
		}
	}

	void SceneTransaction::TrackAll()
	{
		if (!m_History)
			return;

		const Ref<Scene> scene = m_Scene.lock();
		if (!scene)
			return;

		m_bTrackAll = true;
		scene->OnEach([this](const Entity& entity) { Track(entity); });
	}

	void SceneTransaction::Commit()
	{
		UndoHistory* history = m_History;
		const Ref<Scene> scene = m_Scene.lock();
		m_History = nullptr;
		if (!history || !scene || history->IsApplying())
			return;

		// Entities created during the transaction
		if (m_bTrackAll)
		{
			scene->OnEach([this](const Entity& entity)
			{
				const GUID guid = entity.GetGUID();
				if (m_Before.emplace(guid, std::optional<EntitySnapshot>()).second)
					m_Tracked.push_back(guid);
			});
		}

		std::vector<SceneEntitiesCommand::Change> changes;
		for (const GUID& guid : m_Tracked)
		{
			std::optional<EntitySnapshot>& before = m_Before[guid];
			std::optional<EntitySnapshot> after = EntitySnapshot::Capture(*scene, guid);
			if (before != after)
				changes.push_back({ guid, std::move(before), std::move(after) });
		}

		m_Tracked.clear();
		m_Before.clear();
		if (changes.empty())
			return;

		std::string name = m_Name.empty() ? SceneEntitiesCommand::Describe(changes) : std::move(m_Name);
		history->Record(std::move(name), MakeScope<SceneEntitiesCommand>(scene, std::move(changes)));
	}

	void SceneTransaction::Cancel()
	{
		m_History = nullptr;
		m_Tracked.clear();
		m_Before.clear();
	}

	// -----------------
	// EntityEditTracker
	// -----------------
	EntityEditTracker::EntityEditTracker()
		: m_Tracker(
			[](const Target& target) -> std::optional<EntitySnapshot>
			{
				const Ref<Scene> scene = target.TargetScene.lock();
				if (!scene || scene->IsPlaying())
					return {};

				return EntitySnapshot::Capture(*scene, target.EntityGUID);
			},
			[](UndoHistory& history, const Target& target, EntitySnapshot&& before, EntitySnapshot&& after)
			{
				const Ref<Scene> scene = target.TargetScene.lock();
				if (!scene)
					return;

				std::vector<SceneEntitiesCommand::Change> changes;
				changes.push_back({ target.EntityGUID, std::move(before), std::move(after) });
				std::string name = SceneEntitiesCommand::Describe(changes);
				history.Record(std::move(name), MakeScope<SceneEntitiesCommand>(scene, std::move(changes)));
			})
	{
	}

	void EntityEditTracker::BeginFrame(UndoHistory* history, const Ref<Scene>& scene, const Entity& entity)
	{
		const bool bRecord = scene && entity && entity.GetScene() == scene.get() && !scene->IsPlaying();
		m_Tracker.BeginFrame(bRecord ? history : nullptr, Target{ scene, bRecord ? entity.GetGUID() : GUID(0, 0) });
	}

	// --------------------
	// SceneSettingsCommand
	// --------------------
	void SceneSettingsCommand::Undo()
	{
		if (const Ref<Scene> scene = m_Scene.lock())
			scene->SetSettings(m_Before);
	}

	void SceneSettingsCommand::Redo()
	{
		if (const Ref<Scene> scene = m_Scene.lock())
			scene->SetSettings(m_After);
	}

	bool SceneSettingsCommand::CanApply(bool bUndo) const
	{
		const Ref<Scene> scene = m_Scene.lock();
		return scene && !scene->IsPlaying();
	}

	std::string SceneSettingsCommand::Describe(const SceneSettings& before, const SceneSettings& after)
	{
		const auto& [beforePhysics, beforeSkybox] = before;
		const auto& [afterPhysics, afterSkybox] = after;

		const bool bPhysics = beforePhysics != afterPhysics;
		const bool bSkybox = beforeSkybox != afterSkybox;
		if (bPhysics && !bSkybox)
			return "Edit Physics Settings";
		if (bSkybox && !bPhysics)
			return "Edit Skybox Settings";

		return "Edit Scene Settings";
	}

	// ------------------------
	// SceneSettingsEditTracker
	// ------------------------
	SceneSettingsEditTracker::SceneSettingsEditTracker()
		: m_Tracker(
			[](const Target& target) -> std::optional<SceneSettings>
			{
				const Ref<Scene> scene = target.TargetScene.lock();
				if (!scene || scene->IsPlaying())
					return {};

				return scene->GetSettings();
			},
			[](UndoHistory& history, const Target& target, SceneSettings&& before, SceneSettings&& after)
			{
				const Ref<Scene> scene = target.TargetScene.lock();
				if (!scene)
					return;

				std::string name = SceneSettingsCommand::Describe(before, after);
				history.Record(std::move(name), MakeScope<SceneSettingsCommand>(scene, std::move(before), std::move(after)));
			})
	{
	}

	void SceneSettingsEditTracker::BeginFrame(UndoHistory* history, const Ref<Scene>& scene)
	{
		const bool bRecord = scene && !scene->IsPlaying();
		m_Tracker.BeginFrame(bRecord ? history : nullptr, Target{ scene });
	}
}
