#include "egpch.h"

#include "SceneComponent.h"

namespace Eagle
{
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