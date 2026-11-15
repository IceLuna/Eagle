#pragma once

#include <entt.hpp>
#include <unordered_map>

#include "Notifications.h"
#include "Scene.h"
#include "Core.h"

namespace Eagle
{
	class PhysicsActor;

	using EntityIDType = uint32_t;

	// How the physics of an entity reacts to transform changes
	enum class PhysicsMove
	{
		// A kinematic body reaches the new transform during the next physics update and pushes what's in its way.
		// A dynamic body is driven by the simulation, so it's teleported, keeping its velocity (to move it, set its velocity).
		// A ragdoll stays where it is
		Move,
		Teleport, // Teleported, keeping the velocity. A ragdoll is teleported along
		Reset,    // Teleported, and the velocity is reset. A ragdoll is teleported along and stops

		Skip      // Internal. The physics isn't touched (used when the physics moved the entity)
	};

	class Entity
	{
	public:
		Entity() = default;
		Entity(entt::entity entity, Scene* scene)
		: m_Entity(entity)
		, m_Scene(scene)
		{}
		
		Entity(const Entity&) = default;
		Entity(Entity&& other) noexcept
		{ 
			m_Entity = other.m_Entity; other.m_Entity = entt::null; 
			m_Scene = other.m_Scene; other.m_Scene = nullptr;	
		}

		Entity& operator= (const Entity&) = default;
		Entity& operator= (Entity&& other) noexcept
		{
			m_Entity = other.m_Entity; other.m_Entity = entt::null;
			m_Scene = other.m_Scene; other.m_Scene = nullptr;
			return *this;
		}

		void SetParent(Entity parent);
		Entity GetParent();
		const Entity GetParent() const;

		void SetWorldTransform(const Transform& worldTransform, PhysicsMove physicsMove = PhysicsMove::Teleport);
		const Transform& GetWorldTransform() const;

		void SetWorldLocation(const glm::vec3& worldLocation, PhysicsMove physicsMove = PhysicsMove::Teleport);
		const glm::vec3& GetWorldLocation() const;

		void SetWorldRotation(const Rotator& worldRotation, PhysicsMove physicsMove = PhysicsMove::Teleport);
		const Rotator& GetWorldRotation() const;

		void SetWorldScale(const glm::vec3& worldScale, PhysicsMove physicsMove = PhysicsMove::Teleport);
		const glm::vec3& GetWorldScale() const;

		// Relative to the parent. Without a parent, it's the world transform
		void SetRelativeLocation(const glm::vec3& relativeLocation, PhysicsMove physicsMove = PhysicsMove::Teleport);
		const glm::vec3& GetRelativeLocation() const;

		void SetRelativeRotation(const Rotator& relativeRotation, PhysicsMove physicsMove = PhysicsMove::Teleport);
		const Rotator& GetRelativeRotation() const;

		void SetRelativeScale(const glm::vec3& relativeScale, PhysicsMove physicsMove = PhysicsMove::Teleport);
		const glm::vec3& GetRelativeScale() const;
		
		void SetRelativeTransform(const Transform& relativeTransform, PhysicsMove physicsMove = PhysicsMove::Teleport);
		const Transform& GetRelativeTransform() const;

		void SetLinearVelocity(const glm::vec3& velocity);
		glm::vec3 GetLinearVelocity() const;

		void SetAngularVelocity(const glm::vec3& velocity);
		glm::vec3 GetAngularVelocity() const;

		glm::vec3 GetForwardVector() const
		{
			return Math::GetForwardVector(GetWorldRotation());
		}

		glm::vec3 GetUpVector() const
		{
			return Math::GetUpVector(GetWorldRotation());
		}

		glm::vec3 GetRightVector() const
		{
			return Math::GetRightVector(GetWorldRotation());
		}

		void TriggerAnimationEvent(const std::string& name, float time) const;
		void TriggerSequenceEvent(const std::string& name, float time) const;

