#pragma once

#include "Eagle/Core/Notifications.h"

namespace Eagle
{
	class Object : public Observer, public Subject
	{
	public:
		virtual ~Object() = default;
	};
}
