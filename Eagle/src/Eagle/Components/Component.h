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
		Component(const Entity& entity)
			: Object(), Parent(entity)
		{
			ComponentsNotificationSystem::AddObserver(Parent, this);
		}

		Component(const Component&) = delete;
		Component(Component&&) noexcept;
		Component& operator=(const Component&);
		Component& operator=(Component&&) noexcept;
		virtual ~Component();

		// Not called if entity has been destroyed.
		virtual void OnRemoved() {}

	public:
		Entity Parent;
	};
}
