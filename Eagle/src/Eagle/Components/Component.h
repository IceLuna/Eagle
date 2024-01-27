#pragma once

#include "Eagle/Core/Object.h"
#include "Eagle/Core/Entity.h"
#include <set>

#define COMPONENT_DEFAULTS(x) x(const x&) = delete; x(x&&) noexcept = default; x& operator=(const x&) = default; x& operator=(x&&) noexcept = default;

namespace Eagle
{
	class Component : public Object
	{
	public:
		Component()
			: Object(), Parent(Entity::Null) {}

		Component(const Component&) = delete;
		Component(Component&&) noexcept;
		Component& operator=(const Component&);
		Component& operator=(Component&&) noexcept;
		virtual ~Component();

		virtual void OnInit(Entity entity);
		//Not called if entity has been destroyed.
		virtual void OnRemoved(Entity entity) {}

	public:
		Entity Parent;
	};
}
