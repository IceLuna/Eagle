#include "egpch.h"
#include "Component.h" 

namespace Eagle
{
	Component::Component(Component&& other) noexcept
		: Object(other)
		, Name(std::move(other.Name))
		, Parent(std::move(other.Parent))
		, m_Tags(std::move(other.m_Tags))
	{
		ComponentsNotificationSystem::RemoveObserver(Parent, &other);
		ComponentsNotificationSystem::AddObserver(Parent, this);
	}

	Component& Component::operator=(const Component& other)
	{
		if (this == &other)
			return *this;

		Object::operator=(other);

		Name = other.Name;
		m_Tags = other.m_Tags;

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

		Name = std::move(other.Name);
		Parent = std::move(other.Parent);
		m_Tags = std::move(other.m_Tags);

		ComponentsNotificationSystem::RemoveObserver(Parent, &other);
		ComponentsNotificationSystem::AddObserver(Parent, this);

		return *this;
	}

	Component::~Component()
	{
		if (Parent)
			ComponentsNotificationSystem::RemoveObserver(Parent, this);
	}

	void Component::OnInit(Entity& entity)
	{
		Parent = entity;
		ComponentsNotificationSystem::AddObserver(Parent, this);
	}

	void Component::AddTag(const std::string& tag)
	{
		m_Tags.insert(tag);
	}

	void Component::RemoveTag(const std::string& tag)
	{
		auto& it = m_Tags.find(tag);
		if (it != m_Tags.end())
		{
			m_Tags.erase(it);
		}
	}
}