#include "egpch.h"
#include "Notifications.h"
#include "Eagle/Components/SceneComponent.h"
#include "Eagle/Core/Entity.h"

namespace Eagle
{
	static Scene* GetObserversScene(Entity parent)
	{
		return parent.GetEnttID() == entt::null ? nullptr : parent.GetScene();
	}

	void ComponentsNotificationSystem::AddObserver(const Entity& parent, Component* observer)
	{
		if (Scene* scene = GetObserversScene(parent))
			scene->m_ComponentObservers[uint32_t(parent.GetEnttID())].push_back(observer);
	}
	
	void ComponentsNotificationSystem::RemoveObserver(const Entity& parent, Component* observer)
	{
		Scene* scene = GetObserversScene(parent);
		if (!scene)
			return;

		auto& observers = scene->m_ComponentObservers;
		auto it = observers.find(uint32_t(parent.GetEnttID()));
		if (it != observers.end())
		{
			auto& children = it->second;
			auto childIt = std::find(children.begin(), children.end(), observer);
			if (childIt != children.end())
				children.erase(childIt);

			// Note: the entry itself is kept on purpose. Erasing it could move other entries of the map
			// while `Notify` is iterating one of them (an observer might remove itself or another one from `OnNotify`)
		}
	}
	
	void ComponentsNotificationSystem::Notify(const Entity& parent, Notification notification)
	{
		Scene* scene = GetObserversScene(parent);
		if (!scene)
			return;

		auto& observers = scene->m_ComponentObservers;
		auto it = observers.find(uint32_t(parent.GetEnttID()));
		if (it == observers.end())
			return;

		auto& children = it->second;
		for (auto& child : children)
			child->OnNotify(notification);
	}
}
