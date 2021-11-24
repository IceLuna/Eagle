#pragma once

#include <entt.hpp>
#include <unordered_map>

#include "Notifications.h"
#include "Scene.h"

namespace Eagle
{
	class PhysicsActor;

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

		void SetParent(Entity& parent);
		Entity& GetParent();
		const Entity& GetParent() const;

		void SetWorldTransform(const Transform& worldTransform);
		const Transform& GetWorldTransform() const;

		void SetWorldLocation(const glm::vec3& worldLocation);
		const glm::vec3& GetWorldLocation() const;

		void SetWorldRotation(const Rotator& worldRotation);
		const Rotator& GetWorldRotation() const;

		void SetWorldScale(const glm::vec3& worldScale);
		const glm::vec3& GetWorldScale() const;

		void SetRelativeLocation(const glm::vec3& relativeLocation);
		const glm::vec3& GetRelativeLocation() const;

		void SetRelativeRotation(const Rotator& relativeRotation);
		const Rotator& GetRelativeRotation() const;

		void SetRelativeScale(const glm::vec3& relativeScale);
		const glm::vec3& GetRelativeScale() const;
		
		void SetRelativeTransform(const Transform& relativeTransform);
		const Transform& GetRelativeTransform() const;

		glm::vec3 GetLinearVelocity() const;

		glm::vec3 GetForwardVector() const
		{
			return glm::rotate(GetWorldRotation().GetQuat(), glm::vec3(0.f, 0.f, -1.f));
		}

		glm::vec3 GetUpVector() const
		{
			return glm::rotate(GetWorldRotation().GetQuat(), glm::vec3(0.f, 1.f, 0.f));
		}

		glm::vec3 GetRightVector() const
		{
			return glm::rotate(GetWorldRotation().GetQuat(), glm::vec3(1.f, 0.f, 0.f));
		}

		const std::vector<Entity>& GetChildren() const;
		bool HasParent() const;
		bool HasChildren() const;
		bool IsParentOf(const Entity& entity) const;
		bool IsValid() const;

		uint32_t GetID() const { return (uint32_t)m_Entity; }
		const GUID& GetGUID() const;
		entt::entity GetEnttID() const { return m_Entity; }
		const Scene* GetScene() const { return m_Scene; }
		const std::string& GetSceneName() const;

		const Ref<PhysicsActor>& GetPhysicsActor() const { return m_Scene->GetPhysicsActor(*this); }
		Ref<PhysicsActor>& GetPhysicsActor() { return m_Scene->GetPhysicsActor(*this); }

		void OnNotify(Notification notification);

	private:
		void NotifyAllChildren(Notification notification);
		void AddChildren(Entity& child);
		void RemoveChildren(Entity& child);

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
			T& component = m_Scene->m_Registry.emplace<T>(m_Entity, std::forward<Args>(args)...);
			component.OnInit(*this);

			return component;
		}

		template<typename T>
		void RemoveComponent()
		{
			EG_CORE_ASSERT(HasComponent<T>(), "Entity does not have component!");
			GetComponent<T>().OnRemoved(*this);
			m_Scene->m_Registry.remove<T>(m_Entity);
		}

		template<typename... T>
		bool HasAny() const
		{
			return m_Scene->m_Registry.any_of<T...>(m_Entity);
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
			return hash<entt::entity>()(entity.GetEnttID());
		}
	};
}
