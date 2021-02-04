#pragma once

#include <entt.hpp>
#include "Scene.h"

namespace Eagle
{
	class Entity
	{
	public:
		Entity() = default;
		Entity(entt::entity entity, Scene* scene, std::string nameInScene = "Entity")
		: m_Entity(entity)
		, m_Scene(scene)
		, m_NameInScene(nameInScene)
		{}
		
		Entity(const Entity&) = default;

		void SetName(const std::string& name) { m_NameInScene = name; }
		const std::string& GetName() const { return m_NameInScene; }

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
			return m_Scene->m_Registry.emplace<T>(m_Entity, std::forward<Args>(args)...);
		}

		template<typename T>
		void RemoveComponent()
		{
			EG_CORE_ASSERT(HasComponent<T>(), "Entity does not have component!");
			m_Scene->m_Registry.remove<T>(m_Entity);
		}

		operator bool() const { return m_Entity != entt::null; }

	private:
		entt::entity m_Entity = entt::null;
		Scene* m_Scene = nullptr;
		std::string m_NameInScene;
	};
}