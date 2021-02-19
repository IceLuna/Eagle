#pragma once

#include "Eagle/Core/Transform.h"
#include <string>
#include <set>

#include "Eagle/Core/Entity.h"
#include "Eagle/Core/Object.h"

namespace Eagle
{
	class Component : public Object
	{
	public:
		Component(const std::string& name = std::string("Unnamed Component"))
			: Name(name) {}

		Component(const Component&) = default;
		virtual ~Component() = default;

		virtual void OnInit(Entity& entity) { Owner = entity; }

		void AddTag(const std::string& tag);
		void RemoveTag(const std::string& tag);

		const std::set<std::string>& GetTags() const { return m_Tags; }

	public:
		std::string Name;
		Entity Owner;

	protected:
		std::set<std::string> m_Tags;
	};

	class SceneComponent : public Component
	{
	public:
		SceneComponent() = default;
		SceneComponent(const SceneComponent& sc);
		virtual ~SceneComponent();
		virtual void OnInit(Entity& entity) override;
		
		const Transform& GetWorldTransform() const { return WorldTransform; }
		const Transform& GetRelativeTransform() const { return RelativeTransform; }

		void SetWorldTransform(const Transform& worldTransform);
		void SetRelativeTransform(const Transform& relativeTransform);

	protected:
		void OnNotify(Notification notification) override;

	protected:
		Transform WorldTransform;
		Transform RelativeTransform;

		//TODO: 
		//glm::vec3 m_Velocity;
		//bool m_Visible;
		//bool m_HiddenInGame;
	};
}
