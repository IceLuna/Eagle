#include "egpch.h"
#include "PhysicsEngine.h"
#include "PhysXInternal.h"
#include "PhysicsMaterial.h"

namespace Eagle
{
	static Ref<PhysicsMaterial> s_DefaultMaterial;

	void PhysicsEngine::Init()
	{
		PhysXInternal::Init();
		s_DefaultMaterial = PhysicsMaterial::Create();
	}

	void PhysicsEngine::Shutdown()
	{
		s_DefaultMaterial.reset();
		PhysXInternal::Shutdown();
	}

	bool PhysicsEngine::IsVisualDebuggingSupported()
	{
		return PhysXInternal::IsVisualDebuggingSupported();
	}

	const Ref<PhysicsMaterial>& PhysicsEngine::GetDefaultMaterial()
	{
		return s_DefaultMaterial;
	}
}
