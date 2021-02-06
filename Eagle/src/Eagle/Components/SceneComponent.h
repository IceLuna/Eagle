#pragma once

#include "Eagle/Core/Transform.h"
#include <string>
#include <set>

namespace Eagle
{
	class SceneComponent
	{
	public:
		SceneComponent(const Transform& transform = Eagle::Transform(), const std::string& name = std::string("Unnamed Component"))
			: Transform(transform)
			, Name(name)
		{}

		virtual ~SceneComponent() = default;

		void AddTag(const std::string& tag);
		void RemoveTag(const std::string& tag);

		const std::set<std::string>& GetTags() const { return m_Tags; }

	public:
		Eagle::Transform Transform;
		std::string Name;
	
	protected:
		std::set<std::string> m_Tags;

		//TODO: 
		//glm::vec3 m_Velocity;
		//bool m_Visible;
		//bool m_HiddenInGame;
	};
}
