#include "egpch.h"
#include "PhysicsEngine.h"
#include "PhysXInternal.h"

namespace Eagle
{
	void PhysicsEngine::Init()
	{
		PhysXInternal::Init();
	}

	void PhysicsEngine::Shutdown()
	{
		PhysXInternal::Shutdown();
	}
}