		const std::vector<Entity>& GetChildren() const;
		bool HasParent() const;
		bool HasChildren() const;
		bool IsParentOf(const Entity& entity) const;

		// Excludes entities from the input if it already contains its parent.
		// Returns the updated list
		static std::vector<Entity> GetTopmostEntities(const std::vector<Entity>& entities);
		bool IsValid() const;

		EntityIDType GetID() const { return (EntityIDType)m_Entity; }
		const GUID& GetGUID() const;
		entt::entity GetEnttID() const { return m_Entity; }
		const Scene* GetScene() const { return m_Scene; }
		Scene* GetScene() { return m_Scene; }

		void SetName(const std::string& name);
		const std::string& GetName() const;

		const Ref<PhysicsActor>& GetPhysicsActor() const { return m_Scene->GetPhysicsActor(*this); }
		Ref<PhysicsActor>& GetPhysicsActor() { return m_Scene->GetPhysicsActor(*this); }

		void OnNotify(Notification notification);

	private:
		void UpdatePhysicsTransform(const Transform& oldWorldTransform, PhysicsMove physicsMove);
		void NotifyAllChildren(Notification notification, PhysicsMove physicsMove = PhysicsMove::Teleport);
		void AddChildren(Entity child);
		void RemoveChildren(Entity child);

	public:
		template<typename T>
		bool HasComponent() const
		{
			return m_Scene->m_Registry.all_of<T>(m_Entity);
		}

		template<typename T>
		T& GetComponent()
		{
			EG_CORE_ASSERT(HasComponent<T>(), "Entity does not have component!");
			return m_Scene->m_Registry.get<T>(m_Entity);
		}

		template<typename T>
		const T& GetComponent() const
		{
			EG_CORE_ASSERT(HasComponent<T>(), "Entity does not have component!");
			return m_Scene->m_Registry.get<T>(m_Entity);
		}

		template<typename T, typename... Args>
		T& AddComponent(Args&&... args)
		{
			EG_CORE_ASSERT(!HasComponent<T>(), "Entity already has component!");

			if constexpr (std::is_base_of<Component, T>::value)
				return m_Scene->m_Registry.emplace<T>(m_Entity, *this, std::forward<Args>(args)...);
			else
				return m_Scene->m_Registry.emplace<T>(m_Entity, std::forward<Args>(args)...);
		}

		template<typename T>
		void RemoveComponent()
		{
			EG_CORE_ASSERT(HasComponent<T>(), "Entity does not have component!");
			if constexpr (std::is_base_of<Component, T>::value)
				GetComponent<T>().OnRemoved();
			m_Scene->m_Registry.remove<T>(m_Entity);
		}

		template<typename... T>
		bool HasAny() const
		{
			return m_Scene->m_Registry.any_of<T...>(m_Entity);
		}

		template<typename T>
		void SignalComponentChanged(Notification notification)
		{
			m_Scene->OnComponentChanged<T>(GetComponent<T>(), notification);
		}

		operator bool() const { return IsValid(); }
		
		bool operator== (const Entity& other) const
		{
			return m_Entity == other.m_Entity;
		}

		bool operator!= (const Entity& other) const
		{
			return !(*this == other);
		}

		bool operator< (const Entity& other) const
		{
			if (m_Entity != other.m_Entity)
				return m_Entity < other.m_Entity;
			return m_Scene < other.m_Scene;
		}
	
	public:
		static Entity Null;

	private:
		entt::entity m_Entity = entt::null;
		Scene* m_Scene = nullptr;
	};
}

namespace std
{
	template <>
	struct hash<Eagle::Entity>
	{
		std::size_t operator()(const Eagle::Entity& entity) const
		{
			size_t hash = std::hash<entt::entity>()(entity.GetEnttID());
			Eagle::HashCombine(hash, entity.GetScene());
			return hash;
		}
	};
}
