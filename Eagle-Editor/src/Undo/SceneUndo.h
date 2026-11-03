#pragma once

#include "UndoHistory.h"
#include "EditTracker.h"

#include "Eagle/Core/Entity.h"
#include "Eagle/Core/GUID.h"
#include "Eagle/Math/Transform.h"

namespace Eagle
{
	class Scene;
	class AssetTextureCube;

	struct EntitySnapshot
	{
		std::string Name;
		GUID Parent = GUID(0, 0); // Null if it's a root entity
		Transform WorldTransform;
		std::string Components; // YAML. See `Serializer::SerializeEntityComponents()`

		static EntitySnapshot Capture(const Entity& entity);

		// Null if the entity doesn't exist (or it's pending destroy)
		static std::optional<EntitySnapshot> Capture(const Scene& scene, const GUID& entityGUID);

		size_t GetMemoryUsage() const { return sizeof(*this) + Name.capacity() + Components.capacity(); }

		bool operator==(const EntitySnapshot& other) const = default;
	};

	// Restores entities of a scene to their recorded states. An entity that doesn't exist in a state is destroyed, the missing ones are recreated (with the same GUID).
	// Existing entities are restored in place: they keep their entity IDs, so references to them stay valid (selection, children, etc).
	// Only what differs is changed, for example a component is only recreated if its state differs
	class SceneEntitiesCommand : public UndoCommand
	{
	public:
		struct Change
		{
			GUID EntityGUID;
			std::optional<EntitySnapshot> Before; // Null if it didn't exist
			std::optional<EntitySnapshot> After;  // Null if it was destroyed
		};

		SceneEntitiesCommand(const Ref<Scene>& scene, std::vector<Change> changes)
			: m_Scene(scene), m_Changes(std::move(changes))
		{}

		void Undo() override { Apply(true); }
		void Redo() override { Apply(false); }
		bool CanApply(bool bUndo) const override;
		size_t GetMemoryUsage() const override;
		bool ModifiesScene(const Scene& scene) const override { return m_Scene.lock().get() == &scene; }

		Ref<Scene> GetScene() const { return m_Scene.lock(); }
		const std::vector<Change>& GetChanges() const { return m_Changes; }

		// A name that describes the changes, for example "Edit Point Light Intensity" or "Delete Entity"
		static std::string Describe(const std::vector<Change>& changes);

	private:
		void Apply(bool bUndo);

	private:
		Weak<Scene> m_Scene;
		std::vector<Change> m_Changes;
	};

	// Records changes of entities of a scene as one undo transaction.
	// Entities are captured when they're tracked (before the change), and again when the transaction is committed.
	// Only the entities whose state has changed are recorded.
	// Usage example:
	//     SceneTransaction transaction(history, scene, "Delete Entity");
	//     transaction.Track(entity, true);
	//     scene->DestroyEntity(entity);
	class SceneTransaction
	{
	public:
		// @name. If empty, it's named after the changes (see `SceneEntitiesCommand::Describe()`)
		SceneTransaction(UndoHistory* history, const Ref<Scene>& scene, std::string name = {});
		~SceneTransaction() { Commit(); }

		SceneTransaction(const SceneTransaction&) = delete;
		SceneTransaction& operator=(const SceneTransaction&) = delete;

		// Captures the entity before it's changed
		// @bWithChildren. Its children hierarchy is tracked too. For example, deleting an entity changes the parents of its children
		void Track(const Entity& entity, bool bWithChildren = false);

		// For entities that were created during the transaction. They're tracked as if they didn't exist before
		void TrackCreated(const Entity& entity, bool bWithChildren = true);

		// For operations that might change any entity of the scene. Every entity is captured, and the ones created during the transaction are detected
		void TrackAll();

		void Commit();
		void Cancel();

		// False if nothing is going to be recorded
		bool IsRecording() const { return m_History != nullptr; }

	private:
		void TrackEntity(const Entity& entity, bool bWithChildren, bool bCreated);

	private:
		UndoHistory* m_History = nullptr;
		Weak<Scene> m_Scene;
		std::string m_Name;
		std::vector<GUID> m_Tracked; // In the order they were tracked
		std::unordered_map<GUID, std::optional<EntitySnapshot>> m_Before;
		bool m_bTrackAll = false;
	};

	// Turns the edits of an entity, made through the widgets of a panel (for example, the properties panel), into undo transactions.
	class EntityEditTracker
	{
	public:
		EntityEditTracker();

		// Call within the panel's window, right before the widgets that edit the entity are drawn
		void BeginFrame(UndoHistory* history, const Ref<Scene>& scene, const Entity& entity);

		// Call every frame, after the panel (whether it was drawn or not)
		void EndFrame() { m_Tracker.EndFrame(); }

		void Reset() { m_Tracker.Reset(); }

	private:
		struct Target
		{
			Weak<Scene> TargetScene;
			GUID EntityGUID;

			bool operator==(const Target& other) const { return EntityGUID == other.EntityGUID && !TargetScene.owner_before(other.TargetScene) && !other.TargetScene.owner_before(TargetScene); }
		};

		EditTracker<Target, EntitySnapshot> m_Tracker;
	};

	class SceneSettingsCommand : public UndoCommand
	{
	public:
		SceneSettingsCommand(const Ref<Scene>& scene, SceneSettings before, SceneSettings after)
			: m_Scene(scene), m_Before(std::move(before)), m_After(std::move(after)) {}

		void Undo() override;
		void Redo() override;
		bool CanApply(bool bUndo) const override;
		size_t GetMemoryUsage() const override { return sizeof(*this); }
		bool ModifiesScene(const Scene& scene) const override { return m_Scene.lock().get() == &scene; }

		static std::string Describe(const SceneSettings& before, const SceneSettings& after);

	private:
		Weak<Scene> m_Scene;
		SceneSettings m_Before;
		SceneSettings m_After;
	};

	// Turns the edits of the settings of a scene, made through the widgets of a panel, into undo transactions
	class SceneSettingsEditTracker
	{
	public:
		SceneSettingsEditTracker();

		// Call within the panel's window, right before its widgets are drawn
		void BeginFrame(UndoHistory* history, const Ref<Scene>& scene);

		// Call every frame, after the panel (whether it was drawn or not)
		void EndFrame() { m_Tracker.EndFrame(); }

		void Reset() { m_Tracker.Reset(); }

	private:
		struct Target
		{
			Weak<Scene> TargetScene;

			bool operator==(const Target& other) const { return !TargetScene.owner_before(other.TargetScene) && !other.TargetScene.owner_before(TargetScene); }
		};

		EditTracker<Target, SceneSettings> m_Tracker;
	};
}
