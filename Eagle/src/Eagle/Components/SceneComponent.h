#pragma once

#include "Eagle/Core/Transform.h"
#include <string>
#include <set>

namespace Eagle
{
	class SceneComponent
	{
	public:
		SceneComponent(const Transform& transform = Transform(), const std::string& name = std::string("Unnamed Component"))
			: m_Transform(transform)
			, m_Name(name)
		{}

		virtual ~SceneComponent() = default;

		void SetName(const std::string& name);
		void SetTransform(const Transform& transform);
		void AddTag(const std::string& tag);
		void RemoveTag(const std::string& tag);

		const Transform& GetTransform() const { return m_Transform; }
		const std::string& GetName() const { return m_Name; }

	protected:
		Transform m_Transform;
		std::string m_Name;
		std::set<std::string> m_Tags;

		//TODO: 
		//glm::vec3 m_Velocity;
		//bool m_Visible;
		//bool m_HiddenInGame;
	};
}
