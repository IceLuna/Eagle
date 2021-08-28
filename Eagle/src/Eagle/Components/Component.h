#pragma once

#include "Eagle/Core/Object.h"
#include "Eagle/Core/Entity.h"
#include <set>

#define COMPONENT_DEFAULTS(x) x(const x&) = delete; x(x&&) = default; x& operator=(const x&) = default; x& operator=(x&&) = default;

namespace Eagle
{
	class Component : public Object
	{
	public:
		Component(const std::string& name = std::string("Unnamed Component"))
			: Object(), Name(name), Parent(Entity::Null) {}

		Component(const Component&) = delete;
		Component(Component&&) noexcept;
		Component& operator=(const Component&);
		Component& operator=(Component&&) noexcept;
		virtual ~Component();

		virtual void OnInit(Entity& entity);

		void AddTag(const std::string& tag);
		void RemoveTag(const std::string& tag);

		const std::set<std::string>& GetTags() const { return m_Tags; }

	public:
		std::string Name;
		Entity Parent;

	protected:
		std::set<std::string> m_Tags;
	};
}
