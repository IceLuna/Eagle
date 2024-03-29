#pragma once

#include <vector>
#include <entt.hpp>

namespace Eagle
{
	enum class Notification
	{
		None,
		OnParentTransformChanged
	};
	class Component;
	class Entity;

	class ComponentsNotificationSystem
	{
	public:
		static void AddObserver(const Entity& parent, Component* observer);
		static void RemoveObserver(const Entity& parent, Component* observer);
		static void Notify(const Entity& parent, Notification notification);
		static void ResetSystem() { m_Entities.clear();  }

	private:
		static std::unordered_map<entt::entity, std::vector<Component*>> m_Entities;
	};
}
