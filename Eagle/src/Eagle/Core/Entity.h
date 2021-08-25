#pragma once

#include <entt.hpp>
#include <unordered_map>

#include "Notifications.h"
#include "Scene.h"

namespace Eagle
{
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

		void SetOwner(Entity& owner);
		Entity& GetOwner();

		void SetWorldTransform(const Transform& worldTransform);
		const Transform& GetWorldTransform();

		void SetWorldLocation(const glm::vec3& worldLocation);
		const glm::vec3& GetWorldLocation();

		void SetWorldRotation(const glm::vec3& worldRotation);
		const glm::vec3& GetWorldRotation();

		void SetWorldScale(const glm::vec3& worldScale);
		const glm::vec3& GetWorldScale();

		void SetRelativeLocation(const glm::vec3& relativeLocation);
		const glm::vec3& GetRelativeLocation();

		void SetRelativeRotation(const glm::vec3& relativeRotation);
		const glm::vec3& GetRelativeRotation();

		void SetRelativeScale(const glm::vec3& relativeScale);
		const glm::vec3& GetRelativeScale();
		
		void SetRelativeTransform(const Transform& relativeTransform);
		const Transform& GetRelativeTransform();

		const std::vector<Entity>& GetChildren();
		bool HasOwner();
		bool HasChildren();
		bool IsOwnerOf(Entity& entity);
		bool IsValid() const;

		uint32_t GetID() const { return (uint32_t)m_Entity; }
		const GUID& GetGUID() const;
		entt::entity GetEnttID() const { return m_Entity; }
		const Scene* GetScene() const { return m_Scene; }

		void OnNotify(Notification notification);

	private:
		void NotifyAllChildren(Notification notification);
		void AddChildren(Entity& child);
		void RemoveChildren(Entity& child);

	public:
		template<typename T>
		bool HasComponent() const
		{
			return m_Scene->m_Registry.has<T>(m_Entity);
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

			if constexpr (std::is_same<T, CameraComponent>::value)
				if (HasComponent<NativeScriptComponent>() == false)
					AddComponent<NativeScriptComponent>().Bind<CameraController>();

			return component;
		}

		template<typename T>
		void RemoveComponent()
		{
			EG_CORE_ASSERT(HasComponent<T>(), "Entity does not have component!");
			m_Scene->m_Registry.remove<T>(m_Entity);
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
