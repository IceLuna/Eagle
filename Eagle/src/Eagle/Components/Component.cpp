#include "egpch.h"
#include "Component.h" 

namespace Eagle
{
	Component::Component(Component&& other) noexcept
		: Object(other)
		, Parent(std::move(other.Parent))
	{
		ComponentsNotificationSystem::RemoveObserver(Parent, &other);
		ComponentsNotificationSystem::AddObserver(Parent, this);
	}

	Component& Component::operator=(const Component& other)
	{
		if (this == &other)
			return *this;

		Object::operator=(other);

		if (!Parent)
		{
			Parent = other.Parent;
			ComponentsNotificationSystem::AddObserver(Parent, this);
		}

		return *this;
	}

	Component& Component::operator=(Component&& other) noexcept
	{
		Object::operator=(std::move(other));

		Parent = std::move(other.Parent);

		ComponentsNotificationSystem::RemoveObserver(Parent, &other);
		ComponentsNotificationSystem::AddObserver(Parent, this);

		return *this;
	}

	Component::~Component()
	{
		if (Parent)
			ComponentsNotificationSystem::RemoveObserver(Parent, this);
	}

	void Component::OnInit(Entity entity)
	{
		Parent = entity;
		ComponentsNotificationSystem::AddObserver(Parent, this);
	}
}