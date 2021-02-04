#include "egpch.h"

#include "SceneComponent.h"

namespace Eagle
{
	void SceneComponent::SetName(const std::string& name)
	{
		m_Name = name;
	}

	void SceneComponent::SetTransform(const Transform& transform)
	{
		m_Transform = transform;
	}

	void SceneComponent::AddTag(const std::string& tag)
	{
		m_Tags.insert(tag);
	}

	void SceneComponent::RemoveTag(const std::string& tag)
	{
		if (m_Tags.find(tag) != m_Tags.end())
		{
			m_Tags.erase(tag);
		}
	}
}