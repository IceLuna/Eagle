#pragma once

#include "Eagle/Core/Notifications.h"

namespace Eagle
{
	class Object : public Observer, public Subject
	{
	public:
		Object() = default;
		Object(const Object&) = delete;
		Object(Object&&) = default;

		Object& operator=(const Object&) = delete;
		Object& operator=(Object&&) = default;
		virtual ~Object() = default;
	};
}
