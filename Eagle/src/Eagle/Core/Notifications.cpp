#include "egpch.h"
#include "Notifications.h"
#include "Eagle/Components/SceneComponent.h"
#include "Eagle/Core/Entity.h"

namespace Eagle
{
	static std::unordered_map<Entity, std::vector<Component*>> s_Entities;
	
	void ComponentsNotificationSystem::AddObserver(const Entity& parent, Component* observer)
	{
		s_Entities[parent].push_back(observer);
	}
	
	void ComponentsNotificationSystem::RemoveObserver(const Entity& parent, Component* observer)
	{
		auto it = s_Entities.find(parent);
		if (it != s_Entities.end())
		{
			auto& children = it->second;
			auto it = std::find(children.begin(), children.end(), observer);
			if (it != children.end())
				children.erase(it);
		}
	}
	
	void ComponentsNotificationSystem::Notify(const Entity& parent, Notification notification)
	{
		auto& children = s_Entities[parent];
		for (auto& child : children)
			child->OnNotify(notification);
	}
	
	void ComponentsNotificationSystem::ResetSystem()
	{
		s_Entities.clear();
	}
}
