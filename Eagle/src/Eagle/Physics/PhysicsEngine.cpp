#include "egpch.h"
#include "PhysicsEngine.h"
#include "PhysXInternal.h"

namespace Eagle
{
	PhysicsSettings PhysicsEngine::s_Settings;

	void PhysicsEngine::Init()
	{
		PhysXInternal::Init();
	}

	void PhysicsEngine::Shutdown()
	{
		PhysXInternal::Shutdown();
	}
}
