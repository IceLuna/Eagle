#pragma once

#include "Entity.h"

namespace Eagle
{
	class Event;

	class ScriptableEntity
	{
	public:
		virtual ~ScriptableEntity() = default;

		virtual void OnCreate() {}
		virtual void OnDestroy() {}
		virtual void OnUpdate(Timestep ts) {}
		virtual void OnEvent(Event& e) {}

		Entity Parent;
	};
}