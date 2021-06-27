#pragma once

#include <entt.hpp>

#include "Scene.h"
#include "Notifications.h"

namespace Eagle
{
	class Object;

	class Entity
	{
	public:
		Entity() = default;
		Entity(entt::entity entity, Scene* scene)
		: m_Entity(entity)
		, m_Scene(scene)
		{}
		
		Entity(const Entity&) = default;
		Entity(Entity&& other) 
		{ 
			m_Entity = other.m_Entity; other.m_Entity = entt::null; 
			m_Scene = other.m_Scene; other.m_Scene = nullptr;	
		}

		Entity& operator= (const Entity&) = default;
		Entity& operator= (Entity&& other)
		{
			m_Entity = other.m_Entity; other.m_Entity = entt::null;
			m_Scene = other.m_Scene; other.m_Scene = nullptr;
			return *this;
		}

		void SetOwner(Entity& owner);
		Entity& GetOwner();

		const std::vector<Entity>& GetChildren();

		void AddObserver(Observer* observer);
		void RemoveObserver(Observer* observer);

		const Transform& GetWorldTransform();
		void SetWorldTransform(const Transform& worldTransform);
		
		const Transform& GetRelativeTransform();
		void SetRelativeTransform(const Transform& relativeTransform);

		bool HasOwner();
		bool HasChildren();
		bool IsOwnerOf(Entity& entity);
		bool IsValid() const;

		uint32_t GetID() const { return (uint32_t)m_Entity; }

		void OnNotify(Notification notification);

	private:

		void AddChildren(Entity& child);
		void RemoveChildren(Entity& child);

	public:
		template<typename T>
		bool HasComponent()
		{
			return m_Scene->m_Registry.has<T>(m_Entity);
		}

		template<typename T>
		T& GetComponent()
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
			m_Scene->m_Registry.remove<T>(m_Entity);
		}

		operator bool() const { return m_Entity != entt::null; }
		entt::entity GetEnttID() const { return m_Entity; }
		
		bool operator== (const Entity& other)
		{
			return m_Entity == other.m_Entity;
		}

		bool operator!= (const Entity& other)
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
