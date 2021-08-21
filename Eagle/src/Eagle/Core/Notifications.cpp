#include "egpch.h"
#include "Notifications.h"
#include "Eagle/Components/SceneComponent.h"
#include "Eagle/Core/Entity.h"

namespace Eagle
{
	std::unordered_map<entt::entity, std::vector<Component*>> ComponentsNotificationSystem::m_Entities;
	
	void ComponentsNotificationSystem::AddObserver(const Entity& parent, Component* observer)
	{
		m_Entities[parent.GetEnttID()].push_back(observer);
	}
	
	void ComponentsNotificationSystem::RemoveObserver(const Entity& parent, Component* observer)
	{
		auto& children = m_Entities[parent.GetEnttID()];
		auto it = std::find(children.begin(), children.end(), observer);
		if (it != children.end())
			children.erase(it);
	}
	
	void ComponentsNotificationSystem::Notify(const Entity& parent, Notification notification)
	{
		auto& children = m_Entities[parent.GetEnttID()];
		for (auto& child : children)
			child->OnNotify(notification);
	}
}
